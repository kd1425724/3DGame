#include "Player.h"

#include "../../../main.h"
#include "../../Camera/CameraBase.h"
#include "../../Camera/TPSCamera/TPSCamera.h"
#include "../../Magic/LaserShot/LaserShot.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugFlags/DebugFlags.h"
#include "../../Camera/CameraShake.h"

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

	m_pDebugWire = std::make_unique<KdDebugWireFrame>();
}

void Player::Update()
{
	const float dt = Application::Instance().GetDeltaTime();

	// リセット：Rキー、または一定Y以下に落ちたら開始位置へ復帰
	float fallResetY = DebugParams::Instance().Float(U8("プレイヤー/落下リセットY"), -20.0f, -200.0f, 0.0f);
	if (KdInputManager::Instance().IsPress("Respawn") || GetPos().y < fallResetY)
	{
		Respawn();
		return;   // この行以降(移動・ワイヤー等)はスキップ
	}

	// ワイヤーの発射/解除
	UpdateWireInput();

	// ワイヤー接続中はスイング物理だけ行い、通常移動・ジャンプ・レーザーは止める
	if (m_upWire->IsAttached())
	{
		UpdateWireSwing(dt);
		return;
	}

	// 通常移動 → ジャンプ → レーザー
	UpdateMove(dt);
	UpdateJump(dt);
	UpdateLaser();
}

void Player::UpdateWireInput()
{
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

		// 撃った瞬間、今の速度(m_velocity)はそのまま引き継ぐ
		// (走りながら撃てば横の勢いが乗る。速度は基底CharaBaseの共通m_velocity)
		m_upWire->Shoot(from, dir, maxLen);
	}
	if (KdInputManager::Instance().IsRelease("WireShoot"))
	{
		// 離しても速度(m_velocity)はそのまま=スイングの勢いで飛んでいける(フリング)
		m_upWire->Release();
	}
}

void Player::UpdateWireSwing(float dt)
{
	// === ワイヤー中の物理(スイング) ===
	// 重力を3D速度に加える
	float gravity = DebugParams::Instance().Float(U8("キャラ/重力"), 20.0f, 0.0f, 100.0f);
	m_velocity.y -= gravity * dt;

	// 速度で進めてから、ワイヤーの距離拘束を解く
	Math::Vector3 pos = GetPos();
	Math::Vector3 startPos = pos;   // 移動前の位置(スイープの始点)
	pos += m_velocity * dt;

	// W(+1)でたぐり寄せ / S(-1)で伸ばし
	float reel = KdInputManager::Instance().GetAxisState("Move").y;
	m_upWire->Update(pos, m_velocity, dt, reel);

	// === 漕ぎ(ポンプ) ===
	// DebugFlags「ワイヤー/漕ぎ(ポンプ)」でON/OFF。
	//   OFF … 元の純粋な振り子(下りで加速→上りで減速。地面すれすれは速度が落ちる)
	//   ON  … 振り子に接線方向の加速を足す(ブランコを漕ぐイメージ)。上限速度まで勢いを
	//          維持できるので、Spider-Man風に地面すれすれでも速度が落ちにくくなる
	if (DebugFlags::Instance().Get(U8("ワイヤー/漕ぎ(ポンプ)"), false))
	{
		// 今の進行方向(水平)
		Math::Vector3 horiz(m_velocity.x, 0.0f, m_velocity.z);
		float sp = horiz.Length();
		if (sp > 0.0001f)
		{
			Math::Vector3 tdir = horiz / sp;

			// アンカーから外向き(半径方向)の水平成分を除き、接線方向だけに加速する。
			// (半径方向へ足しても距離拘束で打ち消されるだけ＝弧に沿ってのみ漕ぐ)
			Math::Vector3 radial = pos - m_upWire->GetAnchor();
			radial.y = 0.0f;
			if (radial.LengthSquared() > 0.0001f)
			{
				radial.Normalize();
				tdir -= radial * tdir.Dot(radial);
				if (tdir.LengthSquared() > 0.0001f) { tdir.Normalize(); }
				else                                { tdir = Math::Vector3::Zero; }
			}

			float pumpMax = DebugParams::Instance().Float(U8("ワイヤー/ポンプ上限速度"), 20.0f, 0.0f, 60.0f);
			float pumpAcc = DebugParams::Instance().Float(U8("ワイヤー/ポンプ加速"),     15.0f, 0.0f, 100.0f);

			float add = pumpMax - sp;   // 上限速度まであとどれだけ足せるか(それ以上は加速しない)
			if (add > 0.0f && tdir.LengthSquared() > 0.0f)
			{
				float a = pumpAcc * dt;
				if (a > add) { a = add; }
				m_velocity.x += tdir.x * a;
				m_velocity.z += tdir.z * a;
			}
		}
	}

	// 地面に潜らないよう押し上げる(ワイヤー中もすり抜け防止)
	// ※ m_isGroundedもここで更新される。空中を振っている間は接地false=離した時のフリングが残る
	ResolveGround(pos);

	// スイングは高速なので、まず壁を飛び越えるトンネリングを止める(startPos→posを掃引)
	ResolveBumpSweep(startPos, pos);

	// 塔(Block)にめり込まないよう水平に押し出す(スイングで塔に突っ込んでもすり抜けない)
	ResolveBump(pos);

	SetPos(pos);
}

