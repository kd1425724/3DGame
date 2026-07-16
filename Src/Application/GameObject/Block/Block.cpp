#include "Block.h"

void Block::Init()
{
	// 【一時プレビュー】house01の見た目/スケール/多メッシュ描画を確認するため、Blockのモデルを
	// 家に差し替えている。確認後は下のBlock.gltfに戻すこと(またはStageProp化する)
	//SetAsset("Asset/Models/Test/Block/Block.gltf");
	SetAsset("Asset/Models/Stage/House/house01.gltf");

	// 当たり判定登録：モデルの形状そのものを登録する(Groundと同じ方式)
	// ※ BoundingBoxのレイ判定はhitPosを埋めないため、ワイヤー等のレイが正しく当たらない。
	//    モデル形状(KdModelCollision)はhitPosを埋めるので、こちらを使う
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape("BlockCollision", m_spModelWork, KdCollider::TypeBump);
}

void Block::SetAsset(const std::string& assetName)
{
	m_spModelWork = std::make_shared<KdModelWork>();
	m_spModelWork->SetModelData(KdAssets::Instance().m_modeldatas.GetData(assetName));
}

void Block::DrawLit()
{
	if (!m_spModelWork || !m_spModelWork->IsEnable()) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_spModelWork, m_mWorld);
}

void Block::GenerateDepthMapFromLight()
{
	if (!m_spModelWork || !m_spModelWork->IsEnable()) { return; }

	// 深度パス用シェーダはBeginGenerateDepthMapFromLightでセット済み。モデルを描くだけで影の元になる
	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_spModelWork, m_mWorld);
}

void Block::DrawDebug()
{
	// 当たり判定(モデル=1辺1の立方体)を箱で可視化する
	// ※ KdModelCollisionはAddDebugWire未対応(no-op)で枠が出ないため、ここで箱を描いて代用。
	//    m_mWorldに回転・スケールが入っているのでoriented=trueで実形状に一致させる
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
		m_pDebugWire->AddDebugBox(m_mWorld, Math::Vector3(0.5f, 0.5f, 0.5f), Math::Vector3::Zero, true, kGreenColor);
	}

	KdGameObject::DrawDebug();
}
