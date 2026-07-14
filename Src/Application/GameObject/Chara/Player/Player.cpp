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

	// ワイヤーの見た目(板ポリ)。白テクスチャを土台に、描画時に色を乗せる
	m_upWirePoly = std::make_unique<KdSquarePolygon>("Asset/Textures/System/WhiteNoise.png");
	// Lit(陰影あり)描画時に法線を光へ向ける設定。今回はUnLitで描くので実質効かない(任意)
	m_upWirePoly->Set2DObject(false);

	// 自動ターゲットのマーカー(カメラを向く板ポリ)
	m_upMarkerPoly = std::make_unique<KdSquarePolygon>("Asset/Textures/System/WhiteNoise.png");
	m_upMarkerPoly->Set2DObject(false);

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

	// 通常移動 → ジャンプ → 落下攻撃 → レーザー
	UpdateMove(dt);
	UpdateJump(dt);
	UpdateDive(dt);
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

void Player::UpdateDive(float dt)
{
	float diveSpeed = DebugParams::Instance().Float(U8("落下攻撃/降下速度"), 30.0f, 5.0f, 100.0f);
	float radius    = DebugParams::Instance().Float(U8("落下攻撃/範囲"),     3.0f, 0.5f, 15.0f);

	// === 突撃中(ホーミング) ===
	if (m_isDiving)
	{
		std::shared_ptr<KdGameObject> spTarget = m_wpDiveTarget.lock();
		if (spTarget)
		{
			// 対象へ毎フレーム向き直して直進(3D速度を上書き)。近づいたら着弾
			Math::Vector3 to = spTarget->GetPos() - GetPos();
			float dist = to.Length();
			if (dist <= radius) { DiveImpact(); return; }   // 到達＝着弾(周囲ごと撃破)
			if (dist > 0.0001f)
			{
				to /= dist;
				m_velocity = to * diveSpeed;   // 対象へ直進(ロックオン突撃)
			}
		}
		// 対象がいない(未ロック真下ダイブ or 対象消滅)は、そのまま落ちて着地時にDiveImpact
		return;
	}

	// === 開始判定 ===
	if (m_isGrounded) { return; }                                        // 空中専用
	if (!KdInputManager::Instance().IsPress("DiveAttack")) { return; }

	m_isDiving = true;

	// ロックオン対象がいればそこへ突撃、いなければ真下へ急降下
	std::shared_ptr<KdGameObject> spLock = m_wpLockOnTarget.lock();
	if (spLock)
	{
		m_wpDiveTarget = spLock;   // ホーミングの狙い先(=ロックオンした所にアンカーを刺して突撃するイメージ)
		Math::Vector3 to = spLock->GetPos() - GetPos();
		if (to.LengthSquared() > 0.0001f)
		{
			to.Normalize();
			m_velocity = to * diveSpeed;
		}
	}
	else
	{
		m_wpDiveTarget.reset();
		m_velocity.y = -diveSpeed;   // 真下(水平の勢いは残す)
	}
}

void Player::DiveImpact()
{
	// 着地の瞬間：周囲の敵をまとめて撃破し、手応えとしてカメラを揺らす
	float radius = DebugParams::Instance().Float(U8("落下攻撃/範囲"), 3.0f, 0.5f, 15.0f);

	for (auto& spEnemy : SceneManager::Instance().FindObjectsWithTag(ObjectTag::Enemy))
	{
		if (!spEnemy) { continue; }
		if (Math::Vector3::Distance(GetPos(), spEnemy->GetPos()) <= radius)
		{
			spEnemy->OnHit(this);   // 今の敵はOnHitで消滅(=一掃)
		}
	}

	// 着弾の手応え
	CameraShake::Instance().AddTrauma(0.6f);

	m_isDiving = false;
	m_wpDiveTarget.reset();
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

	// 落下攻撃中に着地したら、その瞬間に範囲撃破する
	if (m_isDiving && IsGrounded())
	{
		DiveImpact();
	}

	// === 着地・壁ヒットの手応え(カメラを揺らす) ===
	// CharaBaseが記録した衝撃をConsumeし、一定以上ならtraumaを加える(小さすぎる衝撃は無視)。
	// ※ Playerだけが発火する。Enemyも同じ記録はするが読まないのでカメラは揺れない
	float landing = ConsumeLandingImpact();
	if (landing > 3.0f) { CameraShake::Instance().AddTrauma(std::clamp(landing / 20.0f, 0.0f, 0.6f)); }

	float wall = ConsumeWallImpact();
	if (wall > 4.0f) { CameraShake::Instance().AddTrauma(std::clamp(wall / 25.0f, 0.0f, 0.7f)); }

	// 照準：画面中心に一番近い敵を自動ターゲット(カメラは回さない)
	UpdateTargeting();
}

