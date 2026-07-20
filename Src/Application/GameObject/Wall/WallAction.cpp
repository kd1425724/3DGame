#include "WallAction.h"

#include "../Chara/CharaBase.h"
#include "../../Debug/DebugParams/DebugParams.h"

void WallAction::Update(CharaBase& _body, float _dt)
{
	// 再吸着の待ち時間を消化する
	if (m_cooldown > 0.0f)
	{
		m_cooldown -= _dt;
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

	// 壁を見失ったら終わり(壁の端まで走り抜けた/角を曲がれなかった)
	if (!_body.m_isTouchingWall)
	{
		Stop(_body, false);
		return;
	}

	// 壁の法線を追従させる。曲がった壁でも走り続けられるようにするが、
	// 急に別方向の壁(建物の角)へ乗り換えると挙動が飛ぶので、向きが大きく変わったら剥がす
	float turnLimit = DebugParams::Instance().Float(U8("壁走り/曲がれる角度の内積"), 0.5f, -1.0f, 1.0f);
	if (m_wallNormal.Dot(_body.m_wallNormal) < turnLimit)
	{
		Stop(_body, false);
		return;
	}
	m_wallNormal = _body.m_wallNormal;

	// 走れる時間には上限を設ける(無いと壁に張り付いたまま永久に移動できてしまう)
	float maxTime = DebugParams::Instance().Float(U8("壁走り/走れる時間"), 1.4f, 0.1f, 5.0f);
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

	// 重力の代わりにゆっくり下降させる(通常の重力はm_gravityScale=0で止めてある)。
	// 0にすると水平に張り付いたまま飛ぶので、少しずつずり落ちる方が壁を走っている感じが出る
	float slide = DebugParams::Instance().Float(U8("壁走り/ずり落ちる速さ"), 5.0f, 0.0f, 30.0f);
	v.y -= slide * _dt;

	// 壁へ軽く押し付ける。これが無いと法線成分を消した時点で壁から離れていき、
	// 次のフレームで接触が切れて壁走りが一瞬で終わってしまう。
	// (押し付けたぶんはResolveBumpが押し返すので、めり込みはしない)
	float stick = DebugParams::Instance().Float(U8("壁走り/壁へ押し付ける速さ"), 2.0f, 0.0f, 10.0f);
	v -= m_wallNormal * stick;

	_body.m_velocity = v;
}

bool WallAction::CanStart(const CharaBase& _body) const
{
	// 空中で、壁に触れていて、再吸着の待ちが明けていること
	if (_body.m_isGrounded) { return false; }
	if (m_cooldown > 0.0f) { return false; }
	if (!_body.m_isTouchingWall) { return false; }

	const Math::Vector3& n = _body.m_wallNormal;

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

	float minSpeed = DebugParams::Instance().Float(U8("壁走り/発動に必要な速さ"), 6.0f, 0.0f, 40.0f);
	if (tangent.Length() < minSpeed) { return false; }

	return true;
}

void WallAction::Start(CharaBase& _body)
{
	m_isRunning = true;
	m_runTime = 0.0f;
	m_wallNormal = _body.m_wallNormal;

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
	m_runTime = 0.0f;

	// 重力を戻す
	_body.m_gravityScale = 1.0f;

	float cooldown = DebugParams::Instance().Float(U8("壁走り/再吸着までの待ち"), 0.2f, 0.0f, 2.0f);
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
