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
