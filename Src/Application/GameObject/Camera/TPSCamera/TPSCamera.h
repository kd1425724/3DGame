#pragma once
#include "../CameraBase.h"

class TPSCamera : public CameraBase
{
public:
	TPSCamera()							{}
	~TPSCamera()			override	{}

	void Init()				override;
	void PostUpdate()		override;

	// ロックオン状態を設定する(falseにするとロックオン対象も解除される)
	void SetLockOn(bool isLockOn)
	{
		m_isLockOn = isLockOn;

		if (!isLockOn)
		{
			m_wpLockOnTarget.reset();
		}
	}

	// 現在ロックオン中かどうか
	bool IsLockOn() const { return m_isLockOn; }

	// ロックオン対象を設定する
	// ※ 追従対象(m_wpTarget、プレイヤー)はそのままで、向きの計算にだけ使う
	void SetLockOnTarget(const std::shared_ptr<KdGameObject>& target)
	{
		m_wpLockOnTarget = target;
	}

private:

	// ロックオン中かどうか
	bool m_isLockOn = false;

	// ロックオン対象(向きの計算専用。追従対象とは別)
	std::weak_ptr<KdGameObject> m_wpLockOnTarget;
};