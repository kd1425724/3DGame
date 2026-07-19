#include "Player.h"

#include "../../../main.h"
#include "../../Camera/CameraBase.h"
#include "../../Camera/TPSCamera/TPSCamera.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugFlags/DebugFlags.h"
#include "../../../Debug/DebugWatch/DebugWatch.h"
#include "../../Camera/CameraShake.h"
#include "../../../Effect/EffectManager.h"
#include "../../Targeting/Targeting.h"
#include "../../../Collision/CollisionGrid.h"   // IsWallBetween(落下攻撃の突撃先が壁の裏か)

#include"../../Wire/WireAction.h"

Player::Player()
{
}

Player::~Player()
{
}

void Player::Init()
{
	// スキンメッシュのキャラ(身長約1.95m・変形ボーン77本・アニメ13本を内包)。
	// ※ 立方体だった頃の名残で、当たり判定は「モデル＝1辺1mの立方体」前提のまま
	//    (CharaBaseがGetScale().y*0.5を体の半分の高さとして使う)。表示と当たりのサイズは
	//    まだ一致していない ── 表示を確認してから合わせる
	SetAsset("Asset/Models/Character/Scifi_girl/Scifi_girl.gltf");

	// モデルは頂点カラーで色が付いているので、色の乗算は白(=素の色)にする
	m_color = Math::Color(1.0f, 1.0f, 1.0f, 1.0f);

	SetScale(Math::Vector3(1.0f, 1.0f, 1.0f));

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

	// 反撃(ジャスト回避カウンター)の発動。回避中(無敵)に処理される必要があるので、
	// 回避の早期return(下の m_isDodging)より前に置く。反撃スローはAirFocusの後に上書きする
	UpdateCounter();

	// 回避の無敵時間を消化(回避が終わっても必ず毎フレーム減らす)。
	// ※ 以前はUpdateDodgeの「回避実行中」ブロック内だけで減らしていたため、
	//   無敵時間(0.2)>回避時間(0.18)だと端数が減らず"一度回避したらずっと無敵"になっていた
	if (m_invincibleTimer > 0.0f)
	{
		m_invincibleTimer -= dt;
	}

	// 被弾ノックバックの硬直を消化(この間は移動入力を無視して勢いを崩される)。
	// 硬直中は水平速度を摩擦で徐々に殺し、ノックバックで遠くまで(崖から)吹き飛ばされ続けないようにする
	if (m_staggerTimer > 0.0f)
	{
		m_staggerTimer -= dt;
		float fric = DebugParams::Instance().Float(U8("反撃/被弾の摩擦"), 6.0f, 0.0f, 30.0f);
		float k = 1.0f - fric * dt;
		if (k < 0.0f)
		{
			k = 0.0f;
		}
		m_velocity.x *= k;
		m_velocity.z *= k;
	}

	// ワイヤーの発射/解除(入力・狙いはPlayer側。スイング物理はWireActionに委譲)
	UpdateWireInput();

	// 加速/空中ステップ(右クリック)。ワイヤー中でも使えるよう、ワイヤー分岐より前で処理する
	UpdateAccel(dt);

	// ワイヤー接続中はスイング物理だけ行い、通常移動・ジャンプ・レーザーは止める。
	// 移動入力をそのまま渡す：X=操舵(振り子の向きを曲げる) / Y=前方への漕ぎ。
	// 加えて上下の噴射(立体機動のガス噴射にあたる)を Space=上 / Ctrl=下 で渡す。
	// ※ ワイヤー中はジャンプが使われないのでSpaceを転用している
	// 実際の移動はWireAction::UpdateSwingがこのキャラを動かす
	if (m_upWire->IsAttached())
	{
		float thrustUp = 0.0f;
		if (KdInputManager::Instance().IsHold("Jump"))
		{
			thrustUp += 1.0f;
		}
		if (KdInputManager::Instance().IsHold("Ctrl"))
		{
			thrustUp -= 1.0f;
		}

		m_upWire->UpdateSwing(*this, dt, KdInputManager::Instance().GetAxisState("Move"), thrustUp);
		return;
	}

	// 回避ダッシュ(クールダウン消化＋実行中は速度を上書き)。ダッシュ中は他の行動を止める
	UpdateDodge(dt);
	if (m_isDodging) { return; }

	// 落下攻撃(突撃/連続攻撃)中は通常移動・ジャンプを止める。
	// ※ UpdateMoveは接地中に水平速度を入力値(無入力なら0)へ上書きするため、
	//   突撃中に走ると継続受付中の流しや突撃の勢いが地面で殺されてしまう
	// ※ 被弾硬直(m_staggerTimer)中も移動/ジャンプを止め、ノックバックの勢いを残す
	if (!m_isDiving && m_staggerTimer <= 0.0f)
	{
		UpdateMove(dt);
		UpdateJump(dt);
	}
	UpdateDive(dt);
}

