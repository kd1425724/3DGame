#include "PhysicsWorld.h"

// 【順序の制約】Jolt.h は他のJoltヘッダより先に読むこと(Jolt公式の要求)
#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <thread>

// 【using namespace JPH を書かない理由】JoltにはColor/Vec3/Mat44など、
//   このプロジェクトの Math 名前空間やDirectX側と名前がぶつかりやすい型が多い。
//   全部 JPH:: で明示したほうが読む側も迷わない
namespace
{
	//----------------------------------------
	// 物理世界の容量
	//----------------------------------------
	// 破片に加えて、街や地面の静的形状も同じ枠を使うので多めに取る。
	// 足りなくなると「これ以上ボディを追加できない」で静かに失敗するので、
	// 削るのは実測してからにすること
	constexpr JPH::uint kMaxBodies				= 65536;
	constexpr JPH::uint kNumBodyMutexes			= 0;		// 0 = Joltの既定値に任せる
	constexpr JPH::uint kMaxBodyPairs			= 65536;
	constexpr JPH::uint kMaxContactConstraints	= 10240;

	// 物理更新の一時確保用。毎フレームのmalloc/freeを避けるために先に確保しておく
	constexpr JPH::uint kTempAllocatorBytes		= 10 * 1024 * 1024;

	//----------------------------------------
	// 衝突レイヤー
	//----------------------------------------
	// 「動かないもの同士」は当たり判定を取る必要がないので、レイヤーで分けて除外する。
	// これをやらないと街の建物同士を延々と判定し続けることになる
	namespace Layers
	{
		static constexpr JPH::ObjectLayer NON_MOVING	= 0;	// 地面・建物
		static constexpr JPH::ObjectLayer MOVING		= 1;	// 破片
		static constexpr JPH::ObjectLayer NUM_LAYERS	= 2;
	}

	// ブロードフェーズ(大まかな絞り込み)のレイヤー。
	// レイヤーごとに別々のツリーになるので、静的なものと動くものを分けると
	// 「動かないツリーを毎フレーム作り直さずに済む」
	namespace BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr JPH::uint NUM_LAYERS(2);
	}

	// オブジェクトレイヤー → ブロードフェーズレイヤーの対応表
	class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:

		BroadPhaseLayerInterfaceImpl()
		{
			m_objectToBroadPhase[Layers::NON_MOVING]	= BroadPhaseLayers::NON_MOVING;
			m_objectToBroadPhase[Layers::MOVING]		= BroadPhaseLayers::MOVING;
		}

		JPH::uint GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}

		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer _layer) const override
		{
			JPH_ASSERT(_layer < Layers::NUM_LAYERS);
			return m_objectToBroadPhase[_layer];
		}

		// 【なぜこの#ifが要るか】JPH_PROFILE_ENABLED付きでビルドしたときだけ
		//   基底クラスにこの純粋仮想関数が生える。定義がずれるとリンクエラーになる
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer _layer) const override
		{
			switch (static_cast<JPH::BroadPhaseLayer::Type>(_layer))
			{
			case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING):
				return "NON_MOVING";
			case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):
				return "MOVING";
			default:
				return "INVALID";
			}
		}
#endif

	private:

		JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
	};

	// オブジェクトレイヤー同士が当たるか
	class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
	{
	public:

		bool ShouldCollide(JPH::ObjectLayer _object1, JPH::ObjectLayer _object2) const override
		{
			switch (_object1)
			{
			case Layers::NON_MOVING:
				// 動かないもの同士は判定しない(街の建物同士など)
				return _object2 == Layers::MOVING;
			case Layers::MOVING:
				return true;
			default:
				return false;
			}
		}
	};

	// オブジェクトレイヤーとブロードフェーズレイヤーが当たるか
	class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:

		bool ShouldCollide(JPH::ObjectLayer _layer1, JPH::BroadPhaseLayer _layer2) const override
		{
			switch (_layer1)
			{
			case Layers::NON_MOVING:
				return _layer2 == BroadPhaseLayers::MOVING;
			case Layers::MOVING:
				return true;
			default:
				return false;
			}
		}
	};

	// Joltが内部で出すメッセージの行き先。VisualStudioの出力ウィンドウへ流す
	void TraceImpl(const char* _format, ...)
	{
		va_list args;
		va_start(args, _format);
		char buffer[1024]{};
		vsnprintf(buffer, sizeof(buffer), _format, args);
		va_end(args);

		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
	}

	// モデルの当たり判定メッシュから【モデル座標系のままの】形状を作る。
	// ワールド変換を含めないので、同じモデルを何棟置いてもこれ1つで足りる。
	// ※ Jolt型を返すのでヘッダには出せない。ここに置く
	JPH::RefConst<JPH::Shape> BuildStaticShape(const KdModelWork& _model);

	// 物理に回すワーカースレッドの本数
	int GetWorkerThreadCount()
	{
		// 【なぜ-1か】ゲームループが1本使っているので、その分を空ける。
		//   hardware_concurrencyは0を返すことが規格上ありうるので下限1で守る
		const int hardware = static_cast<int>(std::thread::hardware_concurrency());
		return (std::max)(1, hardware - 1);
	}
}

