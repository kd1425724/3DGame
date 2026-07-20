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
#include"../../Wall/WallAction.h"

Player::Player()
{
}

Player::~Player()
{
}

void Player::Init()
{
	// スキンメッシュのキャラ(変形ボーン77本・アニメ13本を内包)
	SetAsset("Asset/Models/Character/Scifi_girl/Scifi_girl.gltf");

	// 当たり判定と描画位置をモデルの実寸に合わせる(2026/07/20)。
	// glTFの頂点はY=0.0009〜1.946＝原点が足元・身長1.946m。
	// これを設定しないと「1辺1mの立方体」前提のままになり、足元が半身ぶん浮く
	m_bodyHeight = 1.946f;
	m_modelOriginIsFeet = true;

	// モデルは頂点カラーで色が付いているので、色の乗算は白(=素の色)にする
	m_color = Math::Color(1.0f, 1.0f, 1.0f, 1.0f);

	// 【2026/07/20 実験】街を1/6スケールにしたので、プレイヤーも同じ比率へ。
	// これをしないと世界に対して巨人になり、影の見え方の比較にならない。
	// 戻す時は 1.0f へ(GetBodyHalfHeightがこのスケールを掛けるので当たりも一緒に縮む)
	SetScale(Math::Vector3(0.1667f, 0.1667f, 0.1667f));

	//ワイヤー(物理＋見た目を内包。見た目の板ポリ生成はWireActionのctorが行う)
	m_upWire = std::make_unique<WireAction>();

	// 壁走り／壁ジャンプ(当たり判定はCharaBase::ResolveBumpの結果を読むだけなので追加の負荷は無い)
	m_upWall = std::make_unique<WallAction>();

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
		// ワイヤーへ移ったら壁走りは中断する。
		// ※ 壁走りは重力を止めている(m_gravityScale=0)ので、中断せずに素通りすると
		//    重力が止まったままになる。早期returnする経路では必ずCancelを通すこと
		m_upWall->Cancel(*this);

		// ※ Space/Ctrl単独の上下噴射は 2026/07/20 に廃止(ユーザー指示)。
		//    上への推進は加速ボタン(右クリック)＋Spaceへ一本化した。UpdateAccelが担当する
		Math::Vector2 wireMove = KdInputManager::Instance().GetAxisState("Move");

		// 前後の漕ぎでもエフェクトを出す(吹かしていることが分かるように)
		if (wireMove.y != 0.0f)
		{
			Math::Vector3 horiz(m_velocity.x, 0.0f, m_velocity.z);
			if (horiz.LengthSquared() > 0.0001f)
			{
				horiz.Normalize();
				SpawnBoostFx(horiz * wireMove.y, dt);
			}
		}

		m_upWire->UpdateSwing(*this, dt, wireMove);
		return;
	}

	// 回避ダッシュ(クールダウン消化＋実行中は速度を上書き)。ダッシュ中は他の行動を止める
	UpdateDodge(dt);
	if (m_isDodging)
	{
		// 回避へ移ったら壁走りは中断する(重力を戻すため。上のワイヤー分岐と同じ理由)
		m_upWall->Cancel(*this);
		return;
	}

	// 壁走り／壁ジャンプ(自動発動)。空中で壁に沿って十分な速度で触れていれば走り出す。
	// 中で重力を止め、壁に沿うよう速度を書き換え、Jumpが押されたら壁を蹴る。
	// ※ 突撃中は発動させない。壁沿いに突撃すると壁走りが横取りしてホーミングが崩れるため
	//   (StartDiveでも一度Cancelしているが、突撃が続く間ずっと止めておく必要がある)
	if (m_isDiving)
	{
		m_upWall->Cancel(*this);
	}
	else
	{
		// 移動入力の向きを渡す。壁を向いて前入力していれば、ずり落ちる代わりによじ登る
		m_upWall->Update(*this, dt, GetWishDir());
	}

	// 落下攻撃(突撃/連続攻撃)中は通常移動・ジャンプを止める。
	// ※ UpdateMoveは接地中に水平速度を入力値(無入力なら0)へ上書きするため、
	//   突撃中に走ると継続受付中の流しや突撃の勢いが地面で殺されてしまう
	// ※ 被弾硬直(m_staggerTimer)中も移動/ジャンプを止め、ノックバックの勢いを残す
	// ※ 壁走り中も止める。UpdateMoveが壁沿いの速度を上書きしてしまうのと、
	//   Spaceは壁ジャンプが使うのでUpdateJumpと二重に発火させないため
	if (!m_upWall->IsRunning() && !m_isDiving && m_staggerTimer <= 0.0f)
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
	// 右クリックは接地と空中で意味が変わる。
	//   空中 … 長押しで加速し続ける／単押し(短く離す)で空中ステップ
	//          進撃の巨人2の×ボタン(加速・空中ステップ)にあたる、プレイヤー側の推進力。
	//          方向は移動入力(カメラ基準)、無入力なら進行方向。Jump併用で上向き成分も足す
	//   地上 … 押した瞬間にステップ(回避)、押し続けているあいだダッシュ(原神と同じ形)
	//
	// 【なぜ地上だけ「押した瞬間」なのか】
	// 空中ステップは離した時に出している。単押しか長押しかを単押し判定の時間で見分けてから
	// 出す必要があるためで、その分だけ反応が遅れる。地上ではこの遅れがそのままキレの無さになる。
	// 地上は「押したら必ずステップが出る／押し続けていたらダッシュへ移る」にすることで、
	// 事前に単押しと長押しを見分ける必要そのものを無くしている。
	// 逆に空中を押下発火に揃えてはいけない：空中は長押しの加速が主役なので、
	// 加速しようとするたび毎回ステップが暴発してしまう
	const float tapTime = DebugParams::Instance().Float(U8("加速/単押しとみなす時間"), 0.18f, 0.05f, 0.6f);

	if (KdInputManager::Instance().IsPress("Accel"))
	{
		m_accelHoldTime = 0.0f;
		m_accelPressWasGround = m_isGrounded;

		// ※ 地上ステップ(回避)の発動は UpdateDodge が同じ押下を見て行う。
		//   無敵・クールダウン・速度の上書きが既にあちらに揃っているため、二重に持たない
	}

	bool holding = KdInputManager::Instance().IsHold("Accel");
	if (holding)
	{
		m_accelHoldTime += dt;
	}

	// 地上で押しっぱなし＝ダッシュ。実際の速度切り替えは UpdateMove が行う。
	//
	// 【単押し判定の時間で待ってはいけない】2026/07/20 に一度
	// 「m_accelHoldTime >= tapTime になってからダッシュ」にしたが、これは誤りだった。
	// ボタンを離すと押し時間が0に戻るので、ステップ中に離して押し直すと
	// そこから0.18秒だけ歩き速度に落ちる("一瞬歩きに戻ってからダッシュに入る")。
	// 原神はステップが終わった瞬間そのままダッシュへ繋がり、この隙間が無い。
	//
	// 正しくは「接地して押していて、ステップ中でなければダッシュ」。
	// 押した瞬間はステップ(UpdateDodge)が速度を握っているので歩き速度は挟まらず、
	// ステップが明けた時にまだ押していればそのままダッシュへ移る
	m_isSprinting = m_isGrounded && holding && !m_isDodging;

	// 空中の加速は接地中には効かせない。地上で押している間はダッシュが担当なので、
	// ここで加速度まで足すと二重に効いて地上だけ異常に伸びる
	if (holding && !m_isGrounded)
	{
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

			// 加速していることが一目で分かるよう、後方へ噴射エフェクトを出す
			SpawnBoostFx(dir, dt);
		}
	}

	// 短く離したら空中ステップ(その場から入力方向へ一気に飛ぶ)。
	// ※ 地上で押し始めた場合は出さない。押した瞬間に地上ステップ(回避)が既に出ているので、
	//   段差から落ちながら離すとステップが二重に出てしまうため
	if (KdInputManager::Instance().IsRelease("Accel"))
	{
		if (m_accelHoldTime < tapTime && !m_isGrounded && !m_accelPressWasGround)
		{
			float step = DebugParams::Instance().Float(U8("加速/空中ステップの速さ"), 18.0f, 0.0f, 60.0f);
			Math::Vector3 dir = GetAccelDir();
			if (dir.LengthSquared() > 0.0001f)
			{
				m_velocity += dir * step;

				// ステップは一瞬なので、まとめて数粒出して"バッ"と見せる
				int burst = DebugParams::Instance().Int(U8("加速エフェクト/ステップの粒数"), 6, 0, 30);
				for (int i = 0; i < burst; ++i)
				{
					EffectManager::Instance().SpawnBoost(GetBoostSpawnPos(dir), dir);
				}
			}
		}
		m_accelHoldTime = 0.0f;
		m_accelPressWasGround = false;
	}
}

