#include "StageProp.h"

#include <filesystem>

#include "../../Editor/LevelEditor/LevelEditorManager.h"

void StageProp::Init()
{
	m_spModelWork = std::make_shared<KdModelWork>();
	m_spModelWork->SetModelData(KdAssets::Instance().m_modeldatas.GetData(m_modelPath));

	// 当たり判定：モデル形状で登録する(Block/Groundと同じ方式)。
	// ※ glTF内でノード名に "COL" を含むメッシュは、KdModelが自動で「当たり専用(描画しない)」に
	//    仕分ける。COL箱を入れておけば軽い当たり、無ければ表示メッシュ全体が当たりになる。
	// ※ BoundingBoxのレイ判定はhitPosを埋めないが、モデル形状(KdModelCollision)は埋めるので
	//    ワイヤー等のレイも正しく当たる
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape("StagePropCol", m_spModelWork, KdCollider::TypeBump);
}

void StageProp::DrawLit()
{
	if (!m_spModelWork || !m_spModelWork->IsEnable()) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_spModelWork, m_mWorld);
}

void StageProp::GenerateDepthMapFromLight()
{
	if (!m_spModelWork || !m_spModelWork->IsEnable()) { return; }

	// 深度パス用シェーダはBeginGenerateDepthMapFromLightでセット済み。モデルを描くだけで影の元になる
	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_spModelWork, m_mWorld);
}

void StageProp::DrawDebug()
{
	// 当たり判定(モデル形状=COLメッシュ or 表示メッシュ)の可視化は基底に任せる
	KdGameObject::DrawDebug();
}

void StageProp::RegisterAllToEditor()
{
	namespace fs = std::filesystem;
	LevelEditorManager& mgr = LevelEditorManager::Instance();

	// Asset/Models/Stage/ 直下のカテゴリごとに、その中の各 <名前>/<名前>.gltf を登録する
	const char* categories[] = { "House", "Building", "Prop" };

	for (const char* cat : categories)
	{
		const std::string catDir = std::string("Asset/Models/Stage/") + cat;

		std::error_code ec;
		if (!fs::is_directory(catDir, ec)) { continue; }

		for (const auto& entry : fs::directory_iterator(catDir, ec))
		{
			if (!entry.is_directory()) { continue; }

			const std::string name = entry.path().filename().string();
			const std::string gltf = catDir + "/" + name + "/" + name + ".gltf";
			if (!fs::exists(gltf, ec)) { continue; }

			// 種類名は「<カテゴリ>/<名前>」(例 "House/house01")。この名前がLevel.jsonに保存され、
			// ロード時に同名ファクトリでモデルパス付きのStagePropを再生成する
			const std::string typeName = std::string(cat) + "/" + name;

			mgr.RegisterCreatable(typeName, [gltf]()
			{
				auto obj = std::make_shared<StageProp>();
				obj->SetModelPath(gltf);
				obj->Init();
				return obj;
			});
		}
	}
}