void Player::UpdateWireInput()
{
	// === アンカー射出 / 攻撃（左クリックに統一） ===
	// 進撃の巨人2に寄せた入力(2026/07/19)。同じボタンで文脈により役割が変わる：
	//   ・E(Focus)でターゲットを取っている、または連続攻撃の受付中 → 攻撃(突撃)
	//   ・それ以外                                                  → ワイヤーを撃つ(移動)
	// 押した時にどちらだったかを m_anchorPressWasWire に覚えておき、
	// 「ワイヤーを撃った時に限り」離したら外す。攻撃で押した時は離しても何もしない
	if (KdInputManager::Instance().IsPress("Anchor"))
	{
		// 突撃中／連続攻撃の受付中は、この押下の扱いを UpdateDive 側が持っている
		// (受付中は最寄りの敵を探して繋ぐ処理があるため、ここで横取りしない)
		if (m_isDiving || m_comboWindowTimer > 0.0f)
		{
			m_anchorPressWasWire = false;
			return;
		}

		// E(Focus)でターゲットを取れていれば攻撃、そうでなければワイヤー
		if (IsAttackInput())
		{
			m_anchorPressWasWire = false;
			StartDive();
			return;
		}

		m_anchorPressWasWire = true;
	}

	if (KdInputManager::Instance().IsPress("Anchor") && m_anchorPressWasWire)
	{
		Math::Vector3 from = GetPos() + Math::Vector3(0, 1.0f, 0);   // ワイヤーの手元
		float maxLen = DebugParams::Instance().Float(U8("ワイヤー/最大長"), 30.0f, 1.0f, 100.0f);

		// 撃つ方向は「レティクル(画面中央)が指す点」へ向ける。
		// カメラは後方＋肩ぶん横にズレているので、カメラの向きを手元から飛ばすと視差でズレる。
		//   ① カメラからレティクル方向(=カメラ前方)へレイを飛ばし、最初に当たった点を照準点にする
		//   ② 手元からその照準点へ向けて撃つ(これで画面中央が指す場所へ正確に飛ぶ)
		Math::Vector3 dir = Math::Vector3::Backward;
		if (std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock())
		{
			Math::Vector3 camPos     = spCamera->GetPos();
			Math::Vector3 camForward = Math::Vector3::TransformNormal(Math::Vector3::Backward, spCamera->GetRotationMatrix());
			camForward.Normalize();

			// ① 照準点を求める。何にも当たらなければカメラ前方の遠方点を狙う
			float aimRange = DebugParams::Instance().Float(U8("ワイヤー/照準レイ長"), 200.0f, 10.0f, 1000.0f);
			Math::Vector3 aimPoint = camPos + camForward * aimRange;

			KdCollider::RayInfo aimRay(KdCollider::TypeGround | KdCollider::TypeBump, camPos, camForward, aimRange);
			std::list<KdCollider::CollisionResult> aimHits;
			for (auto& obj : SceneManager::Instance().GetObjList())
			{
				if (!obj) { continue; }

				obj->Intersects(aimRay, &aimHits);
			}
			float best = aimRange;
			for (auto& h : aimHits)
			{
				float d = Math::Vector3::Distance(camPos, h.m_hitPos);
				if (d < best)
				{
					best = d;
					aimPoint = h.m_hitPos;
				}
			}

			// ② 手元→照準点 の向き(これをワイヤーの発射方向にする)
			dir = aimPoint - from;
		}
		if (dir.LengthSquared() > 0.0001f)
		{
			dir.Normalize();
		}
		else
		{
			dir = Math::Vector3::Backward;
		}

		// 撃った瞬間、今の速度(m_velocity)はそのまま引き継ぐ
		// (走りながら撃てば横の勢いが乗る。速度は基底CharaBaseの共通m_velocity)
		m_upWire->Shoot(from, dir, maxLen);
	}

	// ワイヤーとして撃った時だけ、離したら外す(攻撃で押した時は無視)。
	// 離しても速度(m_velocity)はそのまま=スイングの勢いで飛んでいける(フリング)
	if (KdInputManager::Instance().IsRelease("Anchor"))
	{
		if (m_anchorPressWasWire)
		{
			m_upWire->Release();
		}
		m_anchorPressWasWire = false;
	}
}

