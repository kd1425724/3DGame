#include "Player.h"

#include "../../../main.h"
#include "../../Camera/CameraBase.h"
#include "../../Camera/TPSCamera/TPSCamera.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugFlags/DebugFlags.h"
#include "../../Camera/CameraShake.h"
#include "../../../Effect/EffectManager.h"
#include "../../Targeting/Targeting.h"

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

	//ワイヤー(物理＋見た目を内包。見た目の板ポリ生成はWireActionのctorが行う)
	m_upWire = std::make_unique<WireAction>();

	// 照準(画面中心の敵を自動ロックオン＋マーカー描画。マーカー板ポリ生成はTargetingのctorが行う)
	m_upTargeting = std::make_unique<Targeting>();

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

	// 空中スロー(左クリック長押しで時間をスロー＋狙う)。毎フレームtimeScaleを管理する
	UpdateAirFocus();

	// ワイヤーの発射/解除(入力・狙いはPlayer側。スイング物理はWireActionに委譲)
	UpdateWireInput();

	// ワイヤー接続中はスイング物理だけ行い、通常移動・ジャンプ・レーザーは止める。
	// たぐり寄せ入力(W/S=Move.y)を渡し、実際の移動はWireAction::UpdateSwingがこのキャラを動かす
	if (m_upWire->IsAttached())
	{
		m_upWire->UpdateSwing(*this, dt, KdInputManager::Instance().GetAxisState("Move").y);
		return;
	}

	// 回避ダッシュ(クールダウン消化＋実行中は速度を上書き)。ダッシュ中は他の行動を止める
	UpdateDodge(dt);
	if (m_isDodging) { return; }

	// 落下攻撃(突撃/連続攻撃)中は通常移動・ジャンプを止める。
	// ※ UpdateMoveは接地中に水平速度を入力値(無入力なら0)へ上書きするため、
	//   突撃中に走ると継続受付中の流しや突撃の勢いが地面で殺されてしまう
	if (!m_isDiving)
	{
		UpdateMove(dt);
		UpdateJump(dt);
	}
	UpdateDive(dt);
	UpdateSweep(dt);   // Eキーのスキル「振り回し一掃」
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

void Player::UpdateDodge(float dt)
{
	// クールダウンを消化
	if (m_dodgeCooldownTimer > 0.0f) { m_dodgeCooldownTimer -= dt; }

	// === 回避ダッシュ実行中：水平にフラットに素早く移動(縦は止めて空中でもキレよく) ===
	if (m_isDodging)
	{
		float dodgeSpeed = DebugParams::Instance().Float(U8("回避/速度"), 22.0f, 5.0f, 60.0f);
		m_velocity.x = m_dodgeDir.x * dodgeSpeed;
		m_velocity.z = m_dodgeDir.z * dodgeSpeed;
		m_velocity.y = 0.0f;

		m_dodgeTimer -= dt;
		if (m_invincibleTimer > 0.0f) { m_invincibleTimer -= dt; }
		if (m_dodgeTimer <= 0.0f) { m_isDodging = false; }
		return;
	}

	// === 開始判定 ===
	if (m_isDiving) { return; }                                      // 突撃中は回避しない
	if (m_dodgeCooldownTimer > 0.0f) { return; }                     // クールダウン中
	if (!KdInputManager::Instance().IsPress("Dodge")) { return; }

	// 方向：移動入力があればその向き(カメラ基準)、無ければカメラ前方(水平)
	Math::Vector2 moveAxis = KdInputManager::Instance().GetAxisState("Move");
	Math::Vector3 dir = Math::Vector3::Backward * moveAxis.y + Math::Vector3::Right * moveAxis.x;
	if (dir.LengthSquared() < 0.0001f) { dir = Math::Vector3::Backward; }   // 入力なし→前方へ
	if (std::shared_ptr<CameraBase> spCam = m_wpCamera.lock())
	{
		dir = Math::Vector3::TransformNormal(dir, spCam->GetRotationYMatrix());
	}
	dir.y = 0.0f;
	if (dir.LengthSquared() < 0.0001f) { dir = Math::Vector3::Backward; }
	dir.Normalize();
	m_dodgeDir = dir;

	// 開始：無敵とクールダウンを張り、実行時間だけダッシュする
	m_isDodging          = true;
	m_dodgeTimer         = DebugParams::Instance().Float(U8("回避/時間"),         0.18f, 0.05f, 1.0f);
	m_dodgeCooldownTimer = DebugParams::Instance().Float(U8("回避/クールダウン"), 0.5f,  0.0f,  3.0f);
	m_invincibleTimer    = DebugParams::Instance().Float(U8("回避/無敵時間"),     0.2f,  0.0f,  1.0f);
}

