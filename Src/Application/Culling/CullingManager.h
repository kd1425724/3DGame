#pragma once

//====================================================
//
// CullingManager ── 視錐台＋距離カリング(描画/影の間引き)
//
//  ・毎フレーム描画前に CameraBase::PreDraw から Update(カメラ) を呼び、
//    ワールド空間の視錐台とカメラ位置・設定を更新する
//  ・StageProp/Block など多数の静的オブジェクトが、自分のワールド境界球で
//      IsVisible() … 通常描画用(視錐台内 かつ 距離内)
//      IsInRange() … 影(深度マップ)用(距離のみ。画面外の物でも影は画面に落ちうるので視錐台では切らない)
//    を問い合わせて、外れていれば描画を早期returnする
//  ・ON/OFFは DebugFlags「カリング/有効」、距離は DebugParams「カリング/距離」で調整
//  ・エンジン非改造。KdMeshが持つ境界球(GetBoundingSphere)からモデル全体の球を作って使う
//
//====================================================
class KdCamera;
class KdModelWork;

class CullingManager
{
public:

	static CullingManager& Instance()
	{
		static CullingManager instance;
		return instance;
	}

	// 毎フレーム、描画前にカメラから視錐台・カメラ位置・設定を更新する
	void Update(const KdCamera& _camera);

	// 通常描画してよいか(視錐台内 かつ 距離内)。カリング無効時は常にtrue
	bool IsVisible(const DirectX::BoundingSphere& _worldBS) const;

	// 影を落としてよいか(距離のみで判定)。カリング無効時は常にtrue
	bool IsInRange(const DirectX::BoundingSphere& _worldBS) const;

	// モデル全体のローカル境界球を計算する(Init時などに1回。ワールド変換して上の判定へ渡す)
	static DirectX::BoundingSphere CalcLocalBoundingSphere(const KdModelWork& _work);

private:

	CullingManager() = default;
	~CullingManager() = default;
	CullingManager(const CullingManager&) = delete;
	void operator=(const CullingManager&) = delete;

	DirectX::BoundingFrustum	m_frustumWorld;			// ワールド空間の視錐台(Updateで更新)
	Math::Vector3				m_camPos	= {};		// カメラ位置(距離判定用)
	float						m_cullDist	= 120.0f;	// この距離より遠い(球の最近点が)物は描かない
	bool						m_enabled	= true;		// カリング全体のON/OFF
};
