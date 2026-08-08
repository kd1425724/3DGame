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
#include "../Enemy/Enemy.h"   // ロックオン中の関節を狙う(GetJointSphereAt/ApplyJointDamage)
#include "../../../Collision/CollisionGrid.h"   // IsWallBetween(落下攻撃の突撃先が壁の裏か)
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Debug/DebugDraw/DebugDraw.h"   // デバッグ表示のカテゴリ判定

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
	// スキンメッシュのキャラ(自作リグ22ボーン)。
	// ※ 2026/07/21 時点でアニメは「01 idle」「03 run」の2本だけで、中身はどちらも同じ
	//    ダッシュのモーション。他の名前は見つからないが、UpdateAnimationが
	//    「見つからなければ前のアニメを流し続ける」ので止まらない(暫定の見た目確認用)
	SetAsset("Asset/Models/Character/GogglesChara/GogglesChara.gltf");

	// 当たり判定と描画位置をモデルの実寸に合わせる(2026/07/20)。
	// glTFの頂点実測でY=0〜1.8989＝原点が足元・身長1.899m。
	// これを設定しないと「1辺1mの立方体」前提のままになり、足元が半身ぶん浮く
	m_bodyHeight = 1.899f;
	m_modelOriginIsFeet = true;

	// このモデルの正面は -Z(Scifi_girlと同じ)。
	// ※ 当初「Blenderの正面が-Yだから glTFでは+Z」と推論して false にしたが、
	//    実機で見たら後ろ向きに走っていたので true が正しい。軸変換の推論は当てにならず、
	//    結局この値は実機で見て決めるしかない(モデルを差し替えるたびに確認すること)
	m_modelForwardIsMinusZ = true;

	// 色はbase_color.jpgのテクスチャで付くので、色の乗算は白(=素の色)にする
	m_color = Math::Color(1.0f, 1.0f, 1.0f, 1.0f);

	SetScale(Math::Vector3(1.0f, 1.0f, 1.0f));

	//ワイヤー(物理＋見た目を内包。見た目の板ポリ生成はWireActionのctorが行う)
	// 立体機動装置に合わせて腰の左右から2本ぶん用意する
	for (std::unique_ptr<WireAction>& w : m_upWires)
	{
		w = std::make_unique<WireAction>();
	}

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

	// 攻撃(右クリック)。ワイヤーとは独立した入力になった(2026/08/02 入力のAoT2化)
	UpdateAttackInput();

	// 発射したフックの飛行を進める(着弾したらここで拘束が始まる)。
	// ※ 下の「ワイヤー中か」の分岐より前に置くこと。後ろに置くと、着弾したフレームの
	//   スイングが1フレーム遅れる。またWireAction::UpdateSwingAllは繋がっている
	//   ワイヤーが無いと即returnするので、飛行の進行をそこに混ぜることはできない
	for (int i = 0; i < kWireCount; ++i)
	{
		const std::unique_ptr<WireAction>& w = m_upWires[i];
		if (!w) { continue; }

		// 射出口を渡すのは、巻き戻し中のフックが「今の」手元へ帰ってくるようにするため
		if (w->UpdateHookMotion(GetPos(), GetWireMuzzlePos(i), dt))
		{
			SpawnWireImpactFx(*w);
		}
	}

	// 加速/空中ステップ(右クリック)。ワイヤー中でも使えるよう、ワイヤー分岐より前で処理する
	UpdateAccel(dt);

	// ワイヤー接続中はスイング物理だけ行い、通常移動・ジャンプ・レーザーは止める。
	// 移動入力をそのまま渡す：X=操舵(振り子の向きを曲げる) / Y=前方への漕ぎ。
	// 加えて上下の噴射(立体機動のガス噴射にあたる)を Space=上 / Ctrl=下 で渡す。
	// ※ ワイヤー中はジャンプが使われないのでSpaceを転用している
	// 実際の移動はWireAction::UpdateSwingAllがこのキャラを動かす
	if (IsAnyWireAttached())
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
			Math::Vector3 horiz = MathAPI::FlattenY(m_velocity);
			if (MathAPI::TryNormalize(horiz))
			{
				SpawnBoostFx(horiz * wireMove.y, dt);
			}
		}

		// 繋がっている全ワイヤーをまとめて解く。重力・積分・当たり解決は
		// この中で1フレームに1回だけ行われる(本数ぶん重複しない)
		WireAction* wires[kWireCount] = {};
		for (int i = 0; i < kWireCount; ++i)
		{
			wires[i] = m_upWires[i].get();
		}
		WireAction::UpdateSwingAll(*this, dt, wireMove, wires, kWireCount);
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

void Player::UpdateAttackInput()
{
	// === 右クリック＝3段階（射出 → 突撃 → 斬撃） ===
	//
	//   1回目 … 的(ロック中の関節)へアンカーを射出する。突撃はしない
	//   2回目 … その的へ突撃する(引き寄せ)。まだ斬らない
	//   3回目 … 突撃中に押すと斬る。【押した時の間合いでクリティカルが決まる】
	//
	// 【なぜ3段階か】斬撃を自動命中にすると、当たるかどうかがプレイヤーの操作と無関係になる。
	// 最後の一撃を自分で入れさせ、間合いで威力が変わるようにすることで、
	// 上手さがそのまま結果に出る(ユーザーの設計)。
	//
	// ※ 連続攻撃の受付中の押下は UpdateDive 側が持っているので、ここでは扱わない
	if (m_comboWindowTimer > 0.0f) { return; }
	if (!KdInputManager::Instance().IsPress("Attack")) { return; }

	// 3回目＝突撃中に押した → 斬る(ロック解除後でも突撃中なら斬れる)
	if (m_isDiving)
	{
		PerformDiveSlash();
		return;
	}

	// ここから先はロックオンしていて対象がいる時だけ効く
	if (!IsAttackInput()) { return; }

	// 2回目＝既に的へ掛かっている(飛行中も含む) → 突撃
	if (IsAnyJointWireActive())
	{
		StartDive();
		return;
	}

	// 1回目＝的へアンカーを射出する(突撃はしない)
	ShootJointWire();
}

float Player::GetCriticalRange() const
{
	// 【1箇所にまとめる理由】斬撃の判定(PerformDiveSlash / UpdateDiveの先行入力)と
	// デバッグ表示の両方が読む。別々に書くと既定値が食い違ったとき、
	// 先に呼ばれたほうが黙って勝つ(敵の移動速度で同じ形の問題を踏んでいる)
	//
	// 🔴 【2026/08/02】「斬撃範囲」を廃止し、間合いの基準はこれ1つにした。
	//   空振りをやめて先行入力にした時点で、斬撃範囲の内で押しても外で押しても
	//   結果は同じ「通常ヒット」になり、あの境界は何も決めていない値になっていた。
	//   残すのは「ここで押せたらクリティカル」という1本の線だけでよい。
	//
	// 🔴 距離(m)ではなく【狙っている関節の球の半径に対する倍率】で持つ。
	//   身長25mのゴーレムだと関節の球は半径2m前後あり、固定の距離では
	//   敵の大きさを変えるたびに手触りが変わってしまう
	float radius = 1.0f;
	if (Enemy* pEnemy = GetLockedEnemy())
	{
		Math::Vector3 center{};
		float r = 0.0f;
		if (pEnemy->GetJointSphereAt(m_lockedJointIndex, center, r))
		{
			radius = r;
		}
	}

	return radius * DebugParams::Instance().Float(U8("突撃/クリティカル範囲の倍率"), 3.0f, 0.1f, 8.0f);
}

void Player::PerformDiveSlash()
{
	std::shared_ptr<KdGameObject> spTarget = m_wpDiveTarget.lock();
	if (!spTarget) { return; }

	// 狙い先は UpdateDive と同じ求め方にする(ずれると判定と見た目が食い違う)
	Math::Vector3 aim{};
	if (!GetLockedJointPos(aim))
	{
		aim = spTarget->GetPos() + Math::Vector3(0.0f, 0.5f, 0.0f);
	}

	const float dist = Math::Vector3::Distance(aim, GetPos());

	// まだ間合いの外＝先行入力として覚えておき、届いた瞬間に【通常】で出す。
	//
	// 【なぜ空振りにしないか】外して何も起きないと「押せていないのか外したのか」が
	// プレイヤーに区別できず、ただの罰になる(ユーザー判断で空振りは廃止)。
	// 早く押しても攻撃は当たるが【クリティカルにはならない】ようにすることで、
	// 「間合いを見て押す」ことの価値だけを残している
	if (dist > GetCriticalRange())
	{
		m_slashBuffered = true;
		return;
	}

	// 間合いの中で押せた＝クリティカル
	ExecuteSlash(aim, true);
}

