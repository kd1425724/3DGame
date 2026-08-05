#include "DebrisSystem.h"

#include "../../Physics/PhysicsWorld.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Debug/DebugWatch/DebugWatch.h"
#include "../../Debug/DebugFlags/DebugFlags.h"
#include "../../main.h"

// 生成コストの計測用。Pch.hには入っていないのでここで読む
#include <chrono>

namespace
{
	// 【なぜ自前の乱数か】std::rand は呼ぶ場所によって結果が変わって再現しづらく、
	//   mt19937は破片程度には大げさ。線形合同法で十分で、種を固定すれば再現もできる
	uint32_t g_randomState = 12345;

	float RandomRange(float _min, float _max)
	{
		g_randomState = g_randomState * 1664525u + 1013904223u;

		// 上位ビットのほうが偏りが少ないので、そこから0〜1を作る
		const float unit = static_cast<float>(g_randomState >> 8) / static_cast<float>(0x00FFFFFF);
		return _min + (_max - _min) * unit;
	}

	// 破片が飛ぶ向き(水平はぐるり一周、上向きは控えめ)
	Math::Vector3 RandomBurstDirection()
	{
		const float angle = RandomRange(0.0f, DirectX::XM_2PI);
		const float up    = RandomRange(0.3f, 1.0f);

		Math::Vector3 dir(std::cos(angle), up, std::sin(angle));
		dir.Normalize();
		return dir;
	}

	// 3軸それぞれに ±_spin のばらつきを持った回転
	Math::Vector3 RandomSpin(float _spin)
	{
		return Math::Vector3(
			RandomRange(-_spin, _spin),
			RandomRange(-_spin, _spin),
			RandomRange(-_spin, _spin));
	}

	// 破片の見た目に関する調整値(生成のたびに読むので、実行中に変えるとすぐ効く)
	float GetDebrisLife()   { return DebugParams::Instance().Float(U8("破片/寿命"),     8.0f, 1.0f, 60.0f); }
	float GetDebrisSpeed()  { return DebugParams::Instance().Float(U8("破片/飛ぶ速さ"), 12.0f, 0.0f, 60.0f); }
	float GetDebrisSpin()   { return DebugParams::Instance().Float(U8("破片/回る速さ"),  6.0f, 0.0f, 30.0f); }
}

DebrisSystem::~DebrisSystem()
{
	ClearAll();
}

void DebrisSystem::Init()
{
	// 調整用の立方体。部位のgibはEnemy側が必要になったときに登録する
	m_testCubeId = RegisterModel("Asset/Models/Test/Block/Block.gltf");
}

int DebrisSystem::RegisterModel(const std::string& _modelPath)
{
	for (size_t i = 0; i < m_groups.size(); ++i)
	{
		if (m_groups[i].m_modelPath == _modelPath) { return static_cast<int>(i); }
	}

	Group group;
	group.m_modelPath  = _modelPath;
	group.m_spModelWork = std::make_shared<KdModelWork>();
	group.m_spModelWork->SetModelData(KdAssets::Instance().m_modeldatas.GetData(_modelPath));

	// 【ここで凸包も作っておく】出す瞬間に作ると、全身破砕のように数十個を同じ
	//   フレームで出したとき間に合わない(実測でDebug 140ms)。破片は1種類につき
	//   1回しか出さないので「生成時にキャッシュ」では初回に効かない。登録＝読み込みの
	//   時点で払っておき、出す瞬間はボディを作るだけにする
	PhysicsWorld::Instance().PrepareDebrisConvex(*group.m_spModelWork);

	// 頂点の中心も同じタイミングで求めておく(散らす向きの計算に使う)
	group.m_center = CalcModelCenter(*group.m_spModelWork);

	m_groups.push_back(group);
	return static_cast<int>(m_groups.size() - 1);
}

Math::Vector3 DebrisSystem::CalcModelCenter(const KdModelWork& _model)
{
	const std::shared_ptr<KdModelData> spData = _model.GetData();
	if (!spData) { return Math::Vector3::Zero; }

	const std::vector<KdModelData::Node>& dataNodes = _model.GetDataNodes();
	const std::vector<KdModelWork::Node>& workNodes = _model.GetNodes();

	Math::Vector3 sum   = Math::Vector3::Zero;
	int           count = 0;

	for (int index : spData->GetDrawMeshNodeIndices())
	{
		if (index < 0) { continue; }
		if (index >= static_cast<int>(dataNodes.size())) { continue; }
		if (index >= static_cast<int>(workNodes.size())) { continue; }

		const KdMesh* pMesh = dataNodes[index].m_spMesh.get();
		if (!pMesh) { continue; }

		const Math::Matrix& mNode = workNodes[index].m_worldTransform;

		for (const Math::Vector3& local : pMesh->GetVertexPositions())
		{
			sum += Math::Vector3::Transform(local, mNode);
			++count;
		}
	}

	if (count == 0) { return Math::Vector3::Zero; }

	return sum / static_cast<float>(count);
}

