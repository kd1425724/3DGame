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

	// 登録済みモデルの頂点の中心(モデル座標)。
	// 【何に使うか】全身破砕で破片を外向きに散らすとき、その破片が体のどのあたりに
	//   あるかが要る。骨の位置で代用すると、同じ骨の破片が全部同じ向きへ飛んで束になる
	Math::Vector3 GetModelCenter(int _modelId) const;

	// 登録済みモデルの破片を1つ、指定の姿勢で出す。
	// _world は拡大が入っていてよい(敵は等倍でないため)
	void SpawnPiece(int _modelId, const Math::Matrix& _world,
		const Math::Vector3& _velocity, const Math::Vector3& _angularVelocity);

	// 立方体の破片を _center のまわりに _count 個ばら撒く(見た目の調整用)
	void SpawnBurst(const Math::Vector3& _center, int _count);

	// 登録済みモデルの破片を _center のまわりに _count 個ばら撒く。
	// _scale … モデルに掛ける拡大(gibは骨ローカル＝モデル座標で作ってあるので、
	//           身長25mのゴーレムに合わせるならキャラの拡大を渡す)
	// _spread … ばら撒く範囲(m)。キャラが大きいほど広げないと1点に固まって弾け飛ぶ
	void SpawnBurstOfModel(int _modelId, const Math::Vector3& _center,
		const Math::Vector3& _scale, float _spread, int _count);

private:

	// 生きている破片を消す(シーン終了時など)
	void ClearAll();

	// 生成コストと破片の数をDebugWatchへ出す(Updateの末尾で毎フレーム呼ぶ)
	void UpdateSpawnCostWatch();

	// モデルの頂点の中心(モデル座標)を求める。登録時に1回だけ呼ぶ
	static Math::Vector3 CalcModelCenter(const KdModelWork& _model);

	// 破片1つぶん
	struct Debris
	{
		uint32_t		m_bodyId	= 0;	// PhysicsWorld側のID
		float			m_life		= 0.0f;	// 残り寿命(秒)

		// 【なぜ拡大を覚えておくか】物理側は拡大を頂点へ焼き込んであるので、
		//   物理から返ってくるのは「位置と回転」だけ。描画には拡大が要るので
		//   生成時の値をここに持っておき、描画行列を作るとき掛け直す
		Math::Vector3	m_scale		= Math::Vector3::One;

		Math::Matrix	m_world;			// 描画に使う姿勢(拡大込み)
	};

	// 同じモデルの破片をまとめたもの
	struct Group
	{
		std::string						m_modelPath;
		std::shared_ptr<KdModelWork>	m_spModelWork;
		std::vector<Debris>				m_debris;

		// 頂点の中心(モデル座標)。登録時に1回だけ求める
		Math::Vector3					m_center = Math::Vector3::Zero;

		// 描画のたびに作り直すワールド行列の配列(毎回確保しないよう持ち回す)
		std::vector<Math::Matrix>		m_drawMatrices;
	};

	std::vector<Group> m_groups;

	// 立方体(調整用)のモデルID。Init()で登録する
	int m_testCubeId = -1;

	//----------------------------------------
	// 生成コストの計測(全身破砕で30個を1フレームに出すため)
	//----------------------------------------
	// 【なぜ測るか】PhysicsWorld::SpawnDebrisConvex は破片1個ごとに
	//   全頂点の走査→間引き→凸包の構築をやり直している。部位gibの5個なら問題にならないが、
	//   全身破砕で数十個を同じフレームに出すと引っかかる可能性がある。
	//   アセットを作る前に実測しておき、要るならモデル登録時のキャッシュへ変える
	float m_spawnCostThisFrame = 0.0f;	// このフレームで生成に使った合計(ミリ秒)
	float m_lastBurstCost      = 0.0f;	// 直近「生成があったフレーム」の合計(ミリ秒)
	int   m_lastBurstCount     = 0;		// そのフレームで出した個数
};
