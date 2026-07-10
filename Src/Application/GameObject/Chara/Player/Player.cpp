#include "Player.h"

#include "../../../main.h"
#include "../../Camera/CameraBase.h"
#include "../../Camera/TPSCamera/TPSCamera.h"
#include "../../Magic/LaserShot/LaserShot.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"

#include"../../Wire/WireAction.h"

Player::Player()
{
}

Player::~Player()
{
}

void Player::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 他のオブジェクトと見分けが付くように水色にする
	m_color = Math::Color(0.4f, 0.8f, 1.0f, 1.0f);

	// 縮小したGroundとの比率に合わせて小さくする
	SetScale(Math::Vector3(0.5f, 0.5f, 0.5f));

	//ワイヤー
	m_upWire = std::make_unique<WireAction>();
}

void Player::Update()
{
	const float dt = Application::Instance().GetDeltaTime();

	// === ワイヤーの発射 / 解除 ===
	if (KdInputManager::Instance().IsPress("WireShoot"))
	{
		// カメラのフルの向き(ピッチ込み=上も狙える)を撃つ方向にする
		Math::Vector3 dir = Math::Vector3::Backward;
		if (std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock())
		{
			dir = Math::Vector3::TransformNormal(dir, spCamera->GetRotationMatrix());
		}
		dir.Normalize();

		Math::Vector3 from = GetPos() + Math::Vector3(0, 1.0f, 0);
		float maxLen = DebugParams::Instance().Float(U8("ワイヤー/最大長"), 30.0f, 1.0f, 100.0f);

		if (m_upWire->Shoot(from, dir, maxLen))
		{
			// 撃った瞬間、通常システムの落下速度を3D速度へ引き継ぐ(急停止しないように)
			m_velocity = Math::Vector3(0.0f, m_verticalVelocity, 0.0f);
		}
	}
	if (KdInputManager::Instance().IsRelease("WireShoot"))
	{
		m_upWire->Release();
		// 縦の勢いだけ通常システムへ返す(横の勢いの引き継ぎは今後の課題)
		m_verticalVelocity = m_velocity.y;
	}

	// === ワイヤー中の物理(スイング) ===
	if (m_upWire->IsAttached())
	{
		// 重力を3D速度に加える
		float gravity = DebugParams::Instance().Float(U8("キャラ/重力"), 20.0f, 0.0f, 100.0f);
		m_velocity.y -= gravity * dt;

		// 速度で進めてから、ワイヤーの距離拘束を解く
		Math::Vector3 pos = GetPos();
		pos += m_velocity * dt;

		// W(+1)でたぐり寄せ / S(-1)で伸ばし
		float reel = KdInputManager::Instance().GetAxisState("Move").y;
		m_upWire->Update(pos, m_velocity, dt, reel);

		SetPos(pos);
		return;   // ワイヤー中は通常移動・ジャンプ・レーザーは止める
	}

	// === 通常移動(ワイヤーを使っていないとき) ===
	Math::Vector3 pos = GetPos();

	// ※ Forward/Backwardの定義上、見た目と前後が逆に感じたため入れ替え済み
	Math::Vector2 moveAxis = KdInputManager::Instance().GetAxisState("Move");
	Math::Vector3 move = Math::Vector3::Backward * moveAxis.y + Math::Vector3::Right * moveAxis.x;

	if (move.LengthSquared() > 0.0f)
	{
		move.Normalize();
		Math::Matrix rotCamMat = Math::Matrix::Identity;
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
	// ※ ワイヤー中は地面吸着させず、ワイヤー物理(3D速度)に任せる
	if (!m_upWire->IsAttached())
	{
		GroundCheck();
	}

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