//====================================================
// Joltの実体(ヘッダへ出さないためにここへ隠す)
//====================================================
struct PhysicsWorld::Impl
{
	Impl()
		: m_tempAllocator(kTempAllocatorBytes)
		, m_jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, GetWorkerThreadCount())
	{
	}

	JPH::TempAllocatorImpl				m_tempAllocator;
	JPH::JobSystemThreadPool			m_jobSystem;

	// 【寿命の注意】PhysicsSystemはこの3つを【参照で】保持する。
	//   PhysicsSystemより先に壊れると落ちるので、必ず同じ構造体に持たせて寿命を揃える
	BroadPhaseLayerInterfaceImpl		m_broadPhaseLayerInterface;
	ObjectVsBroadPhaseLayerFilterImpl	m_objectVsBroadPhaseFilter;
	ObjectLayerPairFilterImpl			m_objectLayerPairFilter;

	JPH::PhysicsSystem					m_physicsSystem;

	// 登録済みの静的ボディ(シーンを作り直すとき一括で捨てるために覚えておく)
	std::vector<JPH::BodyID>			m_staticBodies;

	// モデルごとの形状キャッシュ。同じ家を100軒置いても形状は1つで済む
	// (InstancedPropRendererが描画でやっているのと同じ考え方)。
	// KdModelDataはKdAssetsが持ち続けるので、生ポインタをキーにしてよい
	std::unordered_map<const KdModelData*, JPH::RefConst<JPH::Shape>> m_shapeCache;
};

// 【なぜここで定義するか】Implが完全型として見えるのはこの.cppだけ。
// ヘッダ側で = default にすると不完全型のまま破棄コードが要求されて落ちる
PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::~PhysicsWorld()
{
	// Application::Release()から呼ばれ損ねた場合の保険。
	// 既に解放済みならRelease()側が何もしない
	Release();
}

void PhysicsWorld::Init()
{
	if (m_pImpl) { return; }

	// 【順番が決まっている】アロケータ → Factory → 型登録 の順。
	// RegisterTypes()はFactoryへ型を登録するので、Factoryが先に無いと落ちる
	JPH::RegisterDefaultAllocator();

	JPH::Trace = TraceImpl;

	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	m_pImpl = std::make_unique<Impl>();

	m_pImpl->m_physicsSystem.Init(
		kMaxBodies,
		kNumBodyMutexes,
		kMaxBodyPairs,
		kMaxContactConstraints,
		m_pImpl->m_broadPhaseLayerInterface,
		m_pImpl->m_objectVsBroadPhaseFilter,
		m_pImpl->m_objectLayerPairFilter);
}

void PhysicsWorld::Update(float _deltaTime)
{
	if (!m_pImpl) { return; }

	// 【第2引数】1フレームを何回に分けて解くか。
	// 大きくするほど安定するが重くなる。破片程度なら1で足りる
	constexpr int kCollisionSteps = 1;

	m_pImpl->m_physicsSystem.Update(_deltaTime, kCollisionSteps, &m_pImpl->m_tempAllocator, &m_pImpl->m_jobSystem);
}

void PhysicsWorld::Release()
{
	if (!m_pImpl) { return; }

	// PhysicsSystemを先に壊してから型登録を解除する(逆にすると解放中に型情報を引きに行く)
	m_pImpl.reset();

	JPH::UnregisterTypes();

	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}