void Player::UpdateMove(float dt)
{
	// === 通常移動(velocityベース。接地=キビキビ、空中=勢いを保つ) ===
	// ※ Forward/Backwardの定義上、見た目と前後が逆に感じたため入れ替え済み
	Math::Vector2 moveAxis = KdInputManager::Instance().GetAxisState("Move");
	Math::Vector3 wishDir = Math::Vector3::Backward * moveAxis.y + Math::Vector3::Right * moveAxis.x;

	float moveSpeed = DebugParams::Instance().Float(U8("プレイヤー/移動速度"), 5.0f, 0.0f, 20.0f);
	Math::Vector3 wishVel = Math::Vector3::Zero;
	if (wishDir.LengthSquared() > 0.0f)
	{
		wishDir.Normalize();
		// カメラの水平方向の向きに合わせて移動方向を回転させる(TPS的な移動)
		if (std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock())
		{
			wishDir = Math::Vector3::TransformNormal(wishDir, spCamera->GetRotationYMatrix());
		}
		wishVel = wishDir * moveSpeed;
	}

	if (m_isGrounded)
	{
		// 接地中はキビキビ動くよう、水平速度を入力に即セット(離せば止まる)
		m_velocity.x = wishVel.x;
		m_velocity.z = wishVel.z;
	}
	else if (wishDir.LengthSquared() > 0.0f)
	{
		// 空中制御(Quake風エアアクセル)：入力方向へ加速はするが、既にある勢いは削らない
		// ・入力方向の速度成分が moveSpeed に達するまでだけ加速する(それ以上は足さない)
		// ・横入力なら進行方向を曲げられる(速度は落ちない=速いフリングにブレーキがかからない)
		float airAccel = DebugParams::Instance().Float(U8("プレイヤー/空中制御"), 10.0f, 0.0f, 100.0f);

		Math::Vector3 horiz(m_velocity.x, 0.0f, m_velocity.z);
		float speedInWishDir = horiz.Dot(wishDir);      // 今の速度のうち入力方向を向いている分
		float addSpeed = moveSpeed - speedInWishDir;     // moveSpeedまであとどれだけ足せるか
		if (addSpeed > 0.0f)
		{
			float accel = airAccel * moveSpeed * dt;
			if (accel > addSpeed) { accel = addSpeed; }  // 入力方向がmoveSpeedを超えないよう頭打ち
			m_velocity.x += wishDir.x * accel;
			m_velocity.z += wishDir.z * accel;
		}
	}

	// 実際の移動・着地はPostUpdateのGroundCheckがm_velocityを積分して解決する
}

