#include "Ground.h"

void Ground::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 地面らしく大きく・薄く潰す
	// ※ 影生成エリアは既定で25x25(KdAmbientController::Init参照)とそこまで広くなく、
	//    カメラ位置を中心に追従する仕組みなので、大きすぎると影が生成されなくなる。
	SetScale(Math::Vector3(6.0f, 0.2f, 6.0f));

	//当てられる側の処理＝＝＝＝＝＝＝＝＝＝
	//当たり判定をつけたいから実体化
	m_pCollider = std::make_unique<KdCollider>();
	//モデルの形状で当たり判定を登録
	m_pCollider->RegisterCollisionShape(
		"GroundCollision",			//識別名の名前
		m_spModelWork,				//登録したいモデルの形状
		KdCollider::TypeGround		//当たり判定の種類
	);
}

void Ground::SetAsset(const std::string& assetName)
{
	m_spModelWork = std::make_shared<KdModelWork>();
	m_spModelWork->SetModelData(KdAssets::Instance().m_modeldatas.GetData(assetName));
}

void Ground::DrawLit()
{
	if (!m_spModelWork || !m_spModelWork->IsEnable()) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_spModelWork, m_mWorld);
}