void Player::UpdateAirFocus()
{
	float maxGauge = DebugParams::Instance().Float(U8("空中スロー/最大時間"), 1.5f, 0.2f, 5.0f);
	float slowVal  = DebugParams::Instance().Float(U8("空中スロー/遅さ"),     0.3f, 0.05f, 1.0f);
	float refill   = DebugParams::Instance().Float(U8("空中スロー/回復速度"), 0.5f, 0.0f, 3.0f);

	// フォーカスゲージは"現実の時間"で増減させる(スローで薄まらないように実時間dtを使う)
	float realDt = Application::Instance().GetRealDeltaTime();

	// スロー可能：空中(ワイヤー未接続) && 左クリック長押し && 突撃していない && ゲージ残
	//   ※ スローは初弾を"ためて狙う"時だけ。連続攻撃中(突撃/受付窓)はスローしない
	bool airborne = !m_isGrounded && !m_upWire->IsAttached();
	bool canAim   = airborne && !m_isDiving;
	bool holding  = KdInputManager::Instance().IsHold("DiveAttack");
	bool slowing  = canAim && holding && m_focusGauge > 0.0f;

	if (slowing)
	{
		Application::Instance().SetTimeScale(slowVal);   // 世界をスローに
		m_focusGauge -= realDt;
		if (m_focusGauge < 0.0f) { m_focusGauge = 0.0f; }
	}
	else
	{
		Application::Instance().SetTimeScale(1.0f);       // 等速に戻す
		m_focusGauge += refill * realDt;                  // 未使用/地上で回復
		if (m_focusGauge > maxGauge) { m_focusGauge = maxGauge; }
	}
}

