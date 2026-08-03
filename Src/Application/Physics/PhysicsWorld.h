#pragma once

//====================================================
//
// PhysicsWorld ── Jolt Physics の物理世界を1つだけ持つシングルトン
//
//  ・用途は【破片の落下・転がり】に限定する。
//    プレイヤー・敵・ワイヤーは今までどおり KdCollider 側で処理する
//    (手作りの操作感を物理エンジンに渡すと、坂で滑る・段差で引っかかる等で台無しになるため。
//     業界でもキャラの移動は物理剛体にせず Kinematic Character Controller にするのが標準)
//
//  ・【ヘッダでJoltを読まない】Joltのヘッダは重く、Jolt.hを最初に読む順序制約もある。
//    Joltの型はすべて .cpp 側の Impl に隠してあるので、ここを読む側は何も知らなくてよい
//
//  ・【Init()をコンストラクタから呼ばない】シングルトンのstatic初期化中に
//    Instance()へ再入するとデッドロックする。Application::Init()から明示的に1回呼ぶこと
//
//====================================================
class PhysicsWorld
{
public:

	static PhysicsWorld& Instance()
	{
		static PhysicsWorld instance;
		return instance;
	}

	// 物理世界を作る。Application::Init()から1回だけ呼ぶ
	void Init();

	// 物理世界を1フレーム進める
	void Update(float _deltaTime);

	// 物理世界を壊す。Application::Release()から呼ぶ
	void Release();

	// Init()が済んでいるか
	bool IsReady() const { return m_pImpl != nullptr; }

	// 静的な地形(地面・建物)を物理世界へ登録する。【動かないものだけ】を入れること。
	//
	// KdColliderと同じ「当たり判定用メッシュノード」を使うので、ゲーム側の当たり判定と
	// 物理側の形がずれない。
	// 形状はモデルごとにキャッシュして共有し、位置・回転・拡大はボディ側が持つ
	// (同じ家を100軒置いても形状は1つで済む)。静的なので登録後は動かせない
	//
	// _model … 形の元になるモデル / _world … そのオブジェクトのワールド行列
	// 戻り値 … 登録できたか(当たり判定メッシュが1つも無ければfalse)
	bool AddStaticMesh(const KdModelWork& _model, const Math::Matrix& _world);

	// 登録済みの静的形状をすべて捨てる。シーンを作り直す前に呼ぶこと。
	// 【なぜ要るか】呼ばないとシーンを切り替えるたびに街が二重三重に積み上がる
	void ClearStaticBodies();

	// 静的な地形を入れ終わったら1回だけ呼ぶ。ブロードフェーズを最適化する。
	// 【なぜ分けてあるか】この処理は登録済みの静的形状ぜんぶを見て木を作り直すので、
	//   AddStaticMeshのたびに呼ぶと 棟数の2乗 の手間になる(街は738棟ある)
	void FinishStaticSetup();

	//----------------------------------------
	// 動く物体(破片)
	//----------------------------------------
	// 【IDがuint32な理由】JoltのBodyIDをヘッダへ出さないため。中身は
	//   JPH::BodyID::GetIndexAndSequenceNumber() と同じ値で、往復して復元できる
	static constexpr uint32_t kInvalidBodyId = 0xFFFFFFFF;

	// 箱の破片を1つ生む。_halfExtent は「半分の大きさ」(0.5で1m角)。
	// 戻り値は kInvalidBodyId なら失敗
	uint32_t SpawnDebrisBox(const Math::Vector3& _pos, const Math::Vector3& _halfExtent,
		const Math::Vector3& _velocity, const Math::Vector3& _angularVelocity);

	// モデルの形から凸包を作って破片を1つ生む(箱より本物に近い当たりになる)。
	// 【なぜ凸包か】動く剛体に三角形メッシュは使えない(Joltが許さない/重い)。
	//   凹んだ形は再現できないが、もげた腕や脚なら見た目との差はほぼ分からない
	//
	// _model    … 形の元(gibのモデル)。当たり判定メッシュが無ければ描画メッシュを使う
	// _world    … 生成時の姿勢。拡大が入っていてよい(頂点側に焼き込む)
	// 戻り値 … kInvalidBodyId なら失敗
	uint32_t SpawnDebrisConvex(const KdModelWork& _model, const Math::Matrix& _world,
		const Math::Vector3& _velocity, const Math::Vector3& _angularVelocity);

	// 物体の今の姿勢を描画用のワールド行列として取り出す
	bool GetBodyMatrix(uint32_t _id, Math::Matrix& _outWorld) const;

	// 物体を消す
	void RemoveBody(uint32_t _id);

private:

	// 【罠】コンストラクタもデストラクタも【必ず.cpp側で定義する】。
	//   ヘッダで = default にすると、このヘッダを読んだ別の.cpp(main.cppなど)で
	//   実体化された時に unique_ptr<Impl> の破棄コードが必要になり、
	//   そこではImplが不完全型なので "can't delete an incomplete type" で落ちる。
	//   pimplを使うときの定番の落とし穴
	PhysicsWorld();
	~PhysicsWorld();

	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;

	// Joltの型をヘッダへ出さないための隠し場所(実体は.cpp)。
	// unique_ptrなのでデストラクタは.cpp側で定義する必要がある
	struct Impl;
	std::unique_ptr<Impl> m_pImpl;
};
