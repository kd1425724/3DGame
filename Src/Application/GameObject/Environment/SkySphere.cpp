#include "SkySphere.h"

void SkySphere::Init()
{
	m_spModelWork = std::make_shared<KdModelWork>();
	m_spModelWork->SetModelData(KdAssets::Instance().m_modeldatas.GetData("Asset/Models/Environment/Sky/Sky.gltf"));

	// 単位球(半径1)を m_radius 倍に広げる。位置は Update でカメラへ合わせる
	SetScale(Math::Vector3(m_radius, m_radius, m_radius));
}

void SkySphere::Update()
{
	// カメラのワールド座標へ球の中心を合わせる。
	// これで前進しても空との距離が変わらず、無限遠にあるように見える。
	// CamPos はカメラの PreDraw で定数バッファへ書かれた値(1フレーム前の位置だが、
	// 背景の巨大な球なので1フレームのズレは見えない)
	const Math::Vector3& camPos = KdShaderManager::Instance().GetCameraCB().CamPos;
	SetPos(camPos);
}

void SkySphere::DrawUnLit()
{
	if (!m_spModelWork || !m_spModelWork->IsEnable()) { return; }

	// UnLit パスなのでライト・影・フォグが乗らない。空テクスチャがそのまま出る
	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_spModelWork, GetMatrix());
}