void Player::UpdateDive(float dt)
{
	float radius     = DebugParams::Instance().Float(U8("落下攻撃/斬撃範囲"), 1.5f, 0.5f, 15.0f);
	float chainRange = DebugParams::Instance().Float(U8("連続攻撃/範囲"),   8.0f, 1.0f, 30.0f);

	// === 突撃中(対象へ引き寄せ、斬ったあとはキー入力で次の敵へ続ける＝連続攻撃) ===
	if (m_isDiving)
	{
		std::shared_ptr<KdGameObject> spTarget = m_wpDiveTarget.lock();
		if (spTarget && spTarget->IsExpired()) { spTarget = nullptr; m_wpDiveTarget.reset(); }

		// --- 斬った後の継続受付：受付時間内に突撃キーを押したら周りの敵へ続けて突撃する ---
		//     (自動では向かわない。押さなければ受付終了で落下に戻る)
		if (!spTarget && m_comboWindowTimer > 0.0f)
		{
			m_comboWindowTimer -= dt;

			// 受付中は減速してほぼ止まって待てるようにする(次を狙う"間")
			float windowDamp = DebugParams::Instance().Float(U8("連続攻撃/継続中の減速"), 8.0f, 0.0f, 30.0f);
			m_velocity *= std::clamp(1.0f - windowDamp * dt, 0.0f, 1.0f);

			if (KdInputManager::Instance().IsRelease("DiveAttack"))
			{
				spTarget = FindNearestEnemy(GetPos(), chainRange);   // 離した瞬間に次の敵へ
				if (spTarget) { m_wpDiveTarget = spTarget; m_comboWindowTimer = 0.0f; }
			}

			if (!spTarget)
			{
				if (m_comboWindowTimer <= 0.0f) { m_isDiving = false; }   // 受付終了→落下へ
				return;
			}
			// 次の対象が決まった → 下のダッシュへ流れる
		}

		// 対象がいない(受付窓もない)=真下ダイブ：そのまま落下(着地でPostUpdateが終了処理)
		if (!spTarget) { return; }

		float pullAccel = DebugParams::Instance().Float(U8("落下攻撃/引き寄せ加速"),     80.0f, 5.0f, 300.0f);
		float pullMax   = DebugParams::Instance().Float(U8("落下攻撃/引き寄せ上限速度"), 45.0f, 5.0f, 150.0f);

		// 対象(少し上=胴の高さ)へのベクトル。対象は動くので毎フレーム狙い直す(ホーミング)
		Math::Vector3 aim = spTarget->GetPos() + Math::Vector3(0.0f, 0.5f, 0.0f);
		Math::Vector3 to  = aim - GetPos();
		float dist = to.Length();

		if (dist <= radius)
		{
			// 斬る
			spTarget->OnHit(this);
			m_diveChainCount++;
			CameraShake::Instance().AddTrauma(std::clamp(0.2f + 0.05f * m_diveChainCount, 0.0f, 0.7f));
			EffectManager::Instance().SpawnSlash(aim);   // 斬った位置に斬撃エフェクト

			// 斬った直後は減速する(0=止まる/1=減速なし)
			float slowRate = DebugParams::Instance().Float(U8("連続攻撃/斬り後の速度残し"), 0.4f, 0.0f, 1.0f);
			m_velocity *= slowRate;

			// 斬った対象を解除し、次の突撃を受け付ける窓を開く(この間にキーを押せば継続突撃)
			m_wpDiveTarget.reset();
			m_comboWindowTimer = DebugParams::Instance().Float(U8("連続攻撃/継続受付時間"), 0.5f, 0.05f, 2.0f);
			return;
		}

		to /= dist;
		// リールで引かれるように、速さを加速でrampしつつ常に対象へまっすぐ向ける
		float sp = m_velocity.Length() + pullAccel * dt;
		if (sp > pullMax) { sp = pullMax; }
		m_velocity = to * sp;
		return;
	}

	// === 開始判定：空中で左クリックを"離した瞬間"に突撃(長押し中はUpdateAirFocusがスロー＋狙い) ===
	if (m_isGrounded) { return; }                                        // 空中専用
	if (!KdInputManager::Instance().IsRelease("DiveAttack")) { return; } // 離した瞬間だけ発火

	m_isDiving = true;
	m_comboWindowTimer = 0.0f;

	// 自動ターゲットがいれば「対象へワイヤーで引き寄せ」、いなければ従来の真下ダイブ
	std::shared_ptr<KdGameObject> spLock = m_upTargeting->GetTarget();
	if (spLock)
	{
		// 以降UpdateDiveが対象へ引き寄せ、ワイヤーの線はDrawWireが手元→対象に描く
		m_wpDiveTarget = spLock;
	}
	else
	{
		m_wpDiveTarget.reset();
		float diveSpeed = DebugParams::Instance().Float(U8("落下攻撃/降下速度"), 30.0f, 5.0f, 100.0f);
		m_velocity.y = -diveSpeed;   // 真下(水平の勢いは残す)
	}
}

std::shared_ptr<KdGameObject> Player::FindNearestEnemy(const Math::Vector3& center, float range) const
{
	// 範囲内で最も近い「生きている」敵を返す(斬った直後の敵はIsExpiredで除外)
	std::shared_ptr<KdGameObject> best;
	float bestDist = range;
	for (auto& spEnemy : SceneManager::Instance().FindObjectsWithTag(ObjectTag::Enemy))
	{
		if (!spEnemy || spEnemy->IsExpired()) { continue; }
		float d = Math::Vector3::Distance(center, spEnemy->GetPos());
		if (d < bestDist) { bestDist = d; best = spEnemy; }
	}
	return best;
}

void Player::UpdateSweep(float dt)
{
	// クールダウンを消化
	if (m_sweepCooldownTimer > 0.0f) { m_sweepCooldownTimer -= dt; }

	// === 発動判定 ===
	if (m_isDiving) { return; }                                    // 突撃中は使わない
	if (m_sweepCooldownTimer > 0.0f) { return; }                   // クールダウン中
	if (!KdInputManager::Instance().IsPress("Skill")) { return; }

	float range = DebugParams::Instance().Float(U8("振り回し/範囲"), 5.0f, 1.0f, 20.0f);

	// 周囲(半径range)の敵をまとめて斬る(振り回しの一掃)
	for (auto& spEnemy : SceneManager::Instance().FindObjectsWithTag(ObjectTag::Enemy))
	{
		if (!spEnemy || spEnemy->IsExpired()) { continue; }
		if (Math::Vector3::Distance(GetPos(), spEnemy->GetPos()) <= range)
		{
			spEnemy->OnHit(this);
		}
	}

	// 見た目：プレイヤーの周りに斬撃を輪状に出して"振り回し"を表現
	const int kNum = 8;
	float visR = range * 0.6f;
	for (int i = 0; i < kNum; ++i)
	{
		float a = (float)i / (float)kNum * DirectX::XM_2PI;
		EffectManager::Instance().SpawnSlash(GetPos() + Math::Vector3(cosf(a) * visR, 0.6f, sinf(a) * visR));
	}

	CameraShake::Instance().AddTrauma(0.6f);
	m_sweepCooldownTimer = DebugParams::Instance().Float(U8("振り回し/クールダウン"), 1.0f, 0.0f, 5.0f);
}

