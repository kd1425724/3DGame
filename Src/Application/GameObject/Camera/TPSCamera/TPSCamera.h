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

	// === スイング酔い対策のスムージング用の内部状態(すべてDebugParamsで強さ調整) ===
	// 初回フレームだけ現在値に合わせる(起動直後のスムージング暴れ防止)
	bool          m_smoothInit     = false;
	// A: 遅れて追従するカメラ注視点(プレイヤーへLerpで寄せる)
	Math::Vector3 m_smoothFollowPos = {};
	// B: 遅れて追従する視点回転(度。ロックオンのスナップやマウスの急変を和らげる)
	Math::Vector3 m_smoothDegAng    = {};
	// C: 対象の移動速度算出用の前フレーム位置
	Math::Vector3 m_prevTargetPos   = {};
	// C: 速度に応じてカメラを後ろへ引く量(平滑化後)
	float         m_smoothPullback  = 0.0f;
	// F: 速度に応じて広げるFOV(度。平滑化後)
	float         m_smoothFov       = 60.0f;

	// カメラのローカル位置の基準オフセット(Initで設定し、PostUpdateで使う)。
	// x=横ずらし(プレイヤーを画面端に寄せる肩越し量。+でカメラを右へ→プレイヤーは画面左寄りになる)、
	// y=注視の高さ、z=引き距離。PostUpdateはこのzへ速度ぶんの引き(m_smoothPullback)を足す。
	Math::Vector3 m_localBaseOffset = { 0.0f, 0.75f, -4.0f };
};