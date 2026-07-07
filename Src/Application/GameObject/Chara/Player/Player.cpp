#include "Player.h"

#include "../../../main.h"
#include "../../Camera/CameraBase.h"
#include "../../Camera/TPSCamera/TPSCamera.h"
#include "../Enemy/Enemy.h"
#include "../../../Scene/SceneManager.h"

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

		pos += move * m_moveSpeed * Application::Instance().GetDeltaTime();
		SetPos(pos);
	}

	// Eキーでプレイヤーの正面にBlueLaserエフェクトを発射する
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

		std::weak_ptr<KdEffekseerObject> wpEfk = KdEffekseerManager::GetInstance().Play("Magic/BlueLaser/BlueLaser.efk", firePos);

		// 発射位置だけでなく向きも正面に合わせる
		std::shared_ptr<KdEffekseerObject> spEfk = wpEfk.lock();
		if (spEfk)
		{
			spEfk->SetWorldMatrix(Math::Matrix::CreateWorld(firePos, front, Math::Vector3::Up));

			// 一定時間(m_laserStopTime秒)で強制停止できるよう発射リストに登録する
			m_firedLasers.push_back({ wpEfk, 0.0f });
		}
	}

	// 発射済みレーザーの経過時間を進め、時間が来たものを止める
	UpdateFiredLasers();
}

void Player::UpdateFiredLasers()
{
	float deltaTime = Application::Instance().GetDeltaTime();

	for (auto itr = m_firedLasers.begin(); itr != m_firedLasers.end(); )
	{
		std::shared_ptr<KdEffekseerObject> spEfk = itr->effect.lock();

		// 既に破棄されている(自然に消えた)ものはリストから除外するだけ
		if (!spEfk)
		{
			itr = m_firedLasers.erase(itr);
			continue;
		}

		itr->elapsed += deltaTime;

		// 規定時間を超えたら強制停止して除外する
		if (itr->elapsed >= m_laserStopTime)
		{
			spEfk->StopEffect();
			itr = m_firedLasers.erase(itr);
		}
		else
		{
			++itr;
		}
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
			std::shared_ptr<Enemy> nearestEnemy;
			float nearestDist = FLT_MAX;

			for (auto& obj : SceneManager::Instance().GetObjList())
			{
				std::shared_ptr<Enemy> spEnemy = std::dynamic_pointer_cast<Enemy>(obj);
				if (!spEnemy) { continue; }

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
