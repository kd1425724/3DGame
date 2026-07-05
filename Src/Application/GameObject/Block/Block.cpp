#include "Block.h"

void Block::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 当たり判定登録(Block.gltfのおおよそのサイズに合わせた箱。半径0.5 = 1辺1の立方体)
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape(
		"BlockCollision",
		DirectX::BoundingBox(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f)),
		KdCollider::TypeBump
	);
}

void Block::SetAsset(const std::string& assetName)
{
	m_modelWork.SetModelData(KdAssets::Instance().m_modeldatas.GetData(assetName));
}

void Block::DrawLit()
{
	if (!m_modelWork.IsEnable()) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}