void Player::UpdateJump(float dt)
{
	// コヨーテタイム：接地を離れた直後の少しの間はまだ跳べる(崖際の取りこぼし対策)
	// 先行入力：着地寸前に押した入力を覚えておき、着地した瞬間に跳ぶ
	float coyoteTime = DebugParams::Instance().Float(U8("プレイヤー/コヨーテ時間"), 0.12f, 0.0f, 0.5f);
	float bufferTime = DebugParams::Instance().Float(U8("プレイヤー/ジャンプ先行入力"), 0.12f, 0.0f, 0.5f);

	// タイマー更新：接地中はコヨーテを0に、空中では増やす
	if (m_isGrounded) { m_coyoteTimer = 0.0f; } else { m_coyoteTimer += dt; }

	// 入力があればバッファを満タンに、なければ減らす
	if (KdInputManager::Instance().IsPress("Jump")) { m_jumpBufferTimer = bufferTime; }
	else { m_jumpBufferTimer -= dt; if (m_jumpBufferTimer < 0.0f) { m_jumpBufferTimer = 0.0f; } }

	// 「先行入力が生きている」かつ「接地中 or コヨーテ猶予内」なら跳ぶ
	bool canJump = (m_jumpBufferTimer > 0.0f) && (m_isGrounded || m_coyoteTimer <= coyoteTime);
	if (canJump)
	{
		DoJump();
		m_jumpBufferTimer = 0.0f;    // 入力を消費(1回で1ジャンプ)
		m_coyoteTimer = 999.0f;      // コヨーテも消費して空中で連続ジャンプしないようにする
	}
}

void Player::UpdateLaser()
{
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

void Player::Respawn()
{
	// 開始位置へ戻し、勢い・接地・ワイヤーをすべてリセットする
	SetPos(m_spawnPos);
	m_velocity = Math::Vector3::Zero;
	m_isGrounded = false;
	if (m_upWire) { m_upWire->Release(); }
}

void Player::PostUpdate()
{
	// 地面(KdCollider::TypeGround)に立つ
	// ※ ワイヤー中は地面吸着させず、ワイヤー物理(3D速度)に任せる
	if (!m_upWire->IsAttached())
	{
		GroundCheck();
	}

	// === 着地・壁ヒットの手応え(カメラを揺らす) ===
	// CharaBaseが記録した衝撃をConsumeし、一定以上ならtraumaを加える(小さすぎる衝撃は無視)。
	// ※ Playerだけが発火する。Enemyも同じ記録はするが読まないのでカメラは揺れない
	float landing = ConsumeLandingImpact();
	if (landing > 3.0f) { CameraShake::Instance().AddTrauma(std::clamp(landing / 20.0f, 0.0f, 0.6f)); }

	float wall = ConsumeWallImpact();
	if (wall > 4.0f) { CameraShake::Instance().AddTrauma(std::clamp(wall / 25.0f, 0.0f, 0.7f)); }

	// ロックオンの切り替え
	UpdateLockOn();
}

void Player::UpdateLockOn()
{
	// ロックオンの切り替え(右クリックはワイヤー発射に使うので、ロックオンは別入力"LockOn")
	std::shared_ptr<TPSCamera> spTpsCamera = std::dynamic_pointer_cast<TPSCamera>(m_wpCamera.lock());

	if (spTpsCamera)
	{
		if (KdInputManager::Instance().IsPress("LockOn"))
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
		else if (KdInputManager::Instance().IsRelease("LockOn"))
		{
			// 離した瞬間：ロックオン解除
			spTpsCamera->SetLockOn(false);
		}
	}
}

void Player::DrawDebug()
{
	if (m_upWire && m_upWire->IsAttached())        // 繋がっている間だけ
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
		m_pDebugWire->AddDebugLine(m_pos, m_upWire->GetAnchor(),kBlueColor);
		m_pDebugWire->Draw();
	}
	KdGameObject::DrawDebug();
}
