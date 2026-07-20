#include "WallAction.h"

#include "../Chara/CharaBase.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Effect/EffectManager.h"

void WallAction::Update(CharaBase& _body, float _dt, const Math::Vector3& _wishDir)
{
	// 再吸着の待ち時間を消化する
	if (m_cooldown > 0.0f)
	{
		m_cooldown -= _dt;
	}

	// 壁の接触に猶予を持たせる。ResolveBumpは「めり込んでいる時」だけ接触を立て、
	// 押し出した直後はめり込みが0になるので、壁沿いに動いていると接触が点滅する。
	// 生のフラグを条件にすると壁走りがほとんど始まらないので、直近で触れていれば
	// 触れているものとして扱う(接地のコヨーテタイムと同じ)
	if (_body.m_isTouchingWall)
	{
		m_sinceTouch = 0.0f;
		m_lastTouchNormal = _body.m_wallNormal;
	}
	else
	{
		m_sinceTouch += _dt;
	}

	// 接地したら「同じ壁の禁止」は解除する(地面を踏めば同じ壁をもう一度走れる)
	if (_body.m_isGrounded)
	{
		m_hasBannedWall = false;
		if (m_isRunning)
		{
			Stop(_body, false);
		}
		return;
	}

	if (!m_isRunning)
	{
		if (CanStart(_body))
		{
			Start(_body);
		}
		return;
	}

	// --- ここから走行中 ---

	// 壁を見失ったら終わり(壁の端まで走り抜けた/角を曲がれなかった)。
	// 猶予つきで見るので、押し出し直後の一瞬の非接触では剥がれない
	float touchGrace = DebugParams::Instance().Float(U8("壁走り/接触の猶予"), 0.2f, 0.0f, 1.0f);
	if (m_sinceTouch > touchGrace)
	{
		Stop(_body, false);
		return;
	}

	// 壁の法線を追従させる。曲がった壁でも走り続けられるようにするが、
	// 急に別方向の壁(建物の角)へ乗り換えると挙動が飛ぶので、向きが大きく変わったら剥がす
	float turnLimit = DebugParams::Instance().Float(U8("壁走り/曲がれる角度の内積"), 0.5f, -1.0f, 1.0f);
	if (m_wallNormal.Dot(m_lastTouchNormal) < turnLimit)
	{
		Stop(_body, false);
		return;
	}
	m_wallNormal = m_lastTouchNormal;

	// 走れる時間には上限を設ける(無いと壁に張り付いたまま永久に移動できてしまう)。
	// ※ よじ登りもこの時間で打ち切られる。1回の取り付きで登れる高さ =
	//    「走れる時間 x よじ登る速さ」。2.0 x 8.0 = 16m ≒ 街の家(SCALE2.0で約20m)の
	//    大部分。もっと登りたければこの2つを上げる
	float maxTime = DebugParams::Instance().Float(U8("壁走り/走れる時間"), 2.0f, 0.1f, 5.0f);
	m_runTime += _dt;
	if (m_runTime > maxTime)
	{
		// 時間切れで剥がれた時も同じ壁は禁止する(すぐ張り付き直して延長できてしまうため)
		Stop(_body, true);
		return;
	}

	// 壁ジャンプ
	if (KdInputManager::Instance().IsPress("Jump"))
	{
		WallJump(_body);
		return;
	}

	// --- 壁に沿った移動 ---
	// 壁の法線成分(壁へ突っ込む/離れる速度)を取り除き、壁面に沿った成分だけ残す。
	// これで壁に当たっても勢いが死なず、壁沿いに走り続けられる
	Math::Vector3 v = _body.m_velocity;
	v -= m_wallNormal * v.Dot(m_wallNormal);

	// 壁の方を向いて前入力していれば「よじ登り」に切り替える。
	// 判定は移動入力の向きと壁の内向き(-法線)の内積＝カメラを壁へ向けてWを押している状態。
	// ずり落ちの代わりに一定速度で上昇する(走行時間の上限はそのまま効くので登り放題にはならない)
	float climbDot = DebugParams::Instance().Float(U8("壁走り/よじ登りに必要な向き"), 0.55f, 0.0f, 1.0f);
	m_isClimbing = false;
	if (_wishDir.LengthSquared() > 0.0001f)
	{
		m_isClimbing = _wishDir.Dot(-m_wallNormal) > climbDot;
	}

	if (m_isClimbing)
	{
		float climbSpeed = DebugParams::Instance().Float(U8("壁走り/よじ登る速さ"), 8.0f, 0.0f, 30.0f);
		v.y = climbSpeed;
	}
	else
	{
		// 重力の代わりにゆっくり下降させる(通常の重力はm_gravityScale=0で止めてある)。
		// 0にすると水平に張り付いたまま飛ぶので、少しずつずり落ちる方が壁を走っている感じが出る
		float slide = DebugParams::Instance().Float(U8("壁走り/ずり落ちる速さ"), 5.0f, 0.0f, 30.0f);
		v.y -= slide * _dt;
	}

	// 壁へ軽く押し付ける。これが無いと法線成分を消した時点で壁から離れていき、
	// 次のフレームで接触が切れて壁走りが一瞬で終わってしまう。
	// (押し付けたぶんはResolveBumpが押し返すので、めり込みはしない)
	float stick = DebugParams::Instance().Float(U8("壁走り/壁へ押し付ける速さ"), 2.0f, 0.0f, 10.0f);
	v -= m_wallNormal * stick;

	_body.m_velocity = v;

	// 壁を擦っている火花を出す(壁走り中だと見て分かるように)
	SpawnFx(_body, _dt);
}