void Player::Respawn()
{
	// 開始位置へ戻し、勢い・接地・ワイヤー・突撃/連続攻撃状態をすべてリセットする
	SetPos(m_spawnPos);
	m_velocity = Math::Vector3::Zero;
	m_isGrounded = false;
	if (m_upWire) { m_upWire->Release(); }
	m_isDiving = false;
	m_wpDiveTarget.reset();
	m_diveChainCount = 0;
	m_comboWindowTimer = 0.0f;
	Application::Instance().SetTimeScale(1.0f);   // スローを解除(スロー中に落下リセットしても等速へ)
	m_isDodging = false;
	m_dodgeTimer = 0.0f;
	m_invincibleTimer = 0.0f;
	m_sweepCooldownTimer = 0.0f;
}

void Player::PostUpdate()
{
	// 地面(KdCollider::TypeGround)に立つ
	// ※ ワイヤー中は地面吸着させず、ワイヤー物理(3D速度)に任せる
	if (!m_upWire->IsAttached())
	{
		GroundCheck();
	}

	// 突撃中に着地したとき：対象も継続受付窓も無い(=真下ダイブが空振りして着地)なら終了する。
	// ダッシュ中(対象あり)や継続受付中(窓あり=キー入力待ち)は着地しても打ち切らない
	if (m_isDiving && IsGrounded() && m_wpDiveTarget.expired() && m_comboWindowTimer <= 0.0f)
	{
		m_isDiving = false;
		m_wpDiveTarget.reset();
	}

	// 接地して待機(突撃していない)状態ならチェインは途切れる(次は1から数え直す)
	if (IsGrounded() && !m_isDiving) { m_diveChainCount = 0; }

	// === 着地・壁ヒットの手応え(カメラを揺らす) ===
	// CharaBaseが記録した衝撃をConsumeし、一定以上ならtraumaを加える(小さすぎる衝撃は無視)。
	// ※ Playerだけが発火する。Enemyも同じ記録はするが読まないのでカメラは揺れない
	float landing = ConsumeLandingImpact();
	if (landing > 3.0f) { CameraShake::Instance().AddTrauma(std::clamp(landing / 20.0f, 0.0f, 0.6f)); }

	float wall = ConsumeWallImpact();
	if (wall > 4.0f) { CameraShake::Instance().AddTrauma(std::clamp(wall / 25.0f, 0.0f, 0.7f)); }

	// 照準：画面中心に一番近い敵を自動ターゲット(カメラは回さない)。選定とマーカーはTargetingが持つ
	m_upTargeting->Update(m_wpCamera.lock(), Application::Instance().GetDeltaTime());
}

void Player::DrawUnLit()
{
	// キャラのモデルは CharaBase::DrawLit が描く。ここ(陰影なしパス)では
	// ワイヤーの見た目・自動ターゲットのマーカーを描く(斬撃VFXはEffectManagerが描く)。
	DrawWire();
	m_upTargeting->DrawMarker();
}

void Player::DrawWire()
{
	if (!m_upWire) { return; }

	// ワイヤーの手元(発射位置に近い高さ)。端点だけ決め、線の描画はWireActionに任せる
	Math::Vector3 from = GetPos() + Math::Vector3(0.0f, 0.25f, 0.0f);

	// スイング中：アンカーへ線を引く
	if (m_upWire->IsAttached())
	{
		m_upWire->Draw(from, m_upWire->GetAnchor());
		return;
	}

	// 突撃(グラップル)中：引き寄せている対象へ線を引く
	if (m_isDiving)
	{
		if (std::shared_ptr<KdGameObject> spTarget = m_wpDiveTarget.lock())
		{
			m_upWire->Draw(from, spTarget->GetPos() + Math::Vector3(0.0f, 0.5f, 0.0f));
		}
	}
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