void Player::UpdateAccel(float dt)
{
	// 右クリック：長押しで加速し続ける／単押し(短く離す)で空中ステップ。
	// 進撃の巨人2の×ボタン(加速・空中ステップ)にあたる、プレイヤー側の推進力。
	// 方向は「移動入力の方向(カメラ基準)」。無入力なら今の進行方向へ。
	// Jumpを併用していれば上向き成分も足す(ユーザー指定)
	const float tapTime = DebugParams::Instance().Float(U8("加速/単押しとみなす時間"), 0.18f, 0.05f, 0.6f);

	if (KdInputManager::Instance().IsPress("Accel"))
	{
		m_accelHoldTime = 0.0f;
	}

	bool holding = KdInputManager::Instance().IsHold("Accel");
	if (holding)
	{
		m_accelHoldTime += dt;

		// 単押し判定の時間を過ぎたら「長押し＝加速」に確定して、以降は加速し続ける
		if (m_accelHoldTime >= tapTime)
		{
			float acc    = DebugParams::Instance().Float(U8("加速/加速度"),     30.0f, 0.0f, 150.0f);
			float maxSpd = DebugParams::Instance().Float(U8("加速/上限速度"),   35.0f, 0.0f, 120.0f);

			Math::Vector3 dir = GetAccelDir();
			if (dir.LengthSquared() > 0.0001f && m_velocity.Length() < maxSpd)
			{
				m_velocity += dir * (acc * dt);
			}
		}
	}

	// 短く離したら空中ステップ(その場から入力方向へ一気に飛ぶ)
	if (KdInputManager::Instance().IsRelease("Accel"))
	{
		if (m_accelHoldTime < tapTime && !m_isGrounded)
		{
			float step = DebugParams::Instance().Float(U8("加速/空中ステップの速さ"), 18.0f, 0.0f, 60.0f);
			Math::Vector3 dir = GetAccelDir();
			if (dir.LengthSquared() > 0.0001f)
			{
				m_velocity += dir * step;
			}
		}
		m_accelHoldTime = 0.0f;
	}
}

