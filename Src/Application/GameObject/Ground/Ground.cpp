#include "Ground.h"

#include "../../Debug/DebugDraw/DebugDraw.h"
#include "../../Culling/CullingManager.h"   // CalcLocalBoundingSphere(モデル全体の境界球)

void Ground::Init()
{
	// 石畳の地面。モデルが最初から実寸(332 x 1.2 x 297m)で、UVも「1タイル=4m」で
	// 展開済みなので、ここでスケールを掛けない(SetScaleは1.0のまま)。
	// ※ 単位立方体をSetScaleで引き伸ばすと、UV1単位が覆う世界の広さも一緒に伸びて
	//    テクスチャが332mに引き伸ばされる。タイリングさせるにはモデル側で作るのが正解
	// ※ マスタは BlenderData/Ground/StonePavement/(make_ground.pyで再生成できる)
	SetAsset("Asset/Models/Environment/Ground/StonePavement.gltf");

	// 【地面の大きさについて】モデル側の実寸で決まる(ここでは変えない)。
	// ※ 影生成エリアはカメラ位置に追従する箱なので、地面の大きさとは独立。
	// ※ 2026/07/20に街を拡張(1軒をSCALE=2.0で大型化＋通りを3本に)。
	//    生成ツール(BlenderData/_tools/gen_town2.py)が出した街の範囲は
	//    X -150.0〜150.0 / Z -92.0〜132.5(145棟・最大の高さ28.2m)。余白込みで X:332 / Z:297。
	//    ※ 2026/07/20に「家の壁面揃え」へ変更した際、中心揃えで無駄になっていた奥行きの
	//       半分ぶんが詰まったのでX方向は以前(410)より狭くなった
	//    天面は y=+0.6(=1.2/2)で、Level.jsonの建物は原点が接地面なので pos.y=0.6 で乗る。
	//    ステージを広げる時は make_ground.py の SIZE_X/SIZE_Z を変えて作り直す
	//    (合わせてカリング距離/影/フォグの調整も要検討)
	//    ※ 旧: 38棟の小規模な街で X:64 / Z:130 だった

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
	// 当たり判定(モデル形状=地面の箱)を可視化する
	// ※ KdModelCollisionはAddDebugWire未対応(no-op)で枠が出ないため、ここで箱を描いて代用
	// ※ 以前は「1辺1の立方体をSetScaleで拡大」していたので半径0.5決め打ちでよかったが、
	//    実寸モデル(332 x 1.2 x 297)へ変えたのでモデルの境界から半径を求める。
	//    決め打ちのままだと緑の枠が0.5m角の点になって見えなくなる
	if (KdGameObject::s_showColliderDebug && DebugDraw::IsOn(DebugDraw::Category::Terrain) && m_spModelWork)
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}

		if (const std::shared_ptr<KdMesh> spMesh = m_spModelWork->GetMesh(0))
		{
			const DirectX::BoundingBox& bb = spMesh->GetBoundingBox();
			m_pDebugWire->AddDebugBox(m_mWorld,
				Math::Vector3(bb.Extents.x, bb.Extents.y, bb.Extents.z),
				Math::Vector3(bb.Center.x, bb.Center.y, bb.Center.z), true, kGreenColor);
		}
	}

	KdGameObject::DrawDebug();
}