Math::Vector3 DebrisSystem::GetModelCenter(int _modelId) const
{
	if (_modelId < 0) { return Math::Vector3::Zero; }
	if (_modelId >= static_cast<int>(m_groups.size())) { return Math::Vector3::Zero; }

	return m_groups[_modelId].m_center;
}

void DebrisSystem::ClearAll()
{
	for (Group& group : m_groups)
	{
		for (const Debris& debris : group.m_debris)
		{
			PhysicsWorld::Instance().RemoveBody(debris.m_bodyId);
		}
		group.m_debris.clear();
	}
}

void DebrisSystem::SpawnPiece(int _modelId, const Math::Matrix& _world,
	const Math::Vector3& _velocity, const Math::Vector3& _angularVelocity)
{
	if (_modelId < 0) { return; }
	if (_modelId >= static_cast<int>(m_groups.size())) { return; }

	Group& group = m_groups[_modelId];
	if (!group.m_spModelWork) { return; }

	// 【計測】凸包の構築は破片1個ごとに走るので、ここが全身破砕の負荷そのものになる
	const std::chrono::steady_clock::time_point spawnBegin = std::chrono::steady_clock::now();

	const uint32_t id = PhysicsWorld::Instance().SpawnDebrisConvex(
		*group.m_spModelWork, _world, _velocity, _angularVelocity);

	const std::chrono::duration<float, std::milli> spawnElapsed =
		std::chrono::steady_clock::now() - spawnBegin;
	m_spawnCostThisFrame += spawnElapsed.count();

	if (id == PhysicsWorld::kInvalidBodyId) { return; }

	// 生成時の拡大を覚えておく(物理からは位置と回転しか返ってこないため)
	Math::Vector3		scale = Math::Vector3::One;
	Math::Quaternion	rotation;
	Math::Vector3		translation;

	Math::Matrix world = _world;
	world.Decompose(scale, rotation, translation);

	Debris debris;
	debris.m_bodyId	= id;
	debris.m_life	= GetDebrisLife();
	debris.m_scale	= scale;
	debris.m_world	= _world;
	group.m_debris.push_back(debris);
}

void DebrisSystem::SpawnBurst(const Math::Vector3& _center, int _count)
{
	const float size   = DebugParams::Instance().Float(U8("破片/大きさ"),   1.0f, 0.1f, 5.0f);
	const float spread = DebugParams::Instance().Float(U8("破片/散らばり"), 1.5f, 0.0f, 10.0f);

	SpawnBurstOfModel(m_testCubeId, _center, Math::Vector3(size), spread, _count);
}

void DebrisSystem::SpawnBurstOfModel(int _modelId, const Math::Vector3& _center,
	const Math::Vector3& _scale, float _spread, int _count)
{
	if (_modelId < 0) { return; }

	const float speed = GetDebrisSpeed();
	const float spin  = GetDebrisSpin();

	for (int i = 0; i < _count; ++i)
	{
		// 同じ場所に重ねて生むと押し合って弾け飛ぶので、少しずらして置く
		const Math::Vector3 offset(
			RandomRange(-_spread, _spread),
			RandomRange(-_spread, _spread),
			RandomRange(-_spread, _spread));

		const Math::Matrix world =
			Math::Matrix::CreateScale(_scale) *
			Math::Matrix::CreateTranslation(_center + offset);

		SpawnPiece(_modelId, world,
			RandomBurstDirection() * RandomRange(speed * 0.5f, speed),
			RandomSpin(spin));
	}
}