Math::Vector3 Player::GetAccelDir() const
{
	// 移動入力(カメラ基準)の方向。無入力なら今の進行方向。
	// Jumpを押していれば上向き成分を混ぜる(上へ吹かしながら加速できる)
	Math::Vector2 axis = KdInputManager::Instance().GetAxisState("Move");
	Math::Vector3 dir = Math::Vector3::Zero;

	if (axis.LengthSquared() > 0.0001f)
	{
		// ※ Forward/Backwardの定義上、見た目と前後が逆に感じたため入れ替えてある(UpdateMoveと同じ)
		Math::Vector3 wish = Math::Vector3::Backward * axis.y + Math::Vector3::Right * axis.x;
		wish.Normalize();

		if (std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock())
		{
			// カメラの水平向きに合わせて回す(UpdateMoveと同じやり方)
			wish = Math::Vector3::TransformNormal(wish, spCamera->GetRotationYMatrix());
		}
		dir = wish;
	}
	else
	{
		// 無入力なら今の進行方向(水平)。止まっていればカメラ前方
		Math::Vector3 horiz(m_velocity.x, 0.0f, m_velocity.z);
		if (horiz.LengthSquared() > 0.0001f)
		{
			horiz.Normalize();
			dir = horiz;
		}
		else if (std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock())
		{
			Math::Vector3 fwd = Math::Vector3::TransformNormal(Math::Vector3::Backward, spCamera->GetRotationMatrix());
			fwd.y = 0.0f;
			if (fwd.LengthSquared() > 0.0001f)
			{
				fwd.Normalize();
				dir = fwd;
			}
		}
	}

	// Jump併用で上向きを足す
	if (KdInputManager::Instance().IsHold("Jump"))
	{
		float upRate = DebugParams::Instance().Float(U8("加速/Jump併用の上向き割合"), 0.8f, 0.0f, 2.0f);
		dir.y += upRate;
		if (dir.LengthSquared() > 0.0001f)
		{
			dir.Normalize();
		}
	}

	return dir;
}