void Player::ClampSpeed()
{
	// 落下は別枠にする。水平の最高速度で落下まで縛ると、高所から落ちた時に
	// ふわっと減速して不自然になるため(終端速度として別に上限を持たせる)
	// 攻撃(突撃/連続攻撃の受付中)は別枠の高い上限を使う。
	// 移動の上限(45)は「ワイヤーを繋ぐほど際限なく速くなる」のを抑えるための値で、
	// これを攻撃にも掛けると敵へ突っ込む勢いまで削られて遅くなりすぎる、というユーザー指摘。
	// 攻撃は自分で狙って出す短い行動なので、上限は緩くてよい
	bool attacking = m_isDiving || m_comboWindowTimer > 0.0f;
	float maxSpeed = attacking
		? DebugParams::Instance().Float(U8("プレイヤー/最高速度(攻撃中)"), 90.0f, 5.0f, 300.0f)
		: DebugParams::Instance().Float(U8("プレイヤー/最高速度"),         45.0f, 5.0f, 200.0f);
	float maxFall  = DebugParams::Instance().Float(U8("プレイヤー/最大落下速度"), 60.0f, 5.0f, 200.0f);

	// 落下速度の頭打ち
	if (m_velocity.y < -maxFall)
	{
		m_velocity.y = -maxFall;
	}

	// 全体の速さを上限で抑える。向きは変えず大きさだけ縮める
	float sp = m_velocity.Length();
	if (sp > maxSpeed)
	{
		m_velocity *= (maxSpeed / sp);
	}
}

