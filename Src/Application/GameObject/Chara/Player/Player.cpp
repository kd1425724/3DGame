#include "Player.h"

#include "../../../main.h"
#include "../../Camera/CameraBase.h"
#include "../../Camera/TPSCamera/TPSCamera.h"
#include "../../Magic/LaserShot/LaserShot.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"

void Player::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 他のオブジェクトと見分けが付くように水色にする
	m_color = Math::Color(0.4f, 0.8f, 1.0f, 1.0f);

	// 縮小したGroundとの比率に合わせて小さくする
	SetScale(Math::Vector3(0.5f, 0.5f, 0.5f));
}

void Player::Update()
{
	Math::Vector3 pos = GetPos();

	// ※ Forward/Backwardの定義上、見た目と前後が逆に感じたため入れ替え済み
	Math::Vector2 moveAxis = KdInputManager::Instance().GetAxisState("Move");
	Math::Vector3 move = Math::Vector3::Backward * moveAxis.y + Math::Vector3::Right * moveAxis.x;

	if (move.LengthSquared() > 0.0f)
	{
		move.Normalize();

		// カメラの水平方向の向きに合わせて移動方向を回転させる(TPS的な移動)
		std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock();
		if (spCamera)
		{
			move = Math::Vector3::TransformNormal(move, spCamera->GetRotationYMatrix());
		}

		float moveSpeed = DebugParams::Instance().Float(U8("プレイヤー/移動速度"), 5.0f, 0.0f, 20.0f);
		pos += move * moveSpeed * Application::Instance().GetDeltaTime();
		SetPos(pos);
	}

	// SPACEでジャンプ(接地中のみ有効。実際の上下移動・着地はPostUpdateのGroundCheckで解決)
	if (KdInputManager::Instance().IsPress("Jump"))
	{
		Jump();
	}

	// Eキーでプレイヤーの正面にレーザー(当たり判定つき)を発射する
	if (KdInputManager::Instance().IsPress("FireLaser"))
	{
		// プレイヤーの正面方向(カメラの水平向きが基準。移動のBackward基準と合わせてある)
		Math::Vector3 front = Math::Vector3::Backward;

		std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock();
		if (spCamera)
		{
			front = Math::Vector3::TransformNormal(front, spCamera->GetRotationYMatrix());
		}

		Math::Vector3 firePos = GetPos() + Math::Vector3(0, 1.0f, 0) + front * 1.0f;

		// LaserShotを生成してシーンに追加する(エフェクト再生・寿命・当たり判定は
		// LaserShot自身が管理するので、Player側は生成して渡すだけ)
		std::shared_ptr<LaserShot> spLaser = std::make_shared<LaserShot>();
		spLaser->Fire(firePos, front);
		SceneManager::Instance().AddObject(spLaser);
	}
}

void Player::PostUpdate()
{
	// 地面(KdCollider::TypeGround)に立つ
	GroundCheck();

	// 右クリックの押した瞬間/離した瞬間でロックオンを切り替える
	std::shared_ptr<TPSCamera> spTpsCamera = std::dynamic_pointer_cast<TPSCamera>(m_wpCamera.lock());

	if (spTpsCamera)
	{
		if (KdInputManager::Instance().IsPress("RightClick"))
		{
			// 押した瞬間：一番近い敵を探してロックオンする
			std::shared_ptr<KdGameObject> nearestEnemy;
			float nearestDist = FLT_MAX;

			for (auto& spEnemy : SceneManager::Instance().FindObjectsWithTag(ObjectTag::Enemy))
			{
				float dist = Math::Vector3::Distance(GetPos(), spEnemy->GetPos());
				if (dist < nearestDist)
				{
					nearestDist = dist;
					nearestEnemy = spEnemy;
				}
			}

			if (nearestEnemy)
			{
				spTpsCamera->SetLockOnTarget(nearestEnemy);
				spTpsCamera->SetLockOn(true);
			}
		}
		else if (KdInputManager::Instance().IsRelease("RightClick"))
		{
			// 離した瞬間：ロックオン解除
			spTpsCamera->SetLockOn(false);
		}
	}
}
