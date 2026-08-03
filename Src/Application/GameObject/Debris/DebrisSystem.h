#pragma once

//====================================================
//
// DebrisSystem ── 破片を一括で持って、まとめて描くオブジェクト
//
//  ・動きは PhysicsWorld(Jolt)が計算する。ここは「物理の姿勢を描画行列にして描く」係
//  ・破片1個ずつをKdGameObjectにはしない。数百個になるとUpdate/Drawの呼び出しだけで
//    重くなるため、InstancedPropRendererと同じく【1オブジェクトがまとめて描く】形にする
//    → 同じモデルなら DrawModelInstanced で1ドローで済む
//  ・シーンに1つ常駐させる(GameScene::Initで追加)
//
//  【モデルごとにグループを分ける理由】DrawModelInstanced は「同じモデル」しか
//    まとめられない。前腕と脛を一緒には描けないので、モデル単位で束ねる
//
//====================================================
class DebrisSystem : public KdGameObject
{
public:

	DebrisSystem()				{}
	~DebrisSystem()	override;

	void Init()						override;
	void Update()					override;
	void PreDraw()					override;
	void DrawLit()					override;
	void GenerateDepthMapFromLight() override;

	//----------------------------------------
	// 破片を出す口
	//----------------------------------------
	// モデルを登録して、以後そのIDで破片を出せるようにする。
	// 同じパスを2回渡しても同じIDを返す(読み込みは1回だけ)
	int RegisterModel(const std::string& _modelPath);

	// 登録済みモデルの破片を1つ、指定の姿勢で出す。
	// _world は拡大が入っていてよい(敵は等倍でないため)
	void SpawnPiece(int _modelId, const Math::Matrix& _world,
		const Math::Vector3& _velocity, const Math::Vector3& _angularVelocity);

	// 立方体の破片を _center のまわりに _count 個ばら撒く(見た目の調整用)
	void SpawnBurst(const Math::Vector3& _center, int _count);

private:

	// 生きている破片を消す(シーン終了時など)
	void ClearAll();

	// 破片1つぶん
	struct Debris
	{
		uint32_t		m_bodyId	= 0;	// PhysicsWorld側のID
		float			m_life		= 0.0f;	// 残り寿命(秒)
		Math::Matrix	m_world;			// 物理から受け取った姿勢(描画に使う)
	};

	// 同じモデルの破片をまとめたもの
	struct Group
	{
		std::string						m_modelPath;
		std::shared_ptr<KdModelWork>	m_spModelWork;
		std::vector<Debris>				m_debris;

		// 描画のたびに作り直すワールド行列の配列(毎回確保しないよう持ち回す)
		std::vector<Math::Matrix>		m_drawMatrices;
	};

	std::vector<Group> m_groups;

	// 立方体(調整用)のモデルID。Init()で登録する
	int m_testCubeId = -1;
};