void Player::ExecuteSlash(const Math::Vector3& _aim, bool _isCritical)
{
	std::shared_ptr<KdGameObject> spTarget = m_wpDiveTarget.lock();
	if (!spTarget) { return; }

	m_slashBuffered = false;   // 出したので先行入力は消費する

	float damage = Enemy::GetAttackPower();
	if (_isCritical)
	{
		damage *= DebugParams::Instance().Float(U8("突撃/クリティカル倍率"), 2.0f, 1.0f, 10.0f);
	}

	// 関節を狙っていればその関節へ、そうでなければ本体へダメージが入る
	if (Enemy* pEnemy = GetLockedEnemy())
	{
		pEnemy->ApplyJointDamage(m_lockedJointIndex, damage);
	}
	else
	{
		spTarget->OnHit(this);
	}

	m_diveChainCount++;

	// クリティカルは手応えを強くする(揺れで成否が体で分かるようにする)
	float trauma = std::clamp(0.2f + 0.05f * m_diveChainCount, 0.0f, 0.7f);
	if (_isCritical)
	{
		trauma = std::clamp(trauma * 1.6f, 0.0f, 1.0f);
	}
	CameraShake::Instance().AddTrauma(trauma);

	EffectManager::Instance().SpawnSlash(_aim);   // 斬った位置に斬撃エフェクト

	// 斬った直後は減速する(0=止まる/1=減速なし)。
	// ※ 0.4 は一撃ごとに6割を捨てる設定で、3連鎖すると 0.4^3 = 6% しか残らず
	//    「攻撃するたびに勢いがリセットされる」原因になっていた。
	//    勢いを繋ぐ方針にしたので 0.85(=一撃あたり15%減)へ緩めた
	float slowRate = DebugParams::Instance().Float(U8("連続攻撃/斬り後の速度残し"), 0.85f, 0.0f, 1.0f);
	m_velocity *= slowRate;

	// 斬った対象を解除し、次の突撃を受け付ける窓を開く(この間に押せば継続突撃)
	m_wpDiveTarget.reset();
	m_comboWindowTimer = DebugParams::Instance().Float(U8("連続攻撃/継続受付時間"), 0.5f, 0.05f, 2.0f);
}

bool Player::IsAnyJointWireActive() const
{
	for (const std::unique_ptr<WireAction>& w : m_upWires)
	{
		if (!w) { continue; }

		// 飛行中も「もう撃った」とみなす。でないと着弾までの間に押した2回目が
		// また射出になってしまい、連打で永久に攻撃できない
		if (w->IsJointAnchor() && (w->IsAttached() || w->IsFlying())) { return true; }
	}

	return false;
}

void Player::ShootJointWire()
{
	Enemy* pEnemy = GetLockedEnemy();
	if (!pEnemy) { return; }

	Math::Vector3 jointPos{};
	if (!GetLockedJointPos(jointPos)) { return; }

	std::shared_ptr<KdGameObject> spTarget = m_upTargeting->GetTarget();
	if (!spTarget) { return; }

	// 地形へ掛かっているワイヤーは畳む。
	// 【なぜ混在させないか】2本掛けの合体は「支点を1回決めて固定する」設計で、
	// 動く関節アンカーと噛み合わない。掛け先は地形か関節かのどちらか一方にする
	ReleaseAllWires(false);

	// 関節へは1本だけ撃つ。2本を同じ点へ撃っても支点は同じで得るものが無く、
	// 合体の経路を通すぶん動くアンカーとの相性問題を抱え込むだけになる
	m_upWires[0]->ShootAtJoint(GetWireMuzzlePos(0), spTarget, m_lockedJointIndex, jointPos);
}

void Player::UpdateWireInput()
{
	// === アンカー射出（左クリック＝常にワイヤー） ===
	// 【2026/08/02 入力のAoT2化】以前は同じボタンで文脈により攻撃/ワイヤーが変わっていたが、
	// 進撃の巨人2に合わせて用途別に分けた。左は常にワイヤー、攻撃は右(UpdateAttackInput)。
	// これで「攻撃したいのにワイヤーが出た」「その逆」が起きなくなった
	if (KdInputManager::Instance().IsPress("Anchor"))
	{
		// 突撃中／連続攻撃の受付中はワイヤーを撃たせない。
		// StartDiveがワイヤーを畳む設計なので、突撃中に繋ぐと突撃が止まって固まる
		if (m_isDiving || m_comboWindowTimer > 0.0f)
		{
			m_anchorPressWasWire = false;
			return;
		}

		m_anchorPressWasWire = true;
	}

	if (KdInputManager::Instance().IsPress("Anchor") && m_anchorPressWasWire)
	{
		Math::Vector3 from = GetPos() + Math::Vector3(0, 1.0f, 0);   // 照準の基準(体の胸あたり)
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
		dir = MathAPI::GetSafeNormal(dir, Math::Vector3::Backward);

		// === 立体機動装置の2本掛け ===
		// 1回の入力で腰の左右から2本撃つ(進撃の巨人2も左右のフックを個別には操作しない)。
		// ただし「同じ照準点へ少し開いて撃つ」だけでは真横の壁に届かないので、
		// 各フックが自分の側を扇状に探して取り付ける面を見つける(FindAnchorDir)
		bool useTwo = DebugFlags::Instance().Get(U8("ワイヤー/2本掛け"), true);

		// 撃った瞬間、今の速度(m_velocity)はそのまま引き継ぐ
		// (走りながら撃てば横の勢いが乗る。速度は基底CharaBaseの共通m_velocity)
		// 前回の探索の軌跡は捨てる(デバッグ表示は「直近に撃った時」のものだけ出す)
		m_wireProbes.clear();

		int shots = useTwo ? kWireCount : 1;
		for (int i = 0; i < shots; ++i)
		{
			// 射出口はそれぞれの腰。見た目の線と物理の始点をそろえる
			Math::Vector3 muzzle = GetWireMuzzlePos(i);

			Math::Vector3 shotDir = dir;
			if (!FindAnchorDir(i, muzzle, dir, maxLen, shotDir))
			{
				// 扇の中に取り付けられる面が無ければ、そのフックは撃たない
				continue;
			}

			m_upWires[i]->Shoot(muzzle, shotDir, maxLen);
		}
	}

	// ワイヤーとして撃った時だけ、離したら外す(攻撃で押した時は無視)。
	// 離しても速度(m_velocity)はそのまま=スイングの勢いで飛んでいける(フリング)
	if (KdInputManager::Instance().IsRelease("Anchor"))
	{
		if (m_anchorPressWasWire)
		{
			// 自分でボタンを離した時だけ、フックが手元へ帰る見た目を出す
			// (自動リリースやリセットは即座に消す。→ WireAction::Release のコメント)
			ReleaseAllWires(true);
		}
		m_anchorPressWasWire = false;
	}
}

