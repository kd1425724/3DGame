#include "Player.h"

#include "../../../main.h"
#include "../../Camera/CameraBase.h"

void Player::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 他のオブジェクトと見分けが付くように水色にする
	m_color = Math::Color(0.4f, 0.8f, 1.0f, 1.0f);
}

void Player::Update()
{
	Math::Vector3 pos = GetPos();

	Math::Vector3 move = Math::Vector3::Zero;

	// ※ Forward/Backwardの定義上、見た目と前後が逆に感じたため入れ替え済み
	if (GetAsyncKeyState('W') & 0x8000) { move += Math::Vector3::Backward; }
	if (GetAsyncKeyState('S') & 0x8000) { move += Math::Vector3::Forward; }
	if (GetAsyncKeyState('D') & 0x8000) { move += Math::Vector3::Right; }
	if (GetAsyncKeyState('A') & 0x8000) { move += Math::Vector3::Left; }

	if (move.LengthSquared() > 0.0f)
	{
		move.Normalize();

		// カメラの水平方向の向きに合わせて移動方向を回転させる(TPS的な移動)
		std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock();
		if (spCamera)
		{
			move = Math::Vector3::TransformNormal(move, spCamera->GetRotationYMatrix());
		}

		pos += move * m_moveSpeed * Application::Instance().GetDeltaTime();
		SetPos(pos);
	}

	// 地面(KdCollider::TypeGround)に立つ
	GroundCheck();
}
