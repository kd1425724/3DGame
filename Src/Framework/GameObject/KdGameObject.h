#pragma once

// ゲーム上に存在するすべてのオブジェクトの基底となるクラス
class KdGameObject : public std::enable_shared_from_this<KdGameObject>
{
public:

	// どのような描画を行うのかを設定するTypeID：Bitフラグで複数指定可能
	enum
	{
		eDrawTypeLit = 1 << 0,
		eDrawTypeUnLit = 1 << 1,
		eDrawTypeBright = 1 << 2,
		eDrawTypeUI = 1 << 3,
		eDrawTypeDepthOfShadow = 1 << 4,
	};

	KdGameObject() {}
	virtual ~KdGameObject() { Release(); }

	// 生成される全てに共通するパラメータに対する初期化のみ
	virtual void Init() {}

	virtual void PreUpdate() {}
	virtual void Update() {}
	virtual void PostUpdate() {}

	// それぞれの状況で描画する関数
	virtual void GenerateDepthMapFromLight() {}
	virtual void PreDraw() {}
	virtual void DrawLit() {}
	virtual void DrawUnLit() {}
	virtual void DrawEffect() {}
	virtual void DrawBright() {}
	virtual void DrawSprite() {}
	virtual void DrawDebug();

	virtual void SetAsset(const std::string&) {}

	virtual Math::Vector3 GetPos() const { return m_mWorld.Translation(); }

	const Math::Matrix& GetMatrix() const { return m_mWorld; }

	virtual bool IsExpired() const { return m_isExpired; }

	virtual bool IsVisible()	const { return false; }
	virtual bool IsRideable()	const { return false; }

	// 視錐台範囲内に入っているかどうか
	virtual bool CheckInScreen(const DirectX::BoundingFrustum&) const { return false; }

	// カメラからの距離を計算
	virtual void CalcDistSqrFromCamera(const Math::Vector3& camPos);

	float GetDistSqrFromCamera() const { return m_distSqrFromCamera; }

	UINT GetDrawType() const { return m_drawType; }

	bool Intersects(const KdCollider::SphereInfo& targetShape, std::list<KdCollider::CollisionResult>* pResults);
	bool Intersects(const KdCollider::BoxInfo& targetBox, std::list<KdCollider::CollisionResult>* pResults);
	bool Intersects(const KdCollider::RayInfo& targetShape, std::list<KdCollider::CollisionResult>* pResults);

	//publicここから下は自分で追加
	//オブジェクトの名前セット
	void SetName(std::string _name) { m_name = _name; }
	//ゲット
	std::string GetName() const{ return m_name; }
	//オブジェクトの種類
	enum class ObjectTag
	{
		None,
	};
	//各当たり判定が必要なオブジェクトでoverride
	virtual ObjectTag GetObjectTag() { return ObjectTag::None; }
	//当たった時になにをするか
	//other ... 衝突してきた相手
	virtual void OnHit(KdGameObject* _other) {}
	virtual void SetPos(const Math::Vector3& pos)
	{
		m_pos = pos;
		MatrixUpdate();
	}
	virtual void SetRot(const Math::Vector3& rot)
	{
		m_rot = rot;
		MatrixUpdate();
	}
	virtual Math::Vector3 GetRot() const
	{
		return m_rot;
	}
	virtual void SetScale(const Math::Vector3& _size)
	{
		m_scale = _size;
		MatrixUpdate();
	}
	virtual void SetScale(const float& _size)
	{
		m_scale = Math::Vector3(_size, _size, _size);
		MatrixUpdate();
	}
	Math::Vector3 GetScale()const
	{
		return m_scale;
	}
	//全ての行列更新
	void MatrixUpdate()
	{
		Math::Matrix mscale = Math::Matrix::CreateScale(m_scale);
		Math::Matrix matRot =
			Math::Matrix::CreateRotationX(DirectX::XMConvertToRadians(m_rot.x)) *
			Math::Matrix::CreateRotationY(DirectX::XMConvertToRadians(m_rot.y)) *
			Math::Matrix::CreateRotationZ(DirectX::XMConvertToRadians(m_rot.z));
		Math::Matrix mtrans = Math::Matrix::CreateTranslation(m_pos);
		m_mWorld = mscale * matRot * mtrans;
	}
	void SetHitFlg(bool flg) { m_hitflg = flg; }

protected:

	void Release() {}

	// 描画タイプ・何の描画を行うのかを決める / 最適な描画リスト作成用
	UINT m_drawType = 0;

	// カメラからの距離
	float m_distSqrFromCamera = 0;

	// 存在消滅フラグ
	bool m_isExpired = false;

	// 3D空間に存在する機能
	Math::Matrix	m_mWorld;

	// 当たり判定クラス
	std::unique_ptr<KdCollider> m_pCollider = nullptr;

	// デバッグ情報クラス
	std::unique_ptr<KdDebugWireFrame> m_pDebugWire = nullptr;

	//protectedここから下は自分で追加
	//座標
	Math::Vector3 m_pos = {};
	//サイズ
	Math::Vector3 m_scale = { 1,1,1 };
	//回転
	Math::Vector3 m_rot = {};
	//オブジェクトの名前
	std::string m_name;
	//InstanceID何回目に呼ばれたか
	int m_instanceID = -1;
	bool m_hitflg = true;
};