void Player::SpawnBoostFx(const Math::Vector3& _dir, float _dt)
{
	if (_dir.LengthSquared() < 0.0001f) { return; }

	// 毎フレーム出すとフレームレートで密度が変わるので、時間あたりの個数で制御する
	float rate = DebugParams::Instance().Float(U8("加速エフェクト/毎秒の粒数"), 30.0f, 0.0f, 120.0f);
	if (rate <= 0.0f) { return; }

	m_boostFxTimer += _dt;
	float interval = 1.0f / rate;
	while (m_boostFxTimer >= interval)
	{
		m_boostFxTimer -= interval;
		EffectManager::Instance().SpawnBoost(GetBoostSpawnPos(_dir), _dir);
	}
}

Math::Vector3 Player::GetBoostSpawnPos(const Math::Vector3& _dir) const
{
	// 体の中心あたりから、加速方向の少し後ろに出す
	float back = DebugParams::Instance().Float(U8("加速エフェクト/後方オフセット"), 0.5f, 0.0f, 3.0f);
	return GetPos() + Math::Vector3(0.0f, 0.5f, 0.0f) - _dir * back;
}

Math::Vector3 Player::GetWishDir() const
{
	// ※ Forward/Backwardの定義上、見た目と前後が逆に感じたため入れ替え済み
	Math::Vector2 moveAxis = KdInputManager::Instance().GetAxisState("Move");
	Math::Vector3 wishDir = Math::Vector3::Backward * moveAxis.y + Math::Vector3::Right * moveAxis.x;
	if (wishDir.LengthSquared() <= 0.0f) { return Math::Vector3::Zero; }

	wishDir.Normalize();

	// カメラの水平方向の向きに合わせて移動方向を回転させる(TPS的な移動)
	if (std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock())
	{
		wishDir = Math::Vector3::TransformNormal(wishDir, spCamera->GetRotationYMatrix());
	}
	return wishDir;
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
	Math::Vector3 wishDir = GetWishDir();

	// 地上ダッシュ中は速度の定数を差し替えるだけ。接地中は下で水平速度を入力へ即セットするので、
	// これだけで「速く走る・離せば即止まる・向きも即変わる」が手に入る(追加の計算が要らない)。
	// ※ 空中では m_isSprinting は立たないので、空中の推進は従来どおり加速(UpdateAccel)が担当する
	float moveSpeed = m_isSprinting
		? DebugParams::Instance().Float(U8("プレイヤー/ダッシュ速度"), 11.0f, 0.0f, 40.0f)
		: DebugParams::Instance().Float(U8("プレイヤー/移動速度"),     5.0f, 0.0f, 20.0f);
	Math::Vector3 wishVel = Math::Vector3::Zero;
	if (wishDir.LengthSquared() > 0.0f)
	{
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
	// === ストックの再充填 ===
	// 「2回までは続けてすぐ出せて、しばらくステップしなければ2回とも戻る」形(ユーザー指定)。
	// 計測するのは"最後にステップしてからの経過時間"なので、ステップするたび測り直す。
	// ※ 以前は「満タンから使い始めた時だけ測る」方式だったが、それだと1回だけ使って
	//   すぐ2回目を出しても1回目の予定時刻に回復してしまい、「ステップし続けている間は
	//   戻らない」という意図と合わなかった
	const int maxCharges = GetMaxDodgeCharges();

	// 実行中にImGuiでストック数を減らされた時の保険
	if (m_dodgeCharges > maxCharges)
	{
		m_dodgeCharges = maxCharges;
	}

	if (m_dodgeRechargeTimer > 0.0f)
	{
		m_dodgeRechargeTimer -= dt;
	}

	// 戻す時は2回分まとめて全快させる(ユーザー指示)。1つずつ戻すと、
	// 「1つだけ戻った半端な状態」で出してすぐまた空になり、リズムが読めなかった
	if (m_dodgeRechargeTimer <= 0.0f && m_dodgeCharges < maxCharges)
	{
		m_dodgeCharges = maxCharges;
	}

	// === 次のステップの先行入力(ユーザー指定) ===
	// ステップ中に押した分を覚えておき、今のステップが明けた瞬間に次へ繋ぐ。
	// ジャンプの先行入力(m_jumpBufferTimer)と同じ考え方。
	// ※ 猶予はステップ時間(0.18)より長いので、ここで覚えるだけだと1回押しただけで
	//   2回目が勝手に出てしまう。開始時に必ず0へ消費すること(下の開始処理を参照)
	if (m_dodgeBufferTimer > 0.0f)
	{
		m_dodgeBufferTimer -= dt;
	}

	if (KdInputManager::Instance().IsPress("Accel"))
	{
		m_dodgeBufferTimer = DebugParams::Instance().Float(U8("回避/先行入力"), 0.2f, 0.0f, 1.0f);
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
		if (m_dodgeTimer > 0.0f)
		{
			return;
		}

		// 今フレームで終了。ここでreturnせず下の開始判定へ落とすことで、
		// 先行入力が入っていれば"間を空けずに"次のステップへ繋がる
		m_isDodging = false;
	}

	// === 開始判定 ===
	// トリガーは右クリック(Accel)の押下＋接地。地上ダッシュの初動がそのまま回避になる形で、
	// 押し続けているとこのステップが終わったあとダッシュ(UpdateAccel/UpdateMove)へ流れる。
	// ※ 2026/07/20にShift("Dodge")から移した。無敵は反撃(ジャスト回避カウンター)の唯一の
	//   入口なので、トリガーを移してもここを消してはいけない
	// ※ 空中で押した時は回避せず、従来どおり空中ステップ／加速(UpdateAccel)に任せる
	// ※ 入力は上で先行入力タイマーに変換済み。押した瞬間もタイマーを張るので、
	//   ここは「押された or ステップ中に押されていた」をまとめて見ていることになる
	if (m_isDiving) { return; }                                      // 突撃中は回避しない
	if (m_dodgeCharges <= 0) { return; }                             // ストックを使い切っている
	if (!m_isGrounded) { return; }                                   // 空中は空中ステップ/加速の担当
	if (m_dodgeBufferTimer <= 0.0f) { return; }                      // 押していない(先行入力も切れている)

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

	--m_dodgeCharges;

	// ステップしたので、回復までの待ちを測り直す(=最後にステップしてからの経過時間)
	m_dodgeRechargeTimer = DebugParams::Instance().Float(U8("回避/再充填時間"), 0.7f, 0.05f, 5.0f);

	// 先行入力を消費する。猶予(0.2)がステップ時間(0.18)より長いので、
	// ここで消さないと1回押しただけで2回目が勝手に出てしまう
	m_dodgeBufferTimer = 0.0f;

	// 開始：無敵を張り、実行時間だけステップする
	m_isDodging       = true;
	m_dodgeTimer      = DebugParams::Instance().Float(U8("回避/時間"),     0.18f, 0.05f, 1.0f);
	m_invincibleTimer = DebugParams::Instance().Float(U8("回避/無敵時間"), 0.2f,  0.0f,  1.0f);
}

int Player::GetMaxDodgeCharges() const
{
	return DebugParams::Instance().Int(U8("回避/ストック数"), 2, 1, 5);
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

			// 受付中は減速して次を狙う"間"を作る。
			// ※ 8.0 は0.5秒の受付窓でほぼ完全に停止する強さ(残り約2%)で、次の突撃が
			//    毎回ほぼ静止から始まっていた。勢いを繋ぐ方針にしたので 2.0 へ緩めた
			//    (0.5秒で約37%残る)。“間”が欲しければ上げる／流したければ0にする
			float windowDamp = DebugParams::Instance().Float(U8("連続攻撃/継続中の減速"), 2.0f, 0.0f, 30.0f);
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

			// 斬った直後は減速する(0=止まる/1=減速なし)。
			// ※ 0.4 は一撃ごとに6割を捨てる設定で、3連鎖すると 0.4^3 = 6% しか残らず
			//    「攻撃するたびに勢いがリセットされる」原因になっていた。
			//    勢いを繋ぐ方針にしたので 0.85(=一撃あたり15%減)へ緩めた
			float slowRate = DebugParams::Instance().Float(U8("連続攻撃/斬り後の速度残し"), 0.85f, 0.0f, 1.0f);
			m_velocity *= slowRate;

			// 斬った対象を解除し、次の突撃を受け付ける窓を開く(この間にキーを押せば継続突撃)
			m_wpDiveTarget.reset();
			m_comboWindowTimer = DebugParams::Instance().Float(U8("連続攻撃/継続受付時間"), 0.5f, 0.05f, 2.0f);
			return;
		}

		to /= dist;
		// リールで引かれるように、速さを加速でrampしつつ常に対象へまっすぐ向ける。
		// 上限は「引き寄せ上限速度」と「この攻撃に入った時の速さ」の大きい方にする。
		// 一律に45で頭打ちにすると、ワイヤーで勢いを付けて突っ込んでも毎回45まで
		// 落とされて“攻撃のたびにリセット”される(ユーザー指摘 2026/07/20)
		float cap = (m_diveEntrySpeed > pullMax) ? m_diveEntrySpeed : pullMax;
		float sp = m_velocity.Length() + pullAccel * dt;
		if (sp > cap)
		{
			sp = cap;
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

	// 壁を走りながらでも攻撃に移れるように、壁走りは中断する(重力も戻る)
	m_upWall->Cancel(*this);

	m_isDiving = true;
	m_comboWindowTimer = 0.0f;

	// この攻撃に入った時の速さを覚えておく。チェインが続く間はリセットしないので、
	// 勢いを付けて始めた連続攻撃は最後まで速いまま繋がる
	m_diveEntrySpeed = m_velocity.Length();

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
	if (m_upWall)
	{
		m_upWall->Cancel(*this);   // 壁走り中にリセットしても重力が止まったままにならないように
	}
	m_isDiving = false;
	m_wpDiveTarget.reset();
	m_diveChainCount = 0;
	m_comboWindowTimer = 0.0f;
	Application::Instance().SetTimeScale(1.0f);   // スローを解除(スロー中に落下リセットしても等速へ)
	m_isDodging = false;
	m_dodgeTimer = 0.0f;
	m_invincibleTimer = 0.0f;
	m_isSprinting = false;
	m_accelPressWasGround = false;
	m_dodgeCharges = GetMaxDodgeCharges();   // ステップのストックは満タンで再開する
	m_dodgeRechargeTimer = 0.0f;
	m_dodgeBufferTimer = 0.0f;               // 先行入力を持ち越さない
}

void Player::PostUpdate()
{
	// 速度の上限を掛ける(2026/07/20 追加)。
	// ワイヤーの巻き取り・重力・加速・離脱ブーストがどれも速度を足すだけで、
	// 減らす仕組みが無かったため、スイングを繋ぐほど際限なく速くなっていた。
	// 経路(ワイヤー中/通常)によってUpdateの通り道が変わるので、
	// 毎フレーム必ず走るPostUpdateで一括して抑える
	ClampSpeed();

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

	// 着地した瞬間を捉えて、着地モーションを流す時間を確保する。
	// 「接地しているか」だけで判定すると、着地の次のフレームには走り/待機へ移ってしまい
	// 着地モーションがほぼ見えないため、瞬間にタイマーを立ててその間だけ再生する
	if (IsGrounded() && !m_wasGroundedForAnim)
	{
		m_landingAnimTimer = DebugParams::Instance().Float(U8("アニメ/着地モーションの長さ"), 0.25f, 0.0f, 1.0f);
	}
	m_wasGroundedForAnim = IsGrounded();

	if (m_landingAnimTimer > 0.0f)
	{
		m_landingAnimTimer -= Application::Instance().GetDeltaTime();
	}

	// アニメーションを進める(2026/07/20 追加)。
	// 接地・速度・突撃などの状態が全て確定したあとで呼ぶので、PostUpdateの最後に置く
	UpdateAnimation();

	// デバッグ用：状態値をDebugWatchへ(このフレームの最終状態を出す。PostUpdateは毎フレーム必ず走る)
	WatchDebug();
}

std::string Player::SelectAnimation() const
{
	// 上から順に「より特殊な状態」を見て、最初に当てはまったものを再生する。
	// ※ Scifi_girlの13本には ワイヤー・壁走り・斬撃 の専用モーションが無いので、
	//    近いモーションで暫定的に代用している。最終的には手付けの静止ポーズを
	//    用意して差し替える想定(モデルを差し替える時に一緒に作る)

	// 被弾硬直。空中で食らったか地上かで分ける
	if (m_staggerTimer > 0.0f)
	{
		if (IsGrounded()) { return "12 hurt"; }
		return "11 hurt (air)";
	}

	// ワイヤーでぶら下がっている間／突撃中。専用が無いので落下で代用
	if (m_upWire && m_upWire->IsAttached()) { return "08 fall (air)"; }
	if (m_isDiving) { return "08 fall (air)"; }

	// 壁走り・よじ登り。走りで代用
	if (m_upWall && (m_upWall->IsRunning() || m_upWall->IsClimbing())) { return "03 run"; }

	// ステップ(回避)。踏み込みの出だしが近い「加速」で代用
	if (m_isDodging) { return "02 speed up"; }

	// 空中
	if (!IsGrounded()) { return "08 fall (air)"; }

	// 着地の余韻(PostUpdateでタイマーを立てている)
	if (m_landingAnimTimer > 0.0f) { return "10 fall (landing)"; }

	// 接地：動いていれば走り、止まっていれば待機
	float runThreshold = DebugParams::Instance().Float(U8("アニメ/走りに切り替わる速さ"), 0.5f, 0.0f, 5.0f);
	if (GetHorizontalSpeed() > runThreshold) { return "03 run"; }

	return "01 idle";
}

float Player::SelectAnimationSpeed() const
{
	// 走り以外は等速。走りだけ、実際の水平速度に比例させて再生を速める。
	// 歩き(5.0)もダッシュ(11.0)も同じ速度で流すと足が地面を滑って見えるため
	if (m_currentAnimName != "03 run") { return 1.0f; }

	float baseSpeed = DebugParams::Instance().Float(U8("アニメ/走りの基準速度"), 5.0f, 1.0f, 20.0f);
	if (baseSpeed <= 0.0f) { return 1.0f; }

	// 上下に振り切れると不自然なので倍率を制限する
	return std::clamp(GetHorizontalSpeed() / baseSpeed, 0.5f, 2.5f);
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
	w.Watch(U8("Player/速度"),       m_velocity.Length());   // 上限の調整用(プレイヤー/最高速度と見比べる)
	w.Watch(U8("Player/水平速度"),   GetHorizontalSpeed());
	w.Watch(U8("Player/垂直速度"),   m_velocity.y);
	w.Watch(U8("Player/接地"),       IsGrounded());
	w.Watch(U8("Player/ワイヤー接続"), m_upWire && m_upWire->IsAttached());

	// 再生中のアニメ名。空のままなら「名前が見つかっていない」= glTFのアニメ名との
	// 綴り違いを疑う(Scifi_girlは "01 idle" のように番号＋半角スペース＋英字)
	w.Watch(U8("Player/アニメ"), GetCurrentAnimName());

	// 地上ダッシュまわり(右クリック押下でステップ=回避、押しっぱなしでダッシュ)。
	// ステップが出ない時は「ステップの残り」が0になっていないかを見る
	w.Watch(U8("Player/ダッシュ中"),     m_isSprinting);
	w.Watch(U8("Player/ステップ中"),     m_isDodging);
	w.Watch(U8("Player/ステップの残り"),   m_dodgeCharges);
	w.Watch(U8("Player/ステップ再充填"),   m_dodgeRechargeTimer);
	w.Watch(U8("Player/ステップ先行入力"), m_dodgeBufferTimer);

	// 壁走りまわり(発動条件の調整用。壁に触れているのに走り出さない時は
	// 「壁の接触」がtrueで「壁走り中」がfalseのまま＝速度不足を疑う)
	w.Watch(U8("Player/壁の接触"), m_isTouchingWall);
	w.Watch(U8("Player/壁走り中"), m_upWall && m_upWall->IsRunning());
	w.Watch(U8("Player/よじ登り中"), m_upWall && m_upWall->IsClimbing());

	// 攻撃・突撃まわり
	w.Watch(U8("Player/突撃中"),          m_isDiving);
	w.Watch(U8("Player/チェイン数"),      m_diveChainCount);
	w.Watch(U8("Player/突撃の持ち込み速度"), m_diveEntrySpeed);   // 突撃の上限がこれ以上に保たれる
	w.Watch(U8("Player/連続攻撃の受付窓"), m_comboWindowTimer);
	w.Watch(U8("Player/フォーカスゲージ"), m_focusGauge);

	// クールタイム・猶予窓まわり
	// ※ 回避のクールダウンはストック制に変えたので上の「ステップの残り/再充填」を見る
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
