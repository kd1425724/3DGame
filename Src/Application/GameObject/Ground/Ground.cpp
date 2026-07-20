#include "Ground.h"

#include "../../Culling/CullingManager.h"   // CalcLocalBoundingSphere(モデル全体の境界球)

void Ground::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 地面らしく大きく・薄く潰す
	// ※ 影生成エリアは既定で25x25(KdAmbientController::Init参照)とそこまで広くなく、
	//    カメラ位置を中心に追従する仕組みなので、大きすぎると影が生成されなくなる。
	// ※ 2026/07/20に街を拡張(1軒をSCALE=2.0で大型化＋通りを3本に)。
	//    生成ツール(BlenderData/_tools/gen_town2.py)が出した街の範囲は
	//    X -150.0〜150.0 / Z -92.0〜132.5(145棟・最大の高さ28.2m)。
	//    余白込みで X:332 / Z:297 とする。
	//    ※ 2026/07/20に「家の壁面揃え」へ変更した際、中心揃えで無駄になっていた奥行きの
	//       半分ぶんが詰まったのでX方向は以前(410)より狭くなった
	//    天面は y=+0.6(=1.2/2)で、Level.jsonの建物は原点が接地面なので pos.y=0.6 で乗る。
	//    ステージを広げる時はここを大きくする(合わせてカリング距離/影/フォグの調整も要検討)
	//    ※ 旧: 38棟の小規模な街で X:64 / Z:130 だった
	// 【2026/07/20 実験】街を半分スケールで生成した場合の地面サイズ(gen_town2.pyの出力より)。
	// 元は 332 x 297(SCALE=2.0の街)。戻す時はこの数値へ
	SetScale(Math::Vector3(97.0f, 1.2f, 149.0f));

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

DirectX::BoundingSphere Ground::GetColliderBounds() const
{
	// モデル全体のローカル境界球をワールド変換して返す。地面は巨大なので、CollisionGrid側では
	// 「多数のセルにまたがる=常時候補」として毎クエリ必ず候補に含まれる(=常に接地判定される)
	DirectX::BoundingSphere bs(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), 0.0f);
	if (!m_spModelWork) { return bs; }

	const DirectX::BoundingSphere local = CullingManager::CalcLocalBoundingSphere(*m_spModelWork);
	local.Transform(bs, m_mWorld);
	return bs;
}

void Ground::DrawLit()
{
	if (!m_spModelWork || !m_spModelWork->IsEnable()) { return; }

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
