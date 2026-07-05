#include "Block.h"

void Block::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");
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