void Player::UpdateAccel(float dt)
{
	// 【2026/08/02 入力のAoT2化】右クリックからSpaceへ移した。Spaceは接地と空中で意味が変わる。
	//   接地 … ジャンプ(UpdateJumpの担当)。ここでは何もしない
	//   空中 … 単押しでステップ(UpdateDodgeの担当。無敵つき)／長押しでブースト(ここ)
	//          進撃の巨人2の×ボタン(加速・空中ステップ)にあたる、プレイヤー側の推進力。
	//          方向は移動入力(カメラ基準)、無入力なら進行方向
	//
	// 【なぜ空中は押下発火にしないのか】空中は長押しのブーストが主役なので、
	// 押下でステップを出すと、加速しようとするたび毎回ステップが暴発してしまう。
	// 単押しか長押しかを見分けてから出す必要があるので、ステップは離した時に出す
	const float tapTime = DebugParams::Instance().Float(U8("加速/単押しとみなす時間"), 0.18f, 0.05f, 0.6f);

	if (KdInputManager::Instance().IsPress("Jump"))
	{
		m_accelHoldTime = 0.0f;

		// 【重要】この押下が別の行動に消費されたなら、同じ押下をブースト/ステップに使わない。
		//   ・接地中     … ジャンプが出る。押しっぱなしのまま浮くとそのままブーストへ流れ、
		//                   「ジャンプすると必ず加速する」ことになる
		//   ・壁走り中   … 壁ジャンプが出る(WallAction)。放置すると壁ジャンプ直後に
		//                   空中ステップまで二重に出る
		// 離すまでこの押下を無視することで切り離す
		const bool onWall = m_upWall && (m_upWall->IsRunning() || m_upWall->IsClimbing());
		m_accelPressWasGround = m_isGrounded || onWall;
	}

	bool holding = KdInputManager::Instance().IsHold("Jump");
	if (holding)
	{
		m_accelHoldTime += dt;
	}

	// ※ 地上ダッシュ(m_isSprinting)は 2026/08/02 に廃止した(ユーザー指示)。
	//   「歩きを無くして最初からダッシュ」にしたので、速度を切り替える対象そのものが無くなった。
	//   地上の移動速度は UpdateMove が常にダッシュ速度で走らせる

	// 空中でのみブーストする。接地中の押下はジャンプなので、ここでは推進しない
	if (holding && !m_isGrounded && !m_accelPressWasGround)
	{
		// 単押し判定の時間を過ぎたら「長押し＝加速」に確定して、以降は加速し続ける
		if (m_accelHoldTime >= tapTime)
		{
			// 🔴 【罠】ここの「加速/上限速度」を上げても、それだけでは速くならない。
			//   ClampSpeed が速度全体を「プレイヤー/最高速度」(既定20)で頭打ちにしているので、
			//   35でも45でも結果は20で止まる。調整するときは必ず両方を見ること。
			//   ※ 2026/08/02に最高速度を35へ上げて試したが、ユーザー判断で20へ戻した。
			//     移動の速さではなく【ステップを強くする】方向で伸ばす方針
			float acc    = DebugParams::Instance().Float(U8("加速/加速度"),     60.0f, 0.0f, 150.0f);
			float maxSpd = DebugParams::Instance().Float(U8("加速/上限速度"),   35.0f, 0.0f, 120.0f);

			Math::Vector3 dir = GetAccelDir();
			if (dir.LengthSquared() > MathAPI::kSmallNumber && m_velocity.Length() < maxSpd)
			{
				m_velocity += dir * (acc * dt);
			}

			// 加速していることが一目で分かるよう、後方へ噴射エフェクトを出す
			SpawnBoostFx(dir, dt);
		}
	}

	// 短く離したら空中ステップ。実行(無敵・ストック・速度)は UpdateDodge が持っているので、
	// ここでは「単押しだった」ことを先行入力として渡すだけにする。
	// ※ 接地中に押し始めた場合は出さない。その押下はジャンプとして消費されているため
	if (KdInputManager::Instance().IsRelease("Jump"))
	{
		if (m_accelHoldTime < tapTime && !m_isGrounded && !m_accelPressWasGround)
		{
			m_dodgeBufferTimer = DebugParams::Instance().Float(U8("回避/先行入力"), 0.2f, 0.0f, 1.0f);
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

	// 🔴 ステップ中は通常の上限で削らない(2026/08/02)。
	// 移動の上限は「ワイヤーを繋ぐほど際限なく速くならない」ための値で、
	// 「一瞬だけ速い」ステップに掛けると行動の意味そのものが消える。
	// 実際、回避/速度22 に対して通常上限が20だったため、ステップは常に削られていて
	// 「ステップが弱い」という症状になっていた。上限はステップ速度そのものにする
	if (m_isDodging)
	{
		maxSpeed = std::max(maxSpeed, GetDodgeSpeed());
	}
	float maxFall  = DebugParams::Instance().Float(U8("プレイヤー/最大落下速度"), 60.0f, 5.0f, 200.0f);

	// 落下速度の頭打ち
	if (m_velocity.y < -maxFall)
	{
		m_velocity.y = -maxFall;
	}

	// 全体の速さを上限で抑える。向きは変えず大きさだけ縮める
	m_velocity = MathAPI::ClampMagnitude(m_velocity, maxSpeed);
}

void Player::SpawnBoostFx(const Math::Vector3& _dir, float _dt)
{
	if (_dir.LengthSquared() <= MathAPI::kSmallNumber) { return; }

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

void Player::SpawnWireImpactFx(const WireAction& _wire)
{
	// 粒はSpawnWallRun(壁を擦った火花)を流用する。
	// 【なぜ専用のエフェクトを作らないか】壁走りの火花と語彙を揃えたいのと、
	//   「硬いものに金属が当たった」という意味が同じなので、見た目を分ける理由が無い
	Math::Vector3 anchor = _wire.GetAnchor();

	// フックが進んだ向き。火花はこの逆へ流れる(SpawnWallRunが進行方向の逆へ飛ばす)
	Math::Vector3 flightDir = anchor - _wire.GetLaunchPos();
	if (!MathAPI::TryNormalize(flightDir))
	{
		flightDir = Math::Vector3::Backward;
	}

	// 面の法線は分からない(CastAnchorは交点しか返さない)ので、
	// 「アンカーからプレイヤーへ向かう向き」で代用する。ワイヤーはその方向から
	// 飛んできたので、面の外向きとおおよそ一致する。火花が壁にめり込まなければ充分
	Math::Vector3 outward = GetPos() - anchor;
	if (!MathAPI::TryNormalize(outward))
	{
		outward = Math::Vector3::Up;
	}

	int count = DebugParams::Instance().Int(U8("ワイヤー/着弾の火花の数"), 8, 0, 40);
	for (int i = 0; i < count; ++i)
	{
		EffectManager::Instance().SpawnWallRun(anchor, flightDir, outward);
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
	if (!MathAPI::TryNormalize(wishDir)) { return Math::Vector3::Zero; }

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

	if (axis.LengthSquared() > MathAPI::kSmallNumber)
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
		Math::Vector3 horiz = MathAPI::FlattenY(m_velocity);
		if (MathAPI::TryNormalize(horiz))
		{
			dir = horiz;
		}
		else if (std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock())
		{
			Math::Vector3 fwd = MathAPI::FlattenY(
				Math::Vector3::TransformNormal(Math::Vector3::Backward, spCamera->GetRotationMatrix()));
			if (MathAPI::TryNormalize(fwd))
			{
				dir = fwd;
			}
		}
	}

	// ブーストに上向き成分を混ぜる。
	//
	// 【2026/08/02 入力のAoT2化】以前は「Jumpを併用したら上向きを足す」だったが、
	// ブースト自体がSpaceへ移った＝押しているボタンが同じになったので、条件として成立しなくなった。
	// 上向きを足すか否かの選択肢が消えたので、常に一定量を混ぜる形に変えてある。
	// 混ぜないと水平にしか飛べず、ワイヤーを掛け直す高さを稼げない。
	// 上向きが不要なら「加速/上向きの割合」を0にすれば水平だけになる
	float upRate = DebugParams::Instance().Float(U8("加速/上向きの割合"), 0.8f, 0.0f, 2.0f);
	dir.y += upRate;
	MathAPI::TryNormalize(dir);

	return dir;
}

bool Player::IsAttackInput() const
{
	// ロックオン中で、かつターゲットがいる時だけ「攻撃」として扱う。
	// ※ 連続攻撃の受付中はロックしていなくても攻撃になるが(ユーザー指定)、
	//    その分岐は UpdateWireInput の入口で先に処理している(UpdateDiveへ渡す)
	// ※ 以前は IsHold("Focus")(押しっぱなし)だった。2026/08/02にトグルへ変更
	if (!m_isLockedOn) { return false; }
	if (!m_upTargeting) { return false; }

	return m_upTargeting->GetTarget() != nullptr;
}

void Player::UpdateLockOnToggle()
{
	// Eを押すたびにロックのON/OFFを切り替える
	if (!KdInputManager::Instance().IsPress("Focus")) { return; }

	m_isLockedOn = !m_isLockedOn;

	// 掛け直したら狙いは首(添字0)から。前の対象で選んでいた関節を引きずらない
	if (m_isLockedOn)
	{
		m_lockedJointIndex = 0;
	}
}

void Player::UpdateLockOnSelection()
{
	if (!m_isLockedOn) { return; }

	// 対象が消えた(倒した/範囲外)ならロックを解く。
	// ※ ロック中はTargetingが対象を選び直さないので、ここで解かないと掛かったままになる
	// ※ この判定は【Targetingの更新より後】でなければならない。前に置くと、
	//   Eを押した最初のフレームはまだ対象が居ないので、その場でロックが解けてしまう
	if (!m_upTargeting || !m_upTargeting->GetTarget())
	{
		m_isLockedOn = false;
		return;
	}

	// ホイールで狙う関節を切り替える。
	// WM_MOUSEWHEELの値は120刻みなので、符号だけ見て1段ずつ送る
	// (刻み幅に依存させると、高解像度ホイールのマウスで飛び方が変わる)
	const int wheel = Application::Instance().GetMouseWheelValue();
	if (wheel > 0)
	{
		CycleLockedJoint(1);
	}
	else if (wheel < 0)
	{
		CycleLockedJoint(-1);
	}

	// 今狙っている関節が壊れたら、生きている関節へ寄せる
	Enemy* pEnemy = GetLockedEnemy();
	if (pEnemy && !pEnemy->IsJointAlive(m_lockedJointIndex))
	{
		CycleLockedJoint(1);
	}
}

Enemy* Player::GetLockedEnemy() const
{
	if (!m_isLockedOn) { return nullptr; }
	if (!m_upTargeting) { return nullptr; }

	std::shared_ptr<KdGameObject> spTarget = m_upTargeting->GetTarget();
	if (!spTarget) { return nullptr; }

	// dynamic_castの代わりにタグで判定する(この作品の既定の見分け方)
	if (spTarget->GetObjectTag() != ObjectTag::Enemy) { return nullptr; }

	return static_cast<Enemy*>(spTarget.get());
}

void Player::CycleLockedJoint(int _step)
{
	Enemy* pEnemy = GetLockedEnemy();
	if (!pEnemy) { return; }

	// 壊れた関節は飛ばす。全部壊れていたら添字を動かさない(1周して戻る)
	for (int i = 0; i < Enemy::kJointCount; ++i)
	{
		m_lockedJointIndex = (m_lockedJointIndex + _step + Enemy::kJointCount) % Enemy::kJointCount;

		if (pEnemy->IsJointAlive(m_lockedJointIndex)) { return; }
	}
}

bool Player::GetLockedJointPos(Math::Vector3& _outPos) const
{
	Enemy* pEnemy = GetLockedEnemy();
	if (!pEnemy) { return false; }

	float radius = 0.0f;
	return pEnemy->GetJointSphereAt(m_lockedJointIndex, _outPos, radius);
}

void Player::UpdateMove(float dt)
{
	// === 通常移動(velocityベース。接地=キビキビ、空中=勢いを保つ) ===
	Math::Vector3 wishDir = GetWishDir();

	// 【2026/08/02 入力のAoT2化】歩きを廃止し、最初からダッシュ速度で動く(ユーザー指示)。
	// 以前は「歩き5.0 / 押しっぱなしでダッシュ11.0」を m_isSprinting で切り替えていたが、
	// 切り替えるボタン(旧Accel=右クリック)が攻撃になったので、遅いほうを捨てた。
	// ※ 旧キー「プレイヤー/移動速度」はもう読まれない(JSONには残る)
	float moveSpeed = DebugParams::Instance().Float(U8("プレイヤー/ダッシュ速度"), 11.0f, 0.0f, 40.0f);
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

		Math::Vector3 horiz = MathAPI::FlattenY(m_velocity);
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

	// ※ 先行入力を張るのは UpdateAccel の「空中でSpaceを短く離した」判定に移した
	//   (2026/08/02 入力のAoT2化)。押下では張らない：接地中の押下はジャンプだから

	// === 回避ダッシュ実行中：水平にフラットに素早く移動(縦は止めて空中でもキレよく) ===
	if (m_isDodging)
	{
		float dodgeSpeed = GetDodgeSpeed();
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
	// トリガーは【空中でSpaceを短く離した】こと(UpdateAccelが先行入力に変換して渡す)。
	//
	// 🔴 【2026/08/02 入力のAoT2化】地上ステップを廃止し、空中ステップへ一本化した。
	//   ユーザー指示「歩きを廃止、デフォルトでダッシュにしてダッシュ、ステップを無くす」。
	//   **無敵(iフレーム)は反撃(ジャスト回避カウンター)の唯一の入口なので、
	//   地上ステップを消すぶんの無敵をここ(空中ステップ)へ引き継いである。**
	//   これを消すと反撃システムに入る手段が無くなる
	if (m_isDiving) { return; }                                      // 突撃中は回避しない
	if (m_dodgeCharges <= 0) { return; }                             // ストックを使い切っている
	if (m_isGrounded) { return; }                                    // 接地中の押下はジャンプ
	if (m_dodgeBufferTimer <= 0.0f) { return; }                      // 単押しが来ていない(先行入力も切れている)

	// 方向：移動入力があればその向き(カメラ基準)、無ければカメラ前方(水平)
	Math::Vector2 moveAxis = KdInputManager::Instance().GetAxisState("Move");
	Math::Vector3 dir = Math::Vector3::Backward * moveAxis.y + Math::Vector3::Right * moveAxis.x;
	if (dir.LengthSquared() <= MathAPI::kSmallNumber)
	{
		dir = Math::Vector3::Backward;
	}   // 入力なし→前方へ
	if (std::shared_ptr<CameraBase> spCam = m_wpCamera.lock())
	{
		dir = Math::Vector3::TransformNormal(dir, spCam->GetRotationYMatrix());
	}
	dir = MathAPI::GetSafeNormalXZ(dir, Math::Vector3::Backward);
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

	// ステップは一瞬なので、まとめて数粒出して"バッ"と見せる
	// (空中ステップが持っていた見た目を、こちらへ引き継いだ)
	int burst = DebugParams::Instance().Int(U8("加速エフェクト/ステップの粒数"), 6, 0, 30);
	for (int i = 0; i < burst; ++i)
	{
		EffectManager::Instance().SpawnBoost(GetBoostSpawnPos(m_dodgeDir), m_dodgeDir);
	}
}

float Player::GetDodgeSpeed() const
{
	// 【1箇所にまとめる理由】ステップの実行(UpdateDodge)と、その間だけ速度上限を
	// 緩めるClampSpeedの両方が読む。別々に書くと既定値が食い違ったとき、
	// 上限のほうが低ければステップが黙って削られる(実際にその状態だった)
	return DebugParams::Instance().Float(U8("回避/速度"), 22.0f, 5.0f, 80.0f);
}

int Player::GetMaxDodgeCharges() const
{
	return DebugParams::Instance().Int(U8("回避/ストック数"), 2, 1, 5);
}

void Player::UpdateAirFocus()
{
	// 【2026/08/02】空中スローを既定OFFにした(ユーザー指示「一旦切って。多分消す」)。
	//   ロックオンをトグル化した結果、Eは「タップ＝ロック」になったのに
	//   スローだけ「長押し」のまま残り、1つのキーに2つの意味が乗っていた。
	//   消すかどうかは未確定なのでフラグで殺すに留める。消すと決まったらこの関数ごと撤去する
	if (!DebugFlags::Instance().Get(U8("プレイヤー/空中スローを使う"), false))
	{
		// 掛けっぱなしで抜けないよう、等速へ戻してからreturnする
		Application::Instance().SetTimeScale(1.0f);
		return;
	}

	float maxGauge = DebugParams::Instance().Float(U8("空中スロー/最大時間"), 1.5f, 0.2f, 5.0f);
	float slowVal  = DebugParams::Instance().Float(U8("空中スロー/遅さ"),     0.3f, 0.05f, 1.0f);
	float refill   = DebugParams::Instance().Float(U8("空中スロー/回復速度"), 0.5f, 0.0f, 3.0f);

	// フォーカスゲージは"現実の時間"で増減させる(スローで薄まらないように実時間dtを使う)
	float realDt = Application::Instance().GetRealDeltaTime();

	// スロー可能：空中(ワイヤー未接続) && E(Focus)長押し && 突撃していない && ゲージ残
	//   ※ スローは初弾を"ためて狙う"時だけ。連続攻撃中(突撃/受付窓)はスローしない
	//   ※ 2026/07/19の入力再設計で、長押し元を左クリックからE(Focus)へ移した。
	//     左クリックはアンカー射出＋攻撃に統一したため
	bool airborne = !m_isGrounded && !IsAnyWireAttached();
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
			// ※ 2026/08/02の入力のAoT2化で、攻撃ボタンが左クリック→右クリックへ移った
			if (KdInputManager::Instance().IsPress("Attack"))
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

		// 対象がいない(受付窓もない)＝突撃を続ける相手がいないので終わる。
		// ※ 以前はここから「真下ダイブ(落下攻撃)」へ落ちていたが、2026/08/02に撤去した
		if (!spTarget)
		{
			m_isDiving = false;
			return;
		}

		float pullAccel = DebugParams::Instance().Float(U8("落下攻撃/引き寄せ加速"),     80.0f, 5.0f, 300.0f);
		float pullMax   = DebugParams::Instance().Float(U8("落下攻撃/引き寄せ上限速度"), 45.0f, 5.0f, 150.0f);

		// 狙い先。ロックオンで関節を選んでいればその関節、そうでなければ従来どおり胴の高さ。
		// 対象も関節も動くので毎フレーム狙い直す(ホーミング)
		// ※ 敵の中心へ飛ばすと、身長25mでは体の内側に入ってしまい部位を狙い分けられない
		Math::Vector3 aim{};
		if (!GetLockedJointPos(aim))
		{
			aim = spTarget->GetPos() + Math::Vector3(0.0f, 0.5f, 0.0f);
		}

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

		// 今どの間合いにいるかを出す。斬るタイミングを自分で計る操作にしたので、
		// 「どこからがクリティカルか」を数字で見られないと調整のしようがない
		// (ここで読むことでDebugParamsのキーが起動直後から一覧に並ぶ効果もある)
		{
			const float critRange = GetCriticalRange();

			DebugWatch& w = DebugWatch::Instance();
			w.Watch(U8("Player/的までの距離"),        dist);
			w.Watch(U8("Player/クリティカル範囲(m)"), critRange);
			w.Watch(U8("Player/クリティカル圏内"),    dist <= critRange);
		}

		// 🔴 【2026/08/02】ここにあった「近づいたら自動で斬る」は撤去した(ユーザーの設計)。
		// 斬るのは右クリックの3回目＝PerformDiveSlash が行い、押した時の間合いで
		// クリティカルかどうかが決まる。自動命中だと当たるかどうかが操作と無関係になり、
		// 「タイミングを計る上手さ」を出す余地が無くなるため。
		//
		// そのぶん「通り過ぎたら終わり」をここで見る。これが無いと的の上で
		// 引き寄せられ続けて永久に振り回されることになる
		const float critRange = GetCriticalRange();

		// 先行入力が入っていて、間合いに入ったら【通常】で斬る。
		// 早押しは当たるがクリティカルにはならない、というのがここの肝
		if (m_slashBuffered && dist <= critRange)
		{
			ExecuteSlash(aim, false);
			return;
		}

		// 通過とみなす距離も同じ基準から作る。
		// 【必ずクリティカル範囲より内側にすること】外側にすると、押していないのに
		// 間合いへ入る前に突撃が終わってしまい、斬る機会そのものが無くなる
		float passRange = critRange * DebugParams::Instance().Float(U8("突撃/通過とみなす割合"), 0.25f, 0.05f, 0.9f);

		if (dist <= passRange)
		{
			// 斬らずに到達した＝空振り。勢いは残したまま突撃だけ終える
			m_isDiving = false;
			m_wpDiveTarget.reset();
			m_comboWindowTimer = 0.0f;
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

	// 飛行中／接続中のワイヤーは畳んでおく。
	// 【なぜ現状これが空振りでも入れておくのか】
	//   左クリックは「押す→離す→押す」の順しか踏めず、離した時点で
	//   UpdateWireInput が ReleaseAllWires を呼ぶので、ここに来る時点で
	//   ワイヤーは既に外れている。つまり今は到達しない。
	//   ただし「突撃中にワイヤーが着弾する」と、Update内でワイヤー分岐が
	//   UpdateDive より先に return するため突撃が止まったまま固まる。
	//   その不変条件を離れた場所(入力の押し離し順)に頼りたくないので、
	//   突撃を始める側で明示的に畳んでおく
	ReleaseAllWires();

	m_isDiving = true;
	m_comboWindowTimer = 0.0f;
	m_slashBuffered = false;   // 前の突撃の押しっぱなしを持ち越さない

	// この攻撃に入った時の速さを覚えておく。チェインが続く間はリセットしないので、
	// 勢いを付けて始めた連続攻撃は最後まで速いまま繋がる
	m_diveEntrySpeed = m_velocity.Length();

	// 対象へワイヤーで引き寄せる。以降UpdateDiveが引き寄せ、
	// ワイヤーの線はDrawWireが手元→対象に描く
	//
	// 🔴 【2026/08/02】対象がいない時の「真下ダイブ(落下攻撃)」は撤去した(ユーザー指示)。
	//   右クリックを3段階にした結果、突撃に入るには必ずロックオン中の的が要る
	//   (UpdateAttackInputがIsAttackInput()を通す)。つまりこの分岐は【到達できない】。
	//   反撃窓からの突撃も最寄りの敵を探してから入るので、対象なしでは始まらない
	m_wpDiveTarget = m_upTargeting->GetTarget();
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
	ReleaseAllWires();
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
	if (!IsAnyWireAttached())
	{
		GroundCheck();
	}

	// 突撃中に着地したとき：対象も継続受付窓も無いなら終了する。
	// 突撃中(対象あり)や継続受付中(窓あり=入力待ち)は着地しても打ち切らない
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

	// 照準：ロックオン中だけターゲットを取る(2026/07/19の入力再設計、2026/08/02にトグル化)。
	// 常時ロックだと移動中もマーカーが付きっぱなしになるため、「狙う」を明示的な操作にした。
	// 連続攻撃の受付中はロックしていなくてもターゲットを維持する(次の対象へ繋げるため)。
	// カメラを渡さずに呼ぶとTargeting側がターゲットを解除する
	//
	// ※ ロックのトグルはTargetingの更新より【前】。そのフレームのロック状態で対象を選ばせる
	UpdateLockOnToggle();

	bool wantTarget = m_isLockedOn || m_comboWindowTimer > 0.0f;

	// ロック中で【既に対象を掴んでいる】ときだけ選び直しを止める。
	// 掛けた最初のフレームは対象がまだ居ないので、ここは通常どおり探索させる
	bool keepCurrent = m_isLockedOn && m_upTargeting->GetTarget() != nullptr;

	m_upTargeting->Update(wantTarget ? m_wpCamera.lock() : nullptr,
		Application::Instance().GetDeltaTime(), keepCurrent);

	// 対象が確定した後にロックの後始末と関節の切り替えを行う
	UpdateLockOnSelection();

	// マーカーは狙っている関節に出す。関節が取れないとき(敵でない等)は敵の中心に出る
	Math::Vector3 jointPos{};
	const bool hasJoint = GetLockedJointPos(jointPos);
	if (hasJoint)
	{
		m_upTargeting->SetMarkerOverridePos(jointPos);
	}
	else
	{
		m_upTargeting->ClearMarkerOverridePos();
	}

	// カメラにも同じ点を渡して、ロックオン中はそこを注視させる。
	// 【なぜ「点」を渡すのか】狙っているのは敵そのものではなく関節(ホイールで切替)で、
	//   その位置を知っているのはここだけ。カメラに敵や関節を知らせずに済む
	// ※ 関節が取れないときはロックしない。マーカーも出ていない状態でカメラだけ
	//   固定されると、何を見ているのか分からないまま操作を奪うことになる
	if (std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock())
	{
		if (m_isLockedOn && hasJoint)
		{
			spCamera->SetLockOnAim(jointPos);
		}
		else
		{
			spCamera->ClearLockOnAim();
		}
	}

	// 着地した瞬間を捉えて、着地モーションを流す時間を確保する。
	// 「接地しているか」だけで判定すると、着地の次のフレームには走り/待機へ移ってしまい
	// 着地モーションがほぼ見えないため、瞬間にタイマーを立ててその間だけ再生する
	// ※ コヨーテタイム込みで見る。素のIsGroundedだと、段差で1フレーム浮くたびに
	//    ここが立ち上がりと判定されて着地モーション(1.73秒)が鳴り続ける
	if (IsGroundedOrCoyote() && !m_wasGroundedForAnim)
	{
		// クリップの実長(ローリングは52フレーム=約1.73秒)を入れておく。
		// 再生倍率で割るのは、速く再生するとそのぶん早く終わるため。
		// 割らないと、倍率を変えるたびにこの長さを手で直すことになる
		float len   = DebugParams::Instance().Float(U8("アニメ/着地モーションの長さ"), 1.73f, 0.0f, 3.0f);
		float scale = GetAnimSpeedScale(U8("アニメ/着地の再生速度"));
		m_landingAnimTimer = (scale > 0.0f) ? (len / scale) : len;
	}
	m_wasGroundedForAnim = IsGroundedOrCoyote();   // 立ち上がり検出と同じ基準で持つ

	if (m_landingAnimTimer > 0.0f)
	{
		m_landingAnimTimer -= Application::Instance().GetDeltaTime();
	}

	// 体を進行方向へ向ける(2026/07/21 追加)。
	// 向きは「速度が確定したあとの結果」なのでアニメと同じ位置で呼ぶ。
	// ※ プレイヤーの当たり判定はGetPos()から組むray/sphereで、m_pColliderを登録していないため
	//    m_rot(=m_mWorld)を回しても当たりには影響しない。カメラもGetPos()しか見ていない
	UpdateFacing(Application::Instance().GetDeltaTime());

	//体を傾ける（ワイヤーで振られている感じを出す）
	//向きが決まった後によぶ
	//傾きはローカル軸で効くので、先にヨーが確定している必要がある
	UpdateTilt(Application::Instance().GetDeltaTime());

	//斜面に合わせて体を傾ける(片足が埋まり片方が浮くのを減らす)
	//UpdateTiltと同じくローカル軸で効くので、ヨーが確定したこの位置で呼ぶ
	UpdateSlopeAlign(Application::Instance().GetDeltaTime());

	// アニメーションを進める(2026/07/20 追加)。
	// 接地・速度・突撃などの状態が全て確定したあとで呼ぶので、PostUpdateの最後に置く
	UpdateAnimation();

	//腕の位置再計算
	UpdateArmAim(Application::Instance().GetDeltaTime());

	//脚を慣性でなびかせる(空中で振られると後ろへ流れる)
	//腕の後に呼ぶ＝アニメ→腕→脚の順に上書きしていく
	UpdateLegFlow(Application::Instance().GetDeltaTime());

	// 【部位破壊の方式確認(2026/07/29)】ボーンを潰すと部位が消えるかを実機で確かめる。
	// アニメ→腕→脚と骨を上書きしてきた最後に呼ぶ。ここより前だと後段に塗り潰される
	UpdateBoneCollapseTest();

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

	// ワイヤーでぶら下がっている間は、立体機動の滑空ポーズ(手付けの保持ポーズ)。
	// ※ 参考: ネギ夫氏「立体機動ポーズ集」pixiv id:38998125(ご自由にお使い下さい)を見ながら手付け
	// ※ 体の前傾はこのクリップに焼き込んでいない。ゲーム側のUpdateTiltが
	//    アンカー方向に応じて動的に傾けるので、二重に倒れないようクリップは直立で作ってある
	if (IsAnyWireAttached()) { return "20 odm fly"; }

	// 突撃中。専用が無いので落下で代用
	if (m_isDiving) { return "08 fall (air)"; }

	// 壁走り・よじ登り。走りで代用
	if (m_upWall && (m_upWall->IsRunning() || m_upWall->IsClimbing())) { return "03 run"; }

	// ステップ(回避)。踏み込みの出だしが近い「加速」で代用
	if (m_isDodging) { return "02 speed up"; }

	// 空中
	// ※ コヨーテタイム込みで見る。1〜数フレームの浮きで落下モーションへ飛ばないようにするため
	if (!IsGroundedOrCoyote()) { return "08 fall (air)"; }

	// 着地の余韻(PostUpdateでタイマーを立てている)
	if (m_landingAnimTimer > 0.0f) { return "10 fall (landing)"; }

	// 接地：動いていれば走り、止まっていれば待機
	float runThreshold = DebugParams::Instance().Float(U8("アニメ/走りに切り替わる速さ"), 0.5f, 0.0f, 5.0f);
	if (GetHorizontalSpeed() > runThreshold) { return "03 run"; }

	return "01 idle";
}

float Player::SelectAnimationSpeed() const
{
	// 走りは実際の水平速度に比例させて再生を速める。
	// 歩き(5.0)もダッシュ(11.0)も同じ速度で流すと足が地面を滑って見えるため。
	// それ以外は状態ごとの倍率をそのまま使う(全部DebugParamsで実行中に調整できる)
	if (m_currentAnimName == "03 run")
	{
		float baseSpeed = DebugParams::Instance().Float(U8("アニメ/走りの基準速度"), 5.0f, 1.0f, 20.0f);
		if (baseSpeed <= 0.0f) { return 1.0f; }

		// 上下に振り切れると不自然なので倍率を制限し、そのうえで全体の倍率を掛ける
		float ratio = std::clamp(GetHorizontalSpeed() / baseSpeed, 0.5f, 2.5f);
		return ratio * GetAnimSpeedScale(U8("アニメ/走りの再生速度"));
	}

	if (m_currentAnimName == "01 idle")           { return GetAnimSpeedScale(U8("アニメ/待機の再生速度")); }
	if (m_currentAnimName == "08 fall (air)")     { return GetAnimSpeedScale(U8("アニメ/落下の再生速度")); }
	if (m_currentAnimName == "10 fall (landing)") { return GetAnimSpeedScale(U8("アニメ/着地の再生速度")); }

	return 1.0f;
}

float Player::GetAnimSpeedScale(const char* _key) const
{
	return DebugParams::Instance().Float(_key, 1.0f, 0.1f, 4.0f);
}

float Player::SelectAnimationBlendTime() const
{
	// 0にすると従来どおりの即差し替えに戻るので、効果の有無をその場で見比べられる。
	// 長くすると入力に対して反応が鈍く見えるため、既定は短め
	return DebugParams::Instance().Float(U8("アニメ/切り替えを混ぜる秒数"), 0.15f, 0.0f, 0.5f);
}

float Player::SelectTurnSpeed() const
{
	return DebugParams::Instance().Float(U8("プレイヤー/向き直る速さ"), 720.0f, 90.0f, 2880.0f);
}

void Player::DrawSprite()
{
	// ロックオンのマーカー。ワールド座標をスクリーン座標へ変換して2Dで描く
	// (2Dパスは3Dの絵が全部出た後・深度テスト無しなので、体の内側の関節でも埋まらない)
	m_upTargeting->DrawMarker();
}

void Player::DrawUnLit()
{
	// キャラのモデルは CharaBase::DrawLit が描く。ここ(陰影なしパス)では
	// ワイヤーの見た目を描く(斬撃VFXはEffectManagerが描く)。
	// ※ ロックオンのマーカーは2D描画パス(DrawSprite)へ移した。
	//   狙う関節は体の内側にあるため、3Dで描くと深度テストでモデルに埋まる
	DrawWire();
}

bool Player::IsAnyWireAttached() const
{
	for (const std::unique_ptr<WireAction>& w : m_upWires)
	{
		if (w && w->IsAttached()) { return true; }
	}

	return false;
}

WireAction* Player::GetAttachedWire() const
{
	for (const std::unique_ptr<WireAction>& w : m_upWires)
	{
		if (w && w->IsAttached()) { return w.get(); }
	}

	return nullptr;
}

void Player::ReleaseAllWires(bool _animate)
{
	for (const std::unique_ptr<WireAction>& w : m_upWires)
	{
		if (w) { w->Release(_animate); }
	}
}

bool Player::FindAnchorDir(int _index, const Math::Vector3& _from, const Math::Vector3& _aimDir,
	float _maxLen, Math::Vector3& _outDir)
{
	// 【なぜ扇状に探すのか】
	// 街の実測: 建物は16.6m幅x20.1m高で15.5m間隔に並び、通りの幅は約3.8m(狭い路地)と
	// 約39.4m(広場)の2種類しかない。路地では壁が1.9m横、広場では約20m横にある。
	// 「レティクル方向へ1本のレイ」では、真横にある壁へは原理的に届かない
	// (20m横の壁に4度で届くには前方286m必要)。
	// だからフックは「狙った点」ではなく「自分の側にある取り付けられる面」を探す。
	// 左フックは前方〜左、右フックは前方〜右へ扇状にレイを飛ばす。
	//
	// 【選び方】当たった中で「狙いからのズレが最小」のものを採用する。
	// 壁を直接狙えば0度で当たるので照準がそのまま効き、通りを狙えば0度が抜けて
	// 20〜60度で側面の壁を捉える。プレイヤーの意図を最大限尊重しつつ側面へ落ちる
	float maxAngle = DebugParams::Instance().Float(U8("ワイヤー/探索の最大角度"), 75.0f, 0.0f, 120.0f);
	int   rays     = DebugParams::Instance().Int(  U8("ワイヤー/探索の本数"),      5,   1,  12);
	float upTilt   = DebugParams::Instance().Float(U8("ワイヤー/探索の上向き角"), 15.0f, 0.0f, 60.0f);
	float minDist  = DebugParams::Instance().Float(U8("ワイヤー/最短の取り付き距離"), 4.0f, 0.5f, 30.0f);

	// 添字0=左(-Y回り) / 1=右(+Y回り)
	float sign = (_index == 0) ? -1.0f : 1.0f;

	for (int r = 0; r < rays; ++r)
	{
		// 0度から順に外へ広げる。最初に当たったものが「ズレ最小」になる
		float t = (rays <= 1) ? 0.0f : static_cast<float>(r) / static_cast<float>(rays - 1);
		float angle = maxAngle * t;

		Math::Vector3 dir = Math::Vector3::TransformNormal(
			_aimDir, Math::Matrix::CreateRotationY(DirectX::XMConvertToRadians(angle * sign)));

		// 横へ振るほど上向きに寄せる。建物は20mあるので、側面を狙うときは
		// 高い所に刺さったほうが振り子として使える。真正面(0度)では傾けない=照準を壊さない
		if (upTilt > 0.0f && t > 0.0f)
		{
			dir += Math::Vector3::Up * (std::tan(DirectX::XMConvertToRadians(upTilt)) * t);
			if (!MathAPI::TryNormalize(dir)) { continue; }
		}

		Math::Vector3 hitPos;
		bool isGround = false;
		float dist = 0.0f;
		bool hit = WireAction::CastAnchor(_from, dir, _maxLen, hitPos, isGround, dist);

		// 目の前の壁に刺すと巻き取りで即激突するので、近すぎるものは採用しない
		bool ok = hit && (dist >= minDist);

		// デバッグ表示用に、探した軌跡を残しておく(採用/不採用も含めて)
		m_wireProbes.push_back({ _from, dir, hit ? dist : _maxLen, hit, ok });

		if (!ok) { continue; }

		_outDir = dir;
		return true;
	}

	return false;
}

void Player::DrawDebugWire()
{
	if (!m_pDebugWire) { return; }

	// 探索レイ：どこを探して、どこで当たったか。緑=当たり / 灰=外れ / 黄=実際に採用した1本
	for (const WireProbe& p : m_wireProbes)
	{
		Math::Color col = p.used ? Math::Color(1.0f, 0.9f, 0.15f, 1.0f)
			: (p.hit ? Math::Color(0.3f, 0.9f, 0.3f, 1.0f) : Math::Color(0.45f, 0.45f, 0.45f, 1.0f));
		m_pDebugWire->AddDebugLine(p.from, p.dir, p.length, col);
	}

	// 射出口(腰の左右)。位置の調整に使う
	for (int i = 0; i < kWireCount; ++i)
	{
		m_pDebugWire->AddDebugSphere(GetWireMuzzlePos(i), 0.06f, kWhiteColor);
	}

	// アンカー(実際に刺さっている点)
	for (const std::unique_ptr<WireAction>& w : m_upWires)
	{
		if (!w || !w->IsAttached()) { continue; }

		m_pDebugWire->AddDebugSphere(w->GetAnchor(), 0.25f, kRedColor);
	}

	// 【本命】プレイヤーが閉じ込められている球。
	// 2本を1つにまとめているときはその球、そうでなければ各ワイヤーの球を出す
	const Math::Color kCage(0.25f, 0.75f, 1.0f, 1.0f);
	bool drewMerged = false;
	for (const std::unique_ptr<WireAction>& w : m_upWires)
	{
		if (!w || !w->IsAttached() || !w->IsMerged()) { continue; }

		m_pDebugWire->AddDebugSphere(w->GetMergedPivot(), w->GetMergedRadius(), kCage);
		// 支点(2本の中点に置いた仮想アンカー)
		m_pDebugWire->AddDebugSphere(w->GetMergedPivot(), 0.2f, Math::Color(1.0f, 0.9f, 0.15f, 1.0f));
		drewMerged = true;
		break;
	}
	if (!drewMerged)
	{
		for (const std::unique_ptr<WireAction>& w : m_upWires)
		{
			if (!w || !w->IsAttached()) { continue; }

			m_pDebugWire->AddDebugSphere(w->GetAnchor(), w->GetLength(), kCage);
		}
	}
}

Math::Vector3 Player::GetWireMuzzlePos(int _index) const
{
	// 立体機動装置の射出口は腰の左右。腰のボーンから、体のローカルな左右へずらして求める。
	// ※ ボーンのワールド行列にはキャラの位置・向き・傾きが入っているので、
	//    体が傾けば射出口も一緒に傾く(ワールド軸でずらすと体を捻ったときにズレる)
	// ※ ボーンが無いモデルに差し替えても、従来どおり胴体中心へフォールバックする
	float side = DebugParams::Instance().Float(U8("ワイヤー/射出口の左右幅"), 0.18f, 0.0f, 1.0f);
	float back = DebugParams::Instance().Float(U8("ワイヤー/射出口の後ろへの寄せ"), 0.10f, -0.5f, 0.5f);

	// 添字0=左(+X) / 1=右(-X)
	float sign = (_index == 0) ? 1.0f : -1.0f;

	const KdModelWork::Node* hips = m_modelWork.FindNode("mixamorig:Hips");
	if (hips)
	{
		Math::Matrix hipsMat = GetBoneWorldMatrix(*hips);

		// 腰ボーンのローカル軸で左右・後ろへずらす。glTFのMixamoリグは骨の軸が+Yなので、
		// 左右はRight(X)、体の後ろはBackward(Z)にあたる
		Math::Vector3 offset = Math::Vector3(sign * side, 0.0f, back);
		return Math::Vector3::Transform(offset, hipsMat);
	}

	return GetPos() + Math::Vector3(0.0f, 0.25f, 0.0f);
}

void Player::DrawWire()
{
	// 発射中／スイング中：本数ぶん、それぞれの射出口からフック先端へ線を引く。
	// 終点にアンカーではなく GetHookPos() を使うのは、着弾前は線が伸びていく途中だから。
	// 着弾後は GetHookPos() がアンカーそのものを返すので、同じ経路で描ける
	bool anyActive = false;
	for (int i = 0; i < kWireCount; ++i)
	{
		const std::unique_ptr<WireAction>& w = m_upWires[i];
		if (!w || !w->IsVisible()) { continue; }

		anyActive = true;
		Math::Vector3 muzzle = GetWireMuzzlePos(i);
		w->Draw(muzzle, w->GetHookPos());

		// 先端のフック。線だけだと飛行中に空中で途切れて終わって見える
		w->DrawHook(muzzle);
	}
	if (anyActive) { return; }

	// 突撃(グラップル)中：引き寄せている先へ線を引く(1本でよい)
	if (m_isDiving)
	{
		if (std::shared_ptr<KdGameObject> spTarget = m_wpDiveTarget.lock())
		{
			// 🔴 終点は【狙っている関節】。UpdateDiveが実際に引き寄せている点と同じにする。
			// 以前は対象のGetPos()(=モデルの中心)へ引いていたので、
			// 関節に掛けたはずの線が突撃に移った瞬間に体の中心へ飛んで見えていた
			Math::Vector3 aim{};
			if (!GetLockedJointPos(aim))
			{
				aim = spTarget->GetPos() + Math::Vector3(0.0f, 0.5f, 0.0f);
			}

			m_upWires[0]->Draw(GetWireMuzzlePos(0), aim);
		}
	}
}

void Player::DrawDebug()
{
	// カテゴリごとに出す/出さないを分ける(DebugFlagsの「デバッグ表示/〜」)。
	// プレイヤーの索敵/攻撃範囲と、ワイヤーの拘束範囲は別物なので別カテゴリにしてある
	const bool showPlayer = s_showColliderDebug && DebugDraw::IsOn(DebugDraw::Category::Player);
	const bool showWire   = s_showColliderDebug && DebugDraw::IsOn(DebugDraw::Category::Wire);

	if (showPlayer || showWire)
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
	}
	if (showPlayer)
	{
		DrawDebugRanges();
	}
	if (showWire)
	{
		DrawDebugWire();
	}

	// KdGameObject::DrawDebugが m_pCollider の形状を積み、m_pDebugWire をまとめて描画する。
	// 基底は s_showColliderDebug しか見ないので、コライダー可視化も「プレイヤー」で絞る
	// (絞らないと、ワイヤーだけ見たいときにコライダーまで出てしまう)
	{
		DebugDraw::ScopedGate gate(DebugDraw::Category::Player);
		KdGameObject::DrawDebug();
	}
}

void Player::WatchDebug() const
{
	DebugWatch& w = DebugWatch::Instance();

	// 速度・接地まわり
	w.Watch(U8("Player/速度"),       m_velocity.Length());   // 上限の調整用(プレイヤー/最高速度と見比べる)
	w.Watch(U8("Player/水平速度"),   GetHorizontalSpeed());
	w.Watch(U8("Player/垂直速度"),   m_velocity.y);
	w.Watch(U8("Player/接地"),       IsGrounded());
	// 何本掛かっているかを出す。2本掛けが成立しているかを目で確かめるため
	// (谷間を狙えば2、平らな壁なら2、片方が外れれば1、切れば0)
	int wireCount = 0;
	for (const std::unique_ptr<WireAction>& wire : m_upWires)
	{
		if (wire && wire->IsAttached()) { ++wireCount; }
	}
	w.Watch(U8("Player/ワイヤー本数"), wireCount);

	// 再生中のアニメ名。空のままなら「名前が見つかっていない」= glTFのアニメ名との
	// 綴り違いを疑う(Scifi_girlは "01 idle" のように番号＋半角スペース＋英字)
	w.Watch(U8("Player/アニメ"), GetCurrentAnimName());

	// 空中ステップまわり(空中でSpaceを短く離すと出る。無敵つき)。
	// ステップが出ない時は「ステップの残り」が0になっていないかを見る
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

	// 攻撃判定：クリティカル範囲(赤)と、通過とみなす距離(灰)。
	//
	// 🔴 【2026/08/02】プレイヤーの位置ではなく【狙っている関節の位置】に描く。
	//   判定は「関節までの距離」で行っているので、自分の周りに球を出しても
	//   実際に斬れる範囲とは何の関係も無かった(古い落下攻撃時代の名残)。
	//   実際の判定と同じ GetCriticalRange を読むので、見えている球＝そのまま判定
	{
		Math::Vector3 aim{};
		if (GetLockedJointPos(aim))
		{
			const float critRange = GetCriticalRange();

			// 赤：この中で押せばクリティカル。外で押しても先行入力で通常は当たる
			m_pDebugWire->AddDebugSphere(aim, critRange, Math::Color(1.0f, 0.15f, 0.15f, 1.0f));

			// 灰：ここまで来ても押していなければ通過(突撃終了)
			float passRate = DebugParams::Instance().Float(U8("突撃/通過とみなす割合"), 0.25f, 0.05f, 0.9f);
			m_pDebugWire->AddDebugSphere(aim, critRange * passRate, Math::Color(0.5f, 0.5f, 0.5f, 1.0f));

			// 自分から狙点への線。今どれだけ離れているかが目で分かる
			m_pDebugWire->AddDebugLine(pos, aim, Math::Color(1.0f, 0.8f, 0.3f, 1.0f));
		}
	}


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

	// ※ ワイヤー関連の可視化(拘束範囲・アンカー・探索レイ)は DrawDebugWire へ移した。
	//    「デバッグ表示/ワイヤー」で独立して出せるようにするため
}

Math::Vector2 Player::SelectTilt() const
{
	float pitchGain = DebugParams::Instance().Float(U8("傾き/ワイヤー時のピッチ係数"), 1.0f, -2.0f, 2.0f);
	float rollGain = DebugParams::Instance().Float(U8("傾き/ワイヤー時のロール係数"), 1.0f, -2.0f, 2.0f);
	float leanGain = DebugParams::Instance().Float(U8("傾き/前傾の強さ"), 0.8f, -3.0f, 3.0f);

	// ワイヤー中：体の「上」をアンカーへ向ける(ぶら下がって振られている姿勢)。
	// ※ 2本掛かっているときは全アンカーの中点を向く。片方に引っ張られて偏らないように
	if (IsAnyWireAttached())
	{
		//アンカーの向きを求める
		Math::Vector3 anchorMid = Math::Vector3::Zero;
		int attached = 0;
		for (const std::unique_ptr<WireAction>& w : m_upWires)
		{
			if (!w || !w->IsAttached()) { continue; }

			anchorMid += w->GetAnchor();
			++attached;
		}
		anchorMid /= static_cast<float>(attached);

		Math::Vector3 toAnchor = anchorMid - GetPos();
		toAnchor.Normalize();

		//キャラのローカル空間へ移す
		//Y回転を打ち消す
		Math::Matrix invYaw = Math::Matrix::CreateRotationY(-DirectX::XMConvertToRadians(GetRot().y));
		Math::Vector3 local = Math::Vector3::TransformNormal(toAnchor, invYaw);

		//真上からのズレを、前後と左右に分ける
		float pitch = DirectX::XMConvertToDegrees(std::atan2(local.z, local.y));
		float roll = DirectX::XMConvertToDegrees(std::atan2(local.x, local.y));

		return Math::Vector2(pitch * pitchGain, roll * rollGain);
	}

	// 地上・落下：水平速度に比例して前傾するだけ
	return Math::Vector2(GetHorizontalSpeed() * leanGain, 0.0f);
}

bool Player::SelectArmAimTarget(Math::Vector3& _outTarget) const
{
	// 【優先順位】ワイヤー接続 > 突撃 > 照準。
	// 上ほど「実際に体が繋がっている」状態で、腕の向きが機能に直結するため。
	// ワイヤー中にEを押してターゲットを取ることはありえるが、その時はアンカー側を優先する
	// (実際に引っ張られている先を指すほうが自然で、線の根元=手との整合も保てる)

	// ワイヤーで繋がっている間は左腕をアンカーへ向ける。
	// 判定条件はSelectTiltと同じなので、体の傾きと腕が同じタイミングで入り、同じタイミングで抜ける
	if (WireAction* w = GetAttachedWire())
	{
		_outTarget = w->GetAnchor();
		return true;
	}

	// 突撃(グラップル)中は引き寄せている対象へ向ける。
	// ※ オフセット+0.5はDrawWireが突撃線の終点に使っている値と同じ。
	//    揃えないと「線の終点」と「腕の狙う先」がずれる
	if (m_isDiving)
	{
		if (std::shared_ptr<KdGameObject> spTarget = m_wpDiveTarget.lock())
		{
			_outTarget = spTarget->GetPos() + Math::Vector3(0.0f, 0.5f, 0.0f);
			return true;
		}
	}

	// 照準中(E押しでターゲットをロックしている間)は、その敵へ腕を向ける。
	// ※ 入力(Focus)を直接見ないのは、Targeting::Updateが「カメラを渡さなければ解除」という作りで
	//    押していない間は自動でnullptrになるため。入力を二重に判定すると、連続攻撃の受付中
	//    (Eを離してもロックが続く仕様)で食い違う
	if (m_upTargeting)
	{
		if (std::shared_ptr<KdGameObject> spTarget = m_upTargeting->GetTarget())
		{
			_outTarget = spTarget->GetPos() + Math::Vector3(0.0f, 0.5f, 0.0f);
			return true;
		}
	}

	// それ以外は腕をアニメーションに任せる
	return false;
}

void Player::NotifyCounter()
{
	// 敵の攻撃をジャスト回避で受けた時に敵側から呼ぶ。実際の窓開けは次のUpdateCounterで行う
	// (発動側をPlayerに集約して、スロー窓や突撃移行の管理を1箇所にまとめるため)
	//
	// 🔴 2026-08-04時点で【呼び出し元が無い】。敵の突進(Enemy::ResolveStrikeHit)を撤去したため、
	//   反撃システムに入る手段が消えている。敵に新しい攻撃を入れるとき、必ずここを呼び直すこと
	//   (回避の無敵は反撃の唯一の入口なので、受け側であるこの関数は残してある)
	m_counterPending = true;
}

void Player::ApplyKnockback(const Math::Vector3& _dir, float _power)
{
	// 回避中(無敵)は弾かれない。※呼び出し側(Enemy)も無敵時は反撃へ回すので通常来ないが保険
	if (IsInvincible()) { return; }

	Math::Vector3 dir = MathAPI::GetSafeNormalXZ(_dir, Math::Vector3::Backward);

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

		// 攻撃ボタン(右クリック)を押したら突撃へ移行する(狙いはauto-target、無ければ最寄りの敵)
		// ※ 2026/08/02の入力のAoT2化で左クリック→右クリックへ移った
		if (KdInputManager::Instance().IsPress("Attack"))
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
