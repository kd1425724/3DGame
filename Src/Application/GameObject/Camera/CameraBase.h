#pragma once

class CameraBase : public KdGameObject
{
public:
	CameraBase()						{}
	virtual ~CameraBase()	override	{}

	void Init()				override;
	void PreDraw()			override;

	void SetTarget(const std::shared_ptr<KdGameObject>& target);

	// 「絶対変更しません！見るだけ！」な書き方
	const std::shared_ptr<KdCamera>& GetCamera() const
	{
		return m_spCamera;
	}

	// 「中身弄るかもね」な書き方
	std::shared_ptr<KdCamera> WorkCamera() const
	{
		return m_spCamera;
	}

	const Math::Matrix GetRotationMatrix()const
	{
		return Math::Matrix::CreateFromYawPitchRoll(
		       DirectX::XMConvertToRadians(m_DegAng.y),
		       DirectX::XMConvertToRadians(m_DegAng.x),
		       DirectX::XMConvertToRadians(m_DegAng.z));
	}

	const Math::Matrix GetRotationYMatrix() const
	{
		return Math::Matrix::CreateRotationY(
			   DirectX::XMConvertToRadians(m_DegAng.y));
	}

	// 【現在未使用】カメラの当たり判定を「登録したオブジェクトだけ」に当てるための登録口。
	// TPSCamera側をSceneManagerの全オブジェクト走査(TypeGround|TypeBump)に変更したため呼ばれなくなった。
	// (レベルエディタでLevel.jsonから後ロードされる塔などを取りこぼす問題への対応)
	// 登録方式に戻す可能性を考えて元の実装をコメントで残す。
	//void RegistHitObject(const std::shared_ptr<KdGameObject>& object)
	//{
	//	m_wpHitObjectList.push_back(object);
	//}

	// カメラは毎フレームm_mWorldを独自に(m_mLocalPos * m_mRotation * ターゲット行列で)組み立てているため、
	// KdGameObject::SetPos(m_pos + MatrixUpdate)を使うと回転成分が消えてしまう。
	// そのため平行移動成分だけを直接書き換える元の挙動に戻す。
	void SetPos(const Math::Vector3& pos) override
	{
		m_mWorld.Translation(pos);
	}

protected:
	// カメラ回転用角度
	Math::Vector3								m_DegAng		= Math::Vector3::Zero;

	void UpdateRotateByMouse();

	std::shared_ptr<KdCamera>					m_spCamera		= nullptr;
	std::weak_ptr<KdGameObject>					m_wpTarget;
	// 【現在未使用】RegistHitObjectで登録したカメラ当たり対象。上のRegistHitObjectと対で残す。
	//std::vector<std::weak_ptr<KdGameObject>>	m_wpHitObjectList{};

	Math::Matrix								m_mLocalPos		= Math::Matrix::Identity;
	Math::Matrix								m_mRotation		= Math::Matrix::Identity;

	// カメラ回転用マウス座標の差分
	POINT										m_FixMousePos{};
};