void WallAction::SpawnFx(const CharaBase& _body, float _dt)
{
	// 毎フレーム出すとフレームレートで密度が変わるので、時間あたりの個数で制御する
	float rate = DebugParams::Instance().Float(U8("壁走りエフェクト/毎秒の粒数"), 45.0f, 0.0f, 150.0f);
	if (rate <= 0.0f) { return; }

	// 火花を流す向き＝進んでいる向きの逆(SpawnWallRunが逆向きに流す)。
	// よじ登り中は水平にはほぼ動かないので、上向きを渡して火花が下へ落ちるようにする
	Math::Vector3 runDir = Math::Vector3::Up;
	if (!m_isClimbing)
	{
		runDir = Math::Vector3(_body.m_velocity.x, 0.0f, _body.m_velocity.z);
		if (runDir.LengthSquared() < 0.0001f) { return; }
		runDir.Normalize();
	}

	// 発生位置は壁との接点(体の中心から壁へ寄せた所)の、少し足元寄り
	float radius = DebugParams::Instance().Float(U8("キャラ/壁当たり半径"), 0.4f, 0.1f, 2.0f);
	float footY  = DebugParams::Instance().Float(U8("壁走りエフェクト/足元へのオフセット"), 0.3f, -2.0f, 2.0f);
	Math::Vector3 contact = _body.GetPos() - m_wallNormal * radius;
	contact.y -= footY;

	m_fxTimer += _dt;
	float interval = 1.0f / rate;
	while (m_fxTimer >= interval)
	{
		m_fxTimer -= interval;
		EffectManager::Instance().SpawnWallRun(contact, runDir, m_wallNormal);
	}
}

bool WallAction::CanStart(const CharaBase& _body) const
{
	// 空中で、壁に触れていて(猶予つき)、再吸着の待ちが明けていること
	if (_body.m_isGrounded) { return false; }
	if (m_cooldown > 0.0f) { return false; }

	float touchGrace = DebugParams::Instance().Float(U8("壁走り/接触の猶予"), 0.2f, 0.0f, 1.0f);
	if (m_sinceTouch > touchGrace) { return false; }

	const Math::Vector3& n = m_lastTouchNormal;

	// 直前に蹴った壁と同じ向きの壁には取り付けない。
	// これが無いと1枚の壁で「走る→跳ぶ→また同じ壁」を繰り返して無限に登れてしまう。
	// 別の壁(向きが違う壁)なら取り付けるので、2枚の壁を交互に蹴る動きは成立する
	float sameWall = DebugParams::Instance().Float(U8("壁走り/同じ壁とみなす内積"), 0.7f, 0.0f, 1.0f);
	if (m_hasBannedWall && n.Dot(m_bannedWallNormal) > sameWall) { return false; }

	// 壁に沿った水平の速さが十分にあること。
	// 壁に正面から当たっただけ(=沿う成分が無い)では発動させない。
	// 「走っている勢いを壁に預ける」動きなので、止まっていたら張り付かないのが正しい
	Math::Vector3 hv(_body.m_velocity.x, 0.0f, _body.m_velocity.z);
	Math::Vector3 tangent = hv - n * hv.Dot(n);

	// ※ 2026/07/20 に 6.0 → 3.0 へ緩めた(ユーザー指示「開始判定はもっとゆるくていい」)。
	//    通常の歩き速度でも壁に沿っていれば掛かるようにする
	float minSpeed = DebugParams::Instance().Float(U8("壁走り/発動に必要な速さ"), 3.0f, 0.0f, 40.0f);
	if (tangent.Length() < minSpeed) { return false; }

	return true;
}

