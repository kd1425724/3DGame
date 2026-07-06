#include "EditorCamera.h"

#include "../../../main.h"

void EditorCamera::Init()
{
	CameraBase::Init();

	// 見下ろし気味の適当な初期位置(お好みで変更してください)
	m_mWorld = Math::Matrix::CreateTranslation(0.0f, 5.0f, -10.0f);
	m_DegAng = Math::Vector3(20.0f, 0.0f, 0.0f);
}

void EditorCamera::PostUpdate()
{
	bool rightMouseDown = KdInputManager::Instance().IsHold("RightClick");

	if (KdInputManager::Instance().IsPress("RightClick"))
	{
		// 右ボタンを押した瞬間：現在のカーソル位置を基準にして、カーソルを隠す
		// (押す前の位置を基準にすることで、離した時に元の位置へ違和感なく戻せる)
		GetCursorPos(&m_FixMousePos);
		ShowCursor(FALSE);
	}
	else if (KdInputManager::Instance().IsRelease("RightClick"))
	{
		// 右ボタンを離した瞬間：カーソルを押す前の位置へ戻して、ImGui操作を復帰させる
		SetCursorPos(m_FixMousePos.x, m_FixMousePos.y);
		ShowCursor(TRUE);
	}

	// 右ボタンを押している間だけ視点回転(CameraBaseの機能をそのまま使用)
	if (rightMouseDown)
	{
		UpdateRotateByMouse();
	}

	m_mRotation = GetRotationMatrix();

	// 現在位置を取得
	Math::Vector3 pos = m_mWorld.Translation();

	// 移動も右ボタンを押している間だけ有効(ImGuiのテキスト入力等とキーが競合しないように)
	if (rightMouseDown)
	{
		// 移動速度(Shift押しっぱなしで加速)
		float speed = m_moveSpeed;
		if (KdInputManager::Instance().IsHold("Boost"))
		{
			speed *= m_boostRate;
		}

		// カメラの向いている方向を基準にした移動
		// ※ もしW/Sが前後逆に感じる場合はBackward()をForward()に入れ替えてください
		Math::Vector3 forward = m_mRotation.Backward();
		Math::Vector3 right = m_mRotation.Right();

		Math::Vector2 moveAxis = KdInputManager::Instance().GetAxisState("Move");

		Math::Vector3 move = Math::Vector3::Zero;

		move += forward * moveAxis.y;
		move += right * moveAxis.x;
		if (KdInputManager::Instance().IsHold("MoveUp"))	{ move += Math::Vector3::Up; }
		if (KdInputManager::Instance().IsHold("MoveDown"))	{ move -= Math::Vector3::Up; }

		if (move.LengthSquared() > 0.0f)
		{
			move.Normalize();
			pos += move * speed * Application::Instance().GetDeltaTime();
		}
	}

	// 回転＋移動後の座標を合成してワールド行列を更新
	m_mWorld = m_mRotation;
	m_mWorld.Translation(pos);
}
