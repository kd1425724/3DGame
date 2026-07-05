#include "Player.h"

#include "../../../main.h"

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

	if (GetAsyncKeyState('W') & 0x8000) { move += Math::Vector3::Forward; }
	if (GetAsyncKeyState('S') & 0x8000) { move += Math::Vector3::Backward; }
	if (GetAsyncKeyState('D') & 0x8000) { move += Math::Vector3::Right; }
	if (GetAsyncKeyState('A') & 0x8000) { move += Math::Vector3::Left; }

	if (move.LengthSquared() > 0.0f)
	{
		move.Normalize();
		pos += move * m_moveSpeed * Application::Instance().GetDeltaTime();
		SetPos(pos);
	}

	// 地面(KdCollider::TypeGround)に立つ
	GroundCheck();
}