void WallAction::Start(CharaBase& _body)
{
	m_isRunning = true;
	m_runTime = 0.0f;
	m_fxTimer = 0.0f;
	m_wallNormal = m_lastTouchNormal;

	// 別の壁に取り付けた時点で、前の壁の禁止は用済み
	m_hasBannedWall = false;

	// 壁走り中は重力を止める(ずり落ちはUpdateが別途与える)
	_body.m_gravityScale = 0.0f;

	// 取り付いた瞬間に少し持ち上げる。落下しながら壁に触れた時、そのまま落ち続けると
	// 「張り付いた」手応えが出ないため、下向きの速度を消して軽く上へ跳ねさせる
	float kickUp = DebugParams::Instance().Float(U8("壁走り/取り付いた時の上向き"), 3.0f, 0.0f, 20.0f);
	if (_body.m_velocity.y < kickUp)
	{
		_body.m_velocity.y = kickUp;
	}
}

void WallAction::Stop(CharaBase& _body, bool _lockSameWall)
{
	m_isRunning = false;
	m_isClimbing = false;
	m_runTime = 0.0f;

	// 重力を戻す
	_body.m_gravityScale = 1.0f;

	float cooldown = DebugParams::Instance().Float(U8("壁走り/再吸着までの待ち"), 0.15f, 0.0f, 2.0f);
	m_cooldown = cooldown;

	if (_lockSameWall)
	{
		m_hasBannedWall = true;
		m_bannedWallNormal = m_wallNormal;
	}
}

void WallAction::WallJump(CharaBase& _body)
{
	// 壁を蹴った方向へ跳ぶ：法線方向(壁から離れる)＋上向き＋壁沿いの勢いを一部維持。
	// 壁沿いの勢いを残すのは、跳んだ瞬間に前進が止まると次の壁へ届かなくなるため
	float kick = DebugParams::Instance().Float(U8("壁ジャンプ/壁を蹴る強さ"),   12.0f, 0.0f, 50.0f);
	float up   = DebugParams::Instance().Float(U8("壁ジャンプ/上向きの強さ"),   11.0f, 0.0f, 50.0f);
	float keep = DebugParams::Instance().Float(U8("壁ジャンプ/勢いの持ち越し"),  0.6f, 0.0f,  2.0f);

	Math::Vector3 hv(_body.m_velocity.x, 0.0f, _body.m_velocity.z);
	Math::Vector3 tangent = hv - m_wallNormal * hv.Dot(m_wallNormal);

	Math::Vector3 v = m_wallNormal * kick + tangent * keep;
	v.y = up;

	_body.m_velocity = v;

	// 蹴った瞬間だけ火花をまとめて出す(壁を蹴った手応えを見せる)
	int burst = DebugParams::Instance().Int(U8("壁ジャンプ/火花の数"), 12, 0, 60);
	float radius = DebugParams::Instance().Float(U8("キャラ/壁当たり半径"), 0.4f, 0.1f, 2.0f);
	Math::Vector3 contact = _body.GetPos() - m_wallNormal * radius;
	for (int i = 0; i < burst; ++i)
	{
		// 蹴った方向(法線)へ散らす。SpawnWallRunは第2引数の逆へ流すので、
		// 法線の逆を渡して「壁から外向きに弾ける」動きにする
		EffectManager::Instance().SpawnWallRun(contact, -m_wallNormal, m_wallNormal);
	}

	// 蹴った壁は禁止する(同じ壁に張り付き直して無限に登れないように)
	Stop(_body, true);
}

void WallAction::Cancel(CharaBase& _body)
{
	if (!m_isRunning) { return; }

	// 他の行動(ワイヤー/突撃/リスポーン)へ移る時の中断。
	// ここでは壁を禁止しない(プレイヤーが自分の意思で抜けただけなので、また同じ壁を走れてよい)
	Stop(_body, false);
}
