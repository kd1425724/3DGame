#include "CullingManager.h"

#include "../Debug/DebugParams/DebugParams.h"
#include "../Debug/DebugFlags/DebugFlags.h"

void CullingManager::Update(const KdCamera& _camera)
{
	// Tracy計測(2026/07/19)：視錐台の更新。ここは毎フレーム1回だけの軽い処理。
	// ※ IsVisible/IsInRangeはオブジェクト毎に何百回も呼ばれる極小関数なので、
	//    計測を入れるとTracyのオーバーヘッドの方が本体より大きくなる。意図的に入れていない
	ZoneScoped;

	m_enabled  = DebugFlags::Instance().Get(U8("カリング/有効"), true);
	m_cullDist = DebugParams::Instance().Float(U8("カリング/距離"), 120.0f, 10.0f, 2000.0f);

	// カメラのワールド行列(m_mCam)。平行移動成分がカメラ位置
	const Math::Matrix& camMat = _camera.GetCameraMatrix();
	m_camPos = camMat.Translation();

	// 射影行列からビュー空間の視錐台を作り、カメラのワールド行列でワールド空間へ移す
	// (SimpleMath::MatrixはXMMATRIXへ暗黙変換されるのでそのまま渡せる)
	DirectX::BoundingFrustum::CreateFromMatrix(m_frustumWorld, _camera.GetProjMatrix());
	m_frustumWorld.Transform(m_frustumWorld, camMat);
}

bool CullingManager::IsVisible(const DirectX::BoundingSphere& _worldBS) const
{
	if (!m_enabled) { return true; }

	// 距離で切ってから視錐台判定(距離の方が軽い)
	if (!IsInRange(_worldBS)) { return false; }

	return m_frustumWorld.Intersects(_worldBS);
}

bool CullingManager::IsInRange(const DirectX::BoundingSphere& _worldBS) const
{
	if (!m_enabled) { return true; }

	// 球の半径ぶん近づけて判定する(大きい建物が距離の境界付近でも切れにくくする)
	const Math::Vector3 center(_worldBS.Center.x, _worldBS.Center.y, _worldBS.Center.z);
	const float reach = m_cullDist + _worldBS.Radius;

	return Math::Vector3::DistanceSquared(m_camPos, center) <= reach * reach;
}

DirectX::BoundingSphere CullingManager::CalcLocalBoundingSphere(const KdModelWork& _work)
{
	DirectX::BoundingSphere bs(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), 0.0f);

	const std::shared_ptr<KdModelData> spData = _work.GetData();
	if (!spData) { return bs; }

	// 描画メッシュ(COLは除外済み)の各ノードのAABBを、ノードのモデル内ワールド行列で
	// 置いてから全部マージ→そのAABBから境界球を作る
	const std::vector<KdModelData::Node>& nodes = spData->GetOriginalNodes();

	DirectX::BoundingBox total;
	bool has = false;

	for (int idx : spData->GetDrawMeshNodeIndices())
	{
		if (idx < 0 || idx >= static_cast<int>(nodes.size())) { continue; }

		const KdModelData::Node& node = nodes[idx];
		if (!node.m_spMesh) { continue; }

		DirectX::BoundingBox bb = node.m_spMesh->GetBoundingBox();	// メッシュローカル
		bb.Transform(bb, node.m_worldTransform);					// モデルローカルへ配置

		if (!has)
		{
			total = bb;
			has = true;
		}
		else
		{
			DirectX::BoundingBox::CreateMerged(total, total, bb);
		}
	}

	if (has)
	{
		DirectX::BoundingSphere::CreateFromBoundingBox(bs, total);
	}

	return bs;
}