bool PhysicsWorld::AddStaticMesh(const KdModelWork& _model, const Math::Matrix& _world)
{
	if (!m_pImpl) { return false; }

	const std::shared_ptr<KdModelData> spData = _model.GetData();
	if (!spData) { return false; }

	// --- 形状はモデルごとに1つだけ作って使い回す ---
	// 【なぜ共有するか】街は738棟だが元のモデルは数十種しかない。1棟ごとに形状を
	//   作ると同じ木を何百回も建て直すことになる(実測で725ms掛かっていた)
	JPH::RefConst<JPH::Shape> shape;

	const auto cached = m_pImpl->m_shapeCache.find(spData.get());
	if (cached != m_pImpl->m_shapeCache.end())
	{
		shape = cached->second;
	}
	else
	{
		shape = BuildStaticShape(_model);
		if (shape == nullptr) { return false; }

		m_pImpl->m_shapeCache[spData.get()] = shape;
	}

	// --- 置き方(位置・回転・拡大)はボディ側で持つ ---
	// 【なぜ焼き込まないか】頂点にワールド行列を焼き込むと形状を共有できなくなる
	Math::Vector3		scale;
	Math::Quaternion	rotation;
	Math::Vector3		translation;

	// 【罠】SimpleMathのDecomposeは非constなので、const参照のままでは呼べない。コピーを取る
	Math::Matrix world = _world;
	if (!world.Decompose(scale, rotation, translation)) { return false; }

	// 拡大が1でなければ包んで拡大する(街の建物はスケール2.0で置かれている)
	constexpr float kScaleEpsilon = 0.0001f;
	if (std::abs(scale.x - 1.0f) > kScaleEpsilon
		|| std::abs(scale.y - 1.0f) > kScaleEpsilon
		|| std::abs(scale.z - 1.0f) > kScaleEpsilon)
	{
		JPH::ScaledShapeSettings scaledSettings(shape, JPH::Vec3(scale.x, scale.y, scale.z));
		scaledSettings.SetEmbedded();

		JPH::ShapeSettings::ShapeResult scaledResult = scaledSettings.Create();
		if (scaledResult.HasError()) { return false; }

		shape = scaledResult.Get();
	}

	JPH::BodyCreationSettings bodySettings(
		shape,
		JPH::RVec3(translation.x, translation.y, translation.z),
		JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
		JPH::EMotionType::Static,
		Layers::NON_MOVING);

	const JPH::BodyID id = m_pImpl->m_physicsSystem.GetBodyInterface()
		.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);

	m_pImpl->m_staticBodies.push_back(id);

	return true;
}

namespace
{

JPH::RefConst<JPH::Shape> BuildStaticShape(const KdModelWork& _model)
{
	const std::shared_ptr<KdModelData> spData = _model.GetData();
	if (!spData) { return nullptr; }

	const std::vector<KdModelData::Node>& dataNodes = _model.GetDataNodes();
	const std::vector<KdModelWork::Node>& workNodes = _model.GetNodes();

	JPH::VertexList			vertices;
	JPH::IndexedTriangleList	triangles;

	// 【KdColliderと同じ選び方にする】当たり判定用メッシュノードだけを使う。
	//   ここを変えると「ゲームが当たると思っている地面」と「物理の地面」がずれる
	for (int index : spData->GetCollisionMeshNodeIndices())
	{
		if (index < 0) { continue; }
		if (index >= static_cast<int>(dataNodes.size())) { continue; }
		if (index >= static_cast<int>(workNodes.size())) { continue; }

		const KdMesh* pMesh = dataNodes[index].m_spMesh.get();
		if (!pMesh) { continue; }

		// ノードのモデル内行列だけを掛ける(オブジェクトのワールド行列はボディ側で持つ)
		const Math::Matrix& mNode = workNodes[index].m_worldTransform;

		const JPH::uint32 base = static_cast<JPH::uint32>(vertices.size());

		for (const Math::Vector3& local : pMesh->GetVertexPositions())
		{
			const Math::Vector3 posInModel = Math::Vector3::Transform(local, mNode);
			vertices.push_back(JPH::Float3(posInModel.x, posInModel.y, posInModel.z));
		}

		// 【三角形の向きはそのままでよい】Joltは巻き順で表裏を決めるので逆だとすり抜ける。
		//   KdGLTFLoaderはZを反転すると同時にインデックスの1と2を入れ替えている
		//   (KdGLTFLoader.cpp:655「Z軸ミラーのため、1と2を入れ替えています」)ので、
		//   エンジン側のデータは cross(v1-v0, v2-v0) が外向き＝Joltの期待と一致する
		for (const KdMeshFace& face : pMesh->GetFaces())
		{
			triangles.push_back(JPH::IndexedTriangle(
				base + face.Idx[0],
				base + face.Idx[1],
				base + face.Idx[2]));
		}
	}

	if (triangles.empty()) { return nullptr; }

	JPH::MeshShapeSettings shapeSettings(std::move(vertices), std::move(triangles));
	shapeSettings.SetEmbedded();

	JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
	if (shapeResult.HasError()) { return nullptr; }

	return shapeResult.Get();
}

}	// namespace

void PhysicsWorld::ClearStaticBodies()
{
	if (!m_pImpl) { return; }

	JPH::BodyInterface& bodyInterface = m_pImpl->m_physicsSystem.GetBodyInterface();

	for (const JPH::BodyID& id : m_pImpl->m_staticBodies)
	{
		bodyInterface.RemoveBody(id);
		bodyInterface.DestroyBody(id);
	}

	m_pImpl->m_staticBodies.clear();

	// 形状キャッシュは残す。モデル自体はKdAssetsが持ち続けるので、
	// シーンを作り直しても同じ形状をそのまま使い回せる
}

void PhysicsWorld::FinishStaticSetup()
{
	if (!m_pImpl) { return; }

	m_pImpl->m_physicsSystem.OptimizeBroadPhase();
}
