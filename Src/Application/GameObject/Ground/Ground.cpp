#include "Ground.h"

void Ground::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 地面らしく大きく・薄く潰す
	// ※ 影生成エリアは既定で25x25(KdAmbientController::Init参照)とそこまで広くなく、
	//    カメラ位置を中心に追従する仕組みなので、大きすぎると影が生成されなくなる。
	SetScale(Math::Vector3(100.0f, 1.2f, 100.0f));

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

void Ground::GenerateDepthMapFromLight()
{
	if (!m_spModelWork || !m_spModelWork->IsEnable()) { return; }

	// 深度パス用シェーダはBeginGenerateDepthMapFromLightでセット済み。モデルを描くだけで影の元になる
	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_spModelWork, m_mWorld);
}

void Ground::DrawDebug()
{
	// 当たり判定(モデル=1辺1の立方体をスケールした地面)を箱で可視化する
	// ※ KdModelCollisionはAddDebugWire未対応(no-op)で枠が出ないため、ここで箱を描いて代用
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugBox(m_mWorld, Math::Vector3(0.5f, 0.5f, 0.5f), Math::Vector3::Zero, true, kGreenColor);
	}

	KdGameObject::DrawDebug();
}