void DebrisSystem::Update()
{
	// 【調整用】F2でカメラの前方に立方体をばら撒く。物理の手触りを詰めるために残してある
	if (KdInputManager::Instance().IsPress("SpawnDebris"))
	{
		const Math::Matrix cameraWorld = KdShaderManager::Instance().GetCameraCB().mView.Invert();

		// 【罠】SimpleMathのForward()は右手系前提で-Zを返す。このエンジンは左手系なので
		//   カメラの正面は行列の第3行(+Z軸)そのもの。ここを間違えると背後に湧く
		const Math::Vector3 forward(cameraWorld._31, cameraWorld._32, cameraWorld._33);

		const float distance = DebugParams::Instance().Float(U8("破片/湧かせる距離"), 15.0f, 1.0f, 60.0f);
		const int   count    = DebugParams::Instance().Int(U8("破片/湧かせる数"), 20, 1, 300);

		SpawnBurst(cameraWorld.Translation() + forward * distance, count);
	}

	const float deltaTime = Application::Instance().GetDeltaTime();

	// 【ここは寿命の管理だけ】描画に使う姿勢は PreDraw で取り直す(理由はそちらのコメント)。
	//   GetBodyMatrix はここでは「物理側にまだ生きているか」の確認として使っている
	for (Group& group : m_groups)
	{
		for (size_t i = 0; i < group.m_debris.size(); )
		{
			Debris& debris = group.m_debris[i];

			debris.m_life -= deltaTime;

			const bool expired = (debris.m_life <= 0.0f);
			const bool lost    = !PhysicsWorld::Instance().GetBodyMatrix(debris.m_bodyId, debris.m_world);

			if (!expired && !lost)
			{
				++i;
				continue;
			}

			PhysicsWorld::Instance().RemoveBody(debris.m_bodyId);

			// 並び順に意味は無いので、最後の要素を持ってきて縮める(消すたびに詰め直さない)
			group.m_debris[i] = group.m_debris.back();
			group.m_debris.pop_back();
		}
	}

	UpdateSpawnCostWatch();
}

void DebrisSystem::UpdateSpawnCostWatch()
{
	int aliveCount = 0;
	for (const Group& group : m_groups)
	{
		aliveCount += static_cast<int>(group.m_debris.size());
	}

	// 【なぜ持ち越すか】DebugWatchは毎フレーム書かれなかった項目を消すので、
	//   一瞬で終わる破砕の値をその場で出しても次のフレームには消えて読めない。
	//   「生成があったフレーム」の値を覚えておき、毎フレーム出し直す
	if (m_spawnCostThisFrame > 0.0f)
	{
		m_lastBurstCost      = m_spawnCostThisFrame;
		m_lastBurstCount     = aliveCount;
		m_spawnCostThisFrame = 0.0f;
	}

	DebugWatch::Instance().Watch(U8("破片/生きている数"),           aliveCount);
	DebugWatch::Instance().Watch(U8("破片/直近の生成コスト(ms)"),   m_lastBurstCost);
	DebugWatch::Instance().Watch(U8("破片/その時に出ていた数"),     m_lastBurstCount);
}

void DebrisSystem::PreDraw()
{
	// 【なぜ描画行列をここで作るか】1フレームのうち PreDraw は
	//   「PostUpdate(＝物理が進む)の後」かつ「Draw の前」に走る唯一の場所。
	//
	//   Update で作ると:  物理が進む【前】に読むので、描くのは常に1フレーム前の姿になる
	//   DrawLit で作ると: 影パス(GenerateDepthMapFromLight)が DrawLit より【前】に走るので、
	//                     影だけ1フレーム古くなるか、最初のフレームは影が出ない
	for (Group& group : m_groups)
	{
		group.m_drawMatrices.clear();
		group.m_drawMatrices.reserve(group.m_debris.size());

		for (Debris& debris : group.m_debris)
		{
			Math::Matrix bodyWorld;
			if (!PhysicsWorld::Instance().GetBodyMatrix(debris.m_bodyId, bodyWorld)) { continue; }

			// 物理は「位置と回転」しか持っていない(拡大は頂点へ焼き込んである)ので、
			// 描画用に生成時の拡大を掛け直す。これを忘れるとモデルだけ等倍で描かれる
			debris.m_world = Math::Matrix::CreateScale(debris.m_scale) * bodyWorld;

			group.m_drawMatrices.push_back(debris.m_world);
		}
	}
}

void DebrisSystem::DrawLit()
{
	for (Group& group : m_groups)
	{
		if (group.m_drawMatrices.empty()) { continue; }
		if (!group.m_spModelWork) { continue; }

		// 同じモデルなので、何個あっても「マテリアル数ぶん」のドローで済む
		KdShaderManager::Instance().m_StandardShader.DrawModelInstanced(*group.m_spModelWork, group.m_drawMatrices);
	}
}

void DebrisSystem::GenerateDepthMapFromLight()
{
	// 【切れるようにしてある理由】破片は種類ごとに1ドローなので、全身破砕の30種類が
	//   出ている間は影パスにも30ドロー増える。破片の影は小さくて見えにくい割に高いので、
	//   割に合わなければここを切れる。負荷の切り分けにも使う
	if (!DebugFlags::Instance().Get(U8("破片/影を落とす"), true)) { return; }

	for (Group& group : m_groups)
	{
		if (group.m_drawMatrices.empty()) { continue; }
		if (!group.m_spModelWork) { continue; }

		KdShaderManager::Instance().m_StandardShader.DrawModelInstanced(*group.m_spModelWork, group.m_drawMatrices);
	}
}
