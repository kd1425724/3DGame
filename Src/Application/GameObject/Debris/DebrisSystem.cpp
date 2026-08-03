#include "DebrisSystem.h"

#include "../../Physics/PhysicsWorld.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../main.h"

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
}

DebrisSystem::~DebrisSystem()
{
	ClearAll();
}

void DebrisSystem::Init()
{
	// 【仮のモデル】段階3では挙動を確定させたいだけなので、既存のテスト用立方体を使う。
	// 本物の破片モデル(ゴーレムを砕いたもの)は段階4以降でBlenderから持ってくる
	m_spModelWork = std::make_shared<KdModelWork>();
	m_spModelWork->SetModelData(KdAssets::Instance().m_modeldatas.GetData("Asset/Models/Test/Block/Block.gltf"));
}

void DebrisSystem::ClearAll()
{
	for (const Debris& debris : m_debris)
	{
		PhysicsWorld::Instance().RemoveBody(debris.m_bodyId);
	}

	m_debris.clear();
}

void DebrisSystem::SpawnBurst(const Math::Vector3& _center, int _count)
{
	const float size	= DebugParams::Instance().Float(U8("破片/大きさ"), 1.0f, 0.1f, 5.0f);
	const float speed	= DebugParams::Instance().Float(U8("破片/飛ぶ速さ"), 12.0f, 0.0f, 60.0f);
	const float spin	= DebugParams::Instance().Float(U8("破片/回る速さ"), 6.0f, 0.0f, 30.0f);
	const float life	= DebugParams::Instance().Float(U8("破片/寿命"), 8.0f, 1.0f, 60.0f);
	const float spread	= DebugParams::Instance().Float(U8("破片/散らばり"), 1.5f, 0.0f, 10.0f);

	const Math::Vector3 halfExtent(size * 0.5f, size * 0.5f, size * 0.5f);

	for (int i = 0; i < _count; ++i)
	{
		// 同じ場所に重ねて生むと押し合って弾け飛ぶので、少しずらして置く
		const Math::Vector3 offset(
			RandomRange(-spread, spread),
			RandomRange(-spread, spread),
			RandomRange(-spread, spread));

		const Math::Vector3 velocity = RandomBurstDirection() * RandomRange(speed * 0.5f, speed);

		const Math::Vector3 angularVelocity(
			RandomRange(-spin, spin),
			RandomRange(-spin, spin),
			RandomRange(-spin, spin));

		const uint32_t id = PhysicsWorld::Instance().SpawnDebrisBox(
			_center + offset, halfExtent, velocity, angularVelocity);

		if (id == PhysicsWorld::kInvalidBodyId) { continue; }

		Debris debris;
		debris.m_bodyId	= id;
		debris.m_life	= life;
		m_debris.push_back(debris);
	}
}

void DebrisSystem::Update()
{
	// 【段階3の確認用】F2でカメラの前方に破片をばら撒く。
	// 段階4で「敵の関節を壊したとき」に置き換えるが、それまでの調整用として残しておく
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

	// 【なぜここで姿勢を取るか】物理はPostUpdateで進むので、次のフレームのUpdateで
	//   受け取れば「1フレーム前の姿勢」ではなく最新のものが取れる
	for (size_t i = 0; i < m_debris.size(); )
	{
		Debris& debris = m_debris[i];

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
		m_debris[i] = m_debris.back();
		m_debris.pop_back();
	}

	// 【なぜUpdateで作るか】影パス(GenerateDepthMapFromLight)は通常描画より【前】に走る。
	//   DrawLitで作ると、影のほうが1フレーム古い姿勢を使うか、初回は空になってしまう
	m_drawMatrices.clear();
	m_drawMatrices.reserve(m_debris.size());

	for (const Debris& debris : m_debris)
	{
		m_drawMatrices.push_back(debris.m_world);
	}
}

void DebrisSystem::DrawLit()
{
	if (m_drawMatrices.empty()) { return; }
	if (!m_spModelWork) { return; }

	// 同じモデルなので、何個あっても「マテリアル数ぶん」のドローで済む
	KdShaderManager::Instance().m_StandardShader.DrawModelInstanced(*m_spModelWork, m_drawMatrices);
}

void DebrisSystem::GenerateDepthMapFromLight()
{
	if (m_drawMatrices.empty()) { return; }
	if (!m_spModelWork) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawModelInstanced(*m_spModelWork, m_drawMatrices);
}