bool Player::IsAttackInput() const
{
	// E(Focus)を押していて、かつターゲットがいる時だけ「攻撃」として扱う。
	// ※ 連続攻撃の受付中はEを押していなくても攻撃になるが(ユーザー指定)、
	//    その分岐は UpdateWireInput の入口で先に処理している(UpdateDiveへ渡す)
	if (!KdInputManager::Instance().IsHold("Focus")) { return false; }
	if (!m_upTargeting) { return false; }

	return m_upTargeting->GetTarget() != nullptr;
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
			if (accel > addSpeed)
			{
				accel = addSpeed;
			}   // 入力方向がmoveSpeedを超えないよう頭打ち
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
	if (m_isGrounded)
	{
		m_coyoteTimer = 0.0f;
	}
	else
	{
		m_coyoteTimer += dt;
	}

	// 入力があればバッファを満タンに、なければ減らす
	if (KdInputManager::Instance().IsPress("Jump"))
	{
		m_jumpBufferTimer = bufferTime;
	}
	else
	{
		m_jumpBufferTimer -= dt;
		if (m_jumpBufferTimer < 0.0f)
		{
			m_jumpBufferTimer = 0.0f;
		}
	}

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
	if (m_dodgeCooldownTimer > 0.0f)
	{
		m_dodgeCooldownTimer -= dt;
	}

	// === 回避ダッシュ実行中：水平にフラットに素早く移動(縦は止めて空中でもキレよく) ===
	if (m_isDodging)
	{
		float dodgeSpeed = DebugParams::Instance().Float(U8("回避/速度"), 22.0f, 5.0f, 60.0f);
		m_velocity.x = m_dodgeDir.x * dodgeSpeed;
		m_velocity.z = m_dodgeDir.z * dodgeSpeed;
		m_velocity.y = 0.0f;

		m_dodgeTimer -= dt;
		// ※ 無敵時間(m_invincibleTimer)の消化はUpdate()側で毎フレーム行う。
		//   ここ(回避中のみ)で減らすと、無敵時間>回避時間のとき端数が残って永久無敵になる
		if (m_dodgeTimer <= 0.0f)
		{
			m_isDodging = false;
		}
		return;
	}

	// === 開始判定 ===
	if (m_isDiving) { return; }                                      // 突撃中は回避しない
	if (m_dodgeCooldownTimer > 0.0f) { return; }                     // クールダウン中
	if (!KdInputManager::Instance().IsPress("Dodge")) { return; }

	// 方向：移動入力があればその向き(カメラ基準)、無ければカメラ前方(水平)
	Math::Vector2 moveAxis = KdInputManager::Instance().GetAxisState("Move");
	Math::Vector3 dir = Math::Vector3::Backward * moveAxis.y + Math::Vector3::Right * moveAxis.x;
	if (dir.LengthSquared() < 0.0001f)
	{
		dir = Math::Vector3::Backward;
	}   // 入力なし→前方へ
	if (std::shared_ptr<CameraBase> spCam = m_wpCamera.lock())
	{
		dir = Math::Vector3::TransformNormal(dir, spCam->GetRotationYMatrix());
	}
	dir.y = 0.0f;
	if (dir.LengthSquared() < 0.0001f)
	{
		dir = Math::Vector3::Backward;
	}
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

	// スロー可能：空中(ワイヤー未接続) && E(Focus)長押し && 突撃していない && ゲージ残
	//   ※ スローは初弾を"ためて狙う"時だけ。連続攻撃中(突撃/受付窓)はスローしない
	//   ※ 2026/07/19の入力再設計で、長押し元を左クリックからE(Focus)へ移した。
	//     左クリックはアンカー射出＋攻撃に統一したため
	bool airborne = !m_isGrounded && !m_upWire->IsAttached();
	bool canAim   = airborne && !m_isDiving;
	bool holding  = KdInputManager::Instance().IsHold("Focus");

	// 「スローを掛けたい状況か(空中で長押し中)」と「実際に掛けられるか(ゲージ残あり)」を分ける
	bool wantSlow = canAim && holding;
	bool slowing  = wantSlow && m_focusGauge > 0.0f;

	if (slowing)
	{
		Application::Instance().SetTimeScale(slowVal);   // 世界をスローに
		m_focusGauge -= realDt;
		if (m_focusGauge < 0.0f)
		{
			m_focusGauge = 0.0f;
		}
	}
	else
	{
		Application::Instance().SetTimeScale(1.0f);       // 等速に戻す
	}

	// ゲージ回復は「スローを掛けようとしていない」間だけ行う(地上/未使用/離した時)。
	// ※ 長押し中にゲージ切れした瞬間に回復させると、翌フレームまた少し溜まって再スロー…を毎フレーム
	//   繰り返し、timeScaleが slowVal↔1.0 で振動 → 世界も暗幕も点滅してしまう。それを防ぐため
	//   "長押し中(wantSlow)はゲージ0のまま据え置き"にする(離す/着地して初めて回復する)
	if (!wantSlow)
	{
		m_focusGauge += refill * realDt;
		if (m_focusGauge > maxGauge)
		{
			m_focusGauge = maxGauge;
		}
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
		if (spTarget && spTarget->IsExpired())
		{
			spTarget = nullptr;
			m_wpDiveTarget.reset();
		}

		// --- 斬った後の継続受付：受付時間内に突撃キーを押したら周りの敵へ続けて突撃する ---
		//     (自動では向かわない。押さなければ受付終了で落下に戻る)
		if (!spTarget && m_comboWindowTimer > 0.0f)
		{
			m_comboWindowTimer -= dt;

			// 受付中は減速してほぼ止まって待てるようにする(次を狙う"間")
			float windowDamp = DebugParams::Instance().Float(U8("連続攻撃/継続中の減速"), 8.0f, 0.0f, 30.0f);
			m_velocity *= std::clamp(1.0f - windowDamp * dt, 0.0f, 1.0f);

			// ※ 2026/07/19の入力再設計で「離した瞬間」→「押した瞬間」に変更。
			//    狙い(スロー)がEへ移り、長押しで溜める必要がなくなったため
			if (KdInputManager::Instance().IsPress("Anchor"))
			{
				spTarget = FindNearestEnemy(GetPos(), chainRange);   // 押した瞬間に次の敵へ
				if (spTarget)
				{
					m_wpDiveTarget = spTarget;
					m_comboWindowTimer = 0.0f;
				}
			}

			if (!spTarget)
			{
				if (m_comboWindowTimer <= 0.0f)
				{
					m_isDiving = false;
				}   // 受付終了→落下へ
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

		// 対象が壁(塔)の裏＝遮蔽されていたら突撃を中断(壁に突っ込んで操作不能になるのを防ぐ)
		if (CollisionGrid::IsWallBetween(GetPos(), aim))
		{
			m_isDiving = false;
			m_wpDiveTarget.reset();
			m_comboWindowTimer = 0.0f;
			return;
		}

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
		if (sp > pullMax)
		{
			sp = pullMax;
		}
		m_velocity = to * sp;
		return;
	}

	// ※ 突撃の開始は UpdateWireInput 側(左クリックを押した瞬間)から StartDive() で行う。
	//    以前は「左クリックを離した瞬間」に発火していたが、2026/07/19の入力再設計で
	//    狙い(スロー)がEへ移り、長押しで溜める必要がなくなったため押下開始に変えた
}

void Player::StartDive()
{
	if (m_isGrounded) { return; }   // 空中専用

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
		if (d < bestDist)
		{
			bestDist = d;
			best = spEnemy;
		}
	}
	return best;
}

// ※ スキル「振り回し一掃」(Eキー)は 2026/07/19 に廃止した。
//    進撃の巨人2に寄せた入力へ再設計するにあたり、Eキーを「ターゲット＋スロー」に
//    割り当てる必要が生じたため。ユーザー判断で移設ではなく削除。

void Player::Respawn()
{
	// 開始位置へ戻し、勢い・接地・ワイヤー・突撃/連続攻撃状態をすべてリセットする
	SetPos(m_spawnPos);
	m_velocity = Math::Vector3::Zero;
	m_isGrounded = false;
	if (m_upWire)
	{
		m_upWire->Release();
	}
	m_isDiving = false;
	m_wpDiveTarget.reset();
	m_diveChainCount = 0;
	m_comboWindowTimer = 0.0f;
	Application::Instance().SetTimeScale(1.0f);   // スローを解除(スロー中に落下リセットしても等速へ)
	m_isDodging = false;
	m_dodgeTimer = 0.0f;
	m_invincibleTimer = 0.0f;
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
	if (IsGrounded() && !m_isDiving)
	{
		m_diveChainCount = 0;
	}

	// === 着地・壁ヒットの手応え(カメラを揺らす) ===
	// CharaBaseが記録した衝撃をConsumeし、一定以上ならtraumaを加える(小さすぎる衝撃は無視)。
	// ※ Playerだけが発火する。Enemyも同じ記録はするが読まないのでカメラは揺れない
	float landing = ConsumeLandingImpact();
	if (landing > 3.0f)
	{
		CameraShake::Instance().AddTrauma(std::clamp(landing / 20.0f, 0.0f, 0.6f));
	}

	float wall = ConsumeWallImpact();
	if (wall > 4.0f)
	{
		CameraShake::Instance().AddTrauma(std::clamp(wall / 25.0f, 0.0f, 0.7f));
	}

	// 照準：E(Focus)を押している間だけターゲットを取る(2026/07/19の入力再設計)。
	// 常時ロックだと移動中もマーカーが付きっぱなしになるため、「狙う」を明示的な操作にした。
	// 連続攻撃の受付中はEを押していなくてもロックを維持する(次の対象へ繋げるため)。
	// カメラを渡さずに呼ぶとTargeting側がターゲットを解除する
	bool wantTarget = KdInputManager::Instance().IsHold("Focus") || m_comboWindowTimer > 0.0f;
	m_upTargeting->Update(wantTarget ? m_wpCamera.lock() : nullptr,
		Application::Instance().GetDeltaTime());

	// デバッグ用：状態値をDebugWatchへ(このフレームの最終状態を出す。PostUpdateは毎フレーム必ず走る)
	WatchDebug();
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
	// 当たり判定表示(DebugFlags「当たり判定/AABB表示」)ON時、索敵範囲・攻撃範囲などを可視化する
	if (s_showColliderDebug)
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		DrawDebugRanges();
	}
	// KdGameObject::DrawDebugが m_pCollider のAABBを積み、m_pDebugWire をまとめて描画する
	KdGameObject::DrawDebug();
}

