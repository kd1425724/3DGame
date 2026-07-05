#include "EditorCamera.h"

void EditorCamera::Init()
{
	CameraBase::Init();

	// 見下ろし気味の適当な初期位置(お好みで変更してください)
	m_mWorld = Math::Matrix::CreateTranslation(0.0f, 5.0f, -10.0f);
	m_DegAng = Math::Vector3(20.0f, 0.0f, 0.0f);
}

void EditorCamera::PostUpdate()
{
	// マウスで視点回転(CameraBaseの機能をそのまま使用)
	UpdateRotateByMouse();
	m_mRotation = GetRotationMatrix();

	// 現在位置を取得
	Math::Vector3 pos = m_mWorld.Translation();

	// 移動速度(Shift押しっぱなしで加速)
	float speed = m_moveSpeed;
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
	{
		speed *= m_boostRate;
	}

	// カメラの向いている方向を基準にした移動
	// ※ もしW/Sが前後逆に感じる場合はBackward()をForward()に入れ替えてください
	Math::Vector3 forward = m_mRotation.Backward();
	Math::Vector3 right = m_mRotation.Right();

	Math::Vector3 move = Math::Vector3::Zero;

	if (GetAsyncKeyState('W') & 0x8000) { move += forward; }
	if (GetAsyncKeyState('S') & 0x8000) { move -= forward; }
	if (GetAsyncKeyState('D') & 0x8000) { move += right; }
	if (GetAsyncKeyState('A') & 0x8000) { move -= right; }
	if (GetAsyncKeyState('E') & 0x8000) { move += Math::Vector3::Up; }
	if (GetAsyncKeyState('Q') & 0x8000) { move -= Math::Vector3::Up; }

	if (move.LengthSquared() > 0.0f)
	{
		move.Normalize();
		pos += move * speed * Application::Instance().GetDeltaTime();
	}

	// 回転＋移動後の座標を合成してワールド行列を更新
	m_mWorld = m_mRotation;
	m_mWorld.Translation(pos);
}
