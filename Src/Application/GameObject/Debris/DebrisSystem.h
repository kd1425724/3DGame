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
//  【段階3の時点】見た目は仮の立方体(Block.gltf)。本物の破片モデルは段階4以降で作る。
//    ゼロ値→固定値→本物 の順で進めるため、まず挙動だけ確定させる
//
//====================================================
class DebrisSystem : public KdGameObject
{
public:

	DebrisSystem()				{}
	~DebrisSystem()	override;

	void Init()						override;
	void Update()					override;
	void DrawLit()					override;
	void GenerateDepthMapFromLight() override;

	// _center のまわりに破片を _count 個ばら撒く
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

	std::vector<Debris> m_debris;

	// 見た目のモデル(全破片で共有する)
	std::shared_ptr<KdModelWork> m_spModelWork;

	// 描画のたびに作り直すワールド行列の配列(毎回確保しないようメンバに持つ)
	std::vector<Math::Matrix> m_drawMatrices;
};