void Player::WatchDebug() const
{
	DebugWatch& w = DebugWatch::Instance();

	// 速度・接地まわり
	w.Watch(U8("Player/水平速度"),   GetHorizontalSpeed());
	w.Watch(U8("Player/垂直速度"),   m_velocity.y);
	w.Watch(U8("Player/接地"),       IsGrounded());
	w.Watch(U8("Player/ワイヤー接続"), m_upWire && m_upWire->IsAttached());

	// 攻撃・突撃まわり
	w.Watch(U8("Player/突撃中"),          m_isDiving);
	w.Watch(U8("Player/チェイン数"),      m_diveChainCount);
	w.Watch(U8("Player/連続攻撃の受付窓"), m_comboWindowTimer);
	w.Watch(U8("Player/フォーカスゲージ"), m_focusGauge);

	// クールタイム・猶予窓まわり
	w.Watch(U8("Player/回避クールタイム"),     m_dodgeCooldownTimer);
	w.Watch(U8("Player/反撃スロー窓"),         m_counterWindowTimer);
	w.Watch(U8("Player/被弾硬直"),             m_staggerTimer);
	w.Watch(U8("Player/無敵"),                 IsInvincible());
	w.Watch(U8("Player/コヨーテ時間"),         m_coyoteTimer);
	w.Watch(U8("Player/ジャンプ先行入力"),     m_jumpBufferTimer);
}

