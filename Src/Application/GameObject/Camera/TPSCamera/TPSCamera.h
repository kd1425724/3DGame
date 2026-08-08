#pragma once
#include "../CameraBase.h"

class TPSCamera : public CameraBase
{
public:

	TPSCamera()							{}
	~TPSCamera()			override	{}

	void Init()				override;
	void PostUpdate()		override;

	// 壁寄せのレイと、寄せた結果のカメラ位置を可視化する。
	// DebugFlagsの「デバッグ表示/カメラ」でだけ出す。常に画面の中央付近に出るので、
	// 他のデバッグ表示と一緒くたにすると邪魔になるため独立したカテゴリにしてある
	void DrawDebug()		override;

	// ロックオン中の注視点(ワールド座標)を設定する。毎フレーム呼ぶこと。
	//
	// 【なぜ「対象」ではなく「点」を渡すのか】狙っているのは敵そのものではなく
	//   敵の【関節】(首・肘×2・膝×2。ホイールで切替)で、その位置を知っているのは
	//   Player 側だけ。以前は対象オブジェクトを渡して camera 側で
	//   「原点 + 0.75m」を見ていたが、ゴーレムは身長25mで原点が足元なので
	//   足首を見ることになっていた(この経路は誰も呼んでおらず死んでいた)。
	//   点を渡す形にすれば、カメラは敵も関節も知らなくて済む。
	void SetLockOnAim(const Math::Vector3& aimPos) override
	{
		m_lockOnAimPos = aimPos;
		m_hasLockOnAim = true;
	}

	// ロックオンを解除する(以降はマウス操作だけで回る)
	void ClearLockOnAim() override { m_hasLockOnAim = false; }

	// 現在ロックオン中かどうか
	bool IsLockOn() const { return m_hasLockOnAim; }

private:

	// ロックオン中の注視点と、その有無
	Math::Vector3 m_lockOnAimPos = {};
	bool          m_hasLockOnAim = false;

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