void Player::UpdateTargeting()
{
	// カメラの向き(=画面中心の方向)に一番近い敵を選ぶ。カメラ自体は回さない。
	std::shared_ptr<CameraBase> spCam = m_wpCamera.lock();
	if (!spCam) { m_wpLockOnTarget.reset(); return; }

	// カメラの発射方向(ピッチ込み)。ワイヤー発射と同じ「フルの向き」
	Math::Vector3 camPos = spCam->GetPos();
	Math::Vector3 camFwd = Math::Vector3::TransformNormal(Math::Vector3::Backward, spCam->GetRotationMatrix());
	if (camFwd.LengthSquared() < 0.0001f) { return; }
	camFwd.Normalize();

	// 画面中心からの許容角度。これより外の敵は対象にしない
	float limitDeg = DebugParams::Instance().Float(U8("照準/有効角度"), 40.0f, 5.0f, 90.0f);
	float minDot = cosf(DirectX::XMConvertToRadians(limitDeg));

	// カメラの前方向に一番よく揃っている(=中心に近い)敵を探す
	std::shared_ptr<KdGameObject> best;
	float bestDot = minDot;   // これ未満は中心から外れすぎなので対象外
	for (auto& spEnemy : SceneManager::Instance().FindObjectsWithTag(ObjectTag::Enemy))
	{
		if (!spEnemy) { continue; }
		Math::Vector3 to = spEnemy->GetPos() - camPos;
		if (to.LengthSquared() < 0.0001f) { continue; }
		to.Normalize();
		float d = to.Dot(camFwd);   // 1に近いほど画面中心
		if (d > bestDot) { bestDot = d; best = spEnemy; }
	}

	m_wpLockOnTarget = best;   // 中心に一番近い敵(いなければ空)。落下攻撃の突撃先になる
}

void Player::DrawTargetMarker()
{
	if (!m_upMarkerPoly) { return; }

	std::shared_ptr<KdGameObject> spTarget = m_wpLockOnTarget.lock();
	if (!spTarget) { return; }

	std::shared_ptr<CameraBase> spCam = m_wpCamera.lock();
	if (!spCam) { return; }

	// マーカーサイズ
	float size = DebugParams::Instance().Float(U8("照準/マーカーサイズ"), 0.7f, 0.1f, 3.0f);
	m_upMarkerPoly->SetScale(Math::Vector2(size, size));

	// カメラの回転をそのまま姿勢に使う=常に画面へ正対(ビルボード)。敵の少し上に置く
	Math::Matrix world = spCam->GetRotationMatrix();
	world.Translation(spTarget->GetPos() + Math::Vector3(0.0f, 0.9f, 0.0f));

	// 赤いマーカーとして描く(DrawPolygonはCullNoneなので裏表は不問)
	Math::Color   col(1.0f, 0.3f, 0.2f, 1.0f);
	Math::Vector3 emissive(0.6f, 0.1f, 0.05f);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*m_upMarkerPoly, world, col, emissive);
}

void Player::DrawUnLit()
{
	// キャラのモデルは CharaBase::DrawLit が描く。ここ(陰影なしパス)では
	// ワイヤーの見た目と、自動ターゲットのマーカーを描く。
	DrawWire();
	DrawTargetMarker();
}

void Player::DrawWire()
{
	// 繋がっている間だけ描く
	if (!m_upWire || !m_upWire->IsAttached() || !m_upWirePoly) { return; }

	// ワイヤーの両端(手元→アンカー)。手元は発射位置に近い高さに合わせる
	Math::Vector3 from = GetPos() + Math::Vector3(0.0f, 0.25f, 0.0f);
	Math::Vector3 to   = m_upWire->GetAnchor();

	// 軸(=ワイヤー方向)と長さ
	Math::Vector3 axis = to - from;
	float length = axis.Length();
	if (length < 0.001f) { return; }   // 長さ0は描けない
	axis /= length;

	// 中点＆カメラへの向き(板の面をカメラに向けるビルボード用)
	Math::Vector3 mid = (from + to) * 0.5f;
	Math::Vector3 toCam = Math::Vector3::Backward;
	if (std::shared_ptr<CameraBase> spCam = m_wpCamera.lock())
	{
		toCam = spCam->GetPos() - mid;
	}
	if (toCam.LengthSquared() > 0.0001f) { toCam.Normalize(); }

	// 幅方向=軸と視線に垂直 / 法線=幅と軸に垂直(軸固定ビルボード)
	Math::Vector3 side = axis.Cross(toCam);
	if (side.LengthSquared() < 0.0001f) { side = Math::Vector3::Right; }   // 軸と視線が平行な時の保険
	side.Normalize();
	Math::Vector3 normal = side.Cross(axis);

	// 板の寸法(幅=太さ / 高さ=ワイヤー長)。太さはDebugParamsで調整
	float thickness = DebugParams::Instance().Float(U8("ワイヤー/見た目の太さ"), 0.08f, 0.01f, 1.0f);
	m_upWirePoly->SetScale(Math::Vector2(thickness, length));

	// 姿勢行列(ローカル X=side / Y=axis / Z=normal)＋中点へ配置
	Math::Matrix world = Math::Matrix::Identity;
	world.Right(side);
	world.Up(axis);
	world.Backward(normal);
	world.Translation(mid);

	// 発光っぽい水色のワイヤーとして描く(DrawPolygonはCullNoneなので裏表は気にしなくてよい)
	Math::Color   col(0.4f, 0.85f, 1.0f, 1.0f);
	Math::Vector3 emissive(0.1f, 0.4f, 0.7f);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*m_upWirePoly, world, col, emissive);
}

void Player::DrawDebug()
{
	if (m_upWire && m_upWire->IsAttached())        // 繋がっている間だけ
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
		//m_pDebugWire->AddDebugLine(m_pos, m_upWire->GetAnchor(),kBlueColor);
		m_pDebugWire->Draw();
	}
	KdGameObject::DrawDebug();
}