void Player::DrawDebugRanges()
{
	// ※ 呼び出し元(DrawDebug)で s_showColliderDebug と m_pDebugWire を確認済み
	const Math::Vector3 pos = GetPos();

	// 攻撃判定：落下攻撃の斬撃範囲(赤)。UpdateDiveと同じDebugParamsキーを読む
	float slashR = DebugParams::Instance().Float(U8("落下攻撃/斬撃範囲"), 1.5f, 0.5f, 15.0f);
	m_pDebugWire->AddDebugSphere(pos, slashR, Math::Color(1.0f, 0.2f, 0.2f, 1.0f));


	// 索敵範囲：連続攻撃で次の敵を探す範囲(緑)
	float findR = DebugParams::Instance().Float(U8("連続攻撃/範囲"), 8.0f, 1.0f, 30.0f);
	m_pDebugWire->AddDebugSphere(pos, findR, Math::Color(0.2f, 1.0f, 0.4f, 1.0f));

	// 現在の自動ターゲットへの線(水色)
	if (m_upTargeting)
	{
		std::shared_ptr<KdGameObject> spTarget = m_upTargeting->GetTarget();
		if (spTarget)
		{
			m_pDebugWire->AddDebugLine(pos, spTarget->GetPos(), Math::Color(0.2f, 0.9f, 1.0f, 1.0f));
		}
	}

	// ワイヤー接続中：手元→アンカーの線(青)
	if (m_upWire && m_upWire->IsAttached())
	{
		m_pDebugWire->AddDebugLine(pos, m_upWire->GetAnchor(), Math::Color(0.4f, 0.6f, 1.0f, 1.0f));
	}
}

