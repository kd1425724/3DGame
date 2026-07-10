#include "Block.h"

void Block::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

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