void Player::NotifyCounter()
{
	// 敵(Enemy::ResolveStrikeHit)がジャスト回避を受けた時に呼ぶ。実際の窓開けは次のUpdateCounterで行う
	// (発動側をPlayerに集約して、スロー窓や突撃移行の管理を1箇所にまとめるため)
	m_counterPending = true;
}

void Player::ApplyKnockback(const Math::Vector3& _dir, float _power)
{
	// 回避中(無敵)は弾かれない。※呼び出し側(Enemy)も無敵時は反撃へ回すので通常来ないが保険
	if (IsInvincible()) { return; }

	Math::Vector3 dir = _dir;
	dir.y = 0.0f;
	if (dir.LengthSquared() > 0.0001f)
	{
		dir.Normalize();
	}
	else
	{
		dir = Math::Vector3::Backward;
	}

	// 水平に吐き飛ばして勢いを崩す。HPは無いのでダメージ自体は無い。
	// 上向きには基本飛ばさない(浮かせると空中で慣性が保たれ、崖から遠くまで吹き飛ぶため既定0)
	m_velocity.x = dir.x * _power;
	m_velocity.z = dir.z * _power;
	float pop = DebugParams::Instance().Float(U8("反撃/被弾の浮き"), 0.0f, 0.0f, 15.0f);
	if (pop > 0.0f && m_velocity.y < pop)
	{
		m_velocity.y = pop;
	}

	// 短い硬直(この間は移動入力が効かない=勢いを崩される)
	m_staggerTimer = DebugParams::Instance().Float(U8("反撃/被弾硬直"), 0.3f, 0.0f, 1.5f);

	CameraShake::Instance().AddTrauma(0.4f);
}

void Player::UpdateCounter()
{
	float slow      = DebugParams::Instance().Float(U8("反撃/スロー倍率"), 0.2f, 0.05f, 1.0f);
	float window    = DebugParams::Instance().Float(U8("反撃/受付時間"),   0.6f, 0.1f,  2.0f);
	float findRange = DebugParams::Instance().Float(U8("連続攻撃/範囲"),   8.0f, 1.0f, 30.0f);

	// --- ジャスト回避が成立した瞬間：スローの猶予窓を開く(成立の合図も出す) ---
	if (m_counterPending)
	{
		m_counterPending     = false;
		m_counterWindowTimer = window;
		//反撃直後に発生はいらないクリックしたら攻撃開始でいい
		//EffectManager::Instance().SpawnSlash(GetPos() + Math::Vector3(0.0f, 0.8f, 0.0f));
		CameraShake::Instance().AddTrauma(0.3f);
	}

	// --- スロー猶予窓：この間だけ時間をスローにし、攻撃(左クリック)で今の突撃(ダイブ)へ移行する ---
	if (m_counterWindowTimer > 0.0f)
	{
		// AirFocusの後に呼ばれるのでSetTimeScaleが上書き優先される。窓の消化は実時間で行う
		Application::Instance().SetTimeScale(slow);
		m_counterWindowTimer -= Application::Instance().GetRealDeltaTime();

		// 攻撃ボタン(左クリック)を押したら突撃へ移行する(狙いはauto-target、無ければ最寄りの敵)
		if (KdInputManager::Instance().IsPress("Anchor"))
		{
			std::shared_ptr<KdGameObject> spTarget = m_upTargeting->GetTarget();
			if (!spTarget)
			{
				spTarget = FindNearestEnemy(GetPos(), findRange);
			}

			m_isDodging          = false;   // 回避から突撃へクリーンに移行する
			m_counterWindowTimer = 0.0f;    // 窓を閉じる(スロー解除は次フレームのAirFocusが担当)

			if (spTarget)
			{
				m_isDiving         = true;
				m_comboWindowTimer = 0.0f;
				m_wpDiveTarget     = spTarget;   // 以降UpdateDiveが対象へ引き寄せて突撃する
			}
			return;
		}
		// 押さずに窓が切れたら通常へ戻る(スローは次フレームのAirFocusが等速に戻す)
	}
}
