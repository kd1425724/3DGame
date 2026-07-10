#include "WireAction.h"

// 実装するときに必要になりそうなinclude(今はコメント。使う時にコメントを外す)
//  ・#include "../../Scene/SceneManager.h"        ... Shootのレイ判定で全オブジェクトを走査する
//  ・#include "../../Debug/DebugParams/DebugParams.h" ... リール速度・引き込み力などの調整値を外部化する


bool WireAction::Shoot(const Math::Vector3& _from, const Math::Vector3& _dir, float _maxLength)
{
	// ① _from から _dir 方向へ、長さ _maxLength のレイ(KdCollider::RayInfo)を作る
	//    当てたい種類は地形系。例: KdCollider::TypeGround | KdCollider::TypeBump

	// ② SceneManager の全オブジェクトに Intersects(ray, &結果リスト) して、当たりを集める
	//    (CharaBase::GroundCheck が同じ書き方をしているので参考になる)

	// ③ 集めた当たりの中から「_from に一番近い交点」を選ぶ
	//    (Math::Vector3::Distance で距離を比べて、一番小さいものを採用)

	// ④ 命中したら:
	//      ・m_anchor に交点をセット
	//      ・m_length = _from と m_anchor の距離(＝撃った瞬間のワイヤー長)
	//      ・m_isAttached = true
	//      ・true を返す
	//    命中しなかったら false を返す

	// TODO: 上の①〜④を実装する
	return false;   // ← 仮のreturn(ビルドを通すため)。実装したら正しい戻り値に置き換える
}

void WireAction::Release()
{
	// m_isAttached を false にするだけ
}

bool WireAction::IsAttached() const
{
	// m_isAttached を返すだけ
	return m_isAttached;
}

void WireAction::Update(Math::Vector3& _pos, Math::Vector3& _vel, float _dt, float _reelInput)
{
	// 繋がっていない(!m_isAttached)なら何もせず早期return

	// ① リール: _reelInput でワイヤー長 m_length を伸縮させる
	//    ・縮める量 = _reelInput * リール速度(DebugParams) * _dt を m_length から引く
	//    ・短くなりすぎないよう下限(例:0.5)でクランプ

	// ② アンカーから自分へのベクトル toPos = _pos - m_anchor と、その長さ dist を求める
	//    ・dist がほぼ0なら向きが定義できないので早期return
	//    ・dir = toPos / dist (アンカーから外向きの単位ベクトル)

	// ③ 距離拘束: dist が m_length より大きい(=ワイヤーを伸ばしすぎ)ときだけ:
	//    (a) 位置を円周(球面)上へ戻す:  _pos = m_anchor + dir * m_length
	//    (b) 速度の半径方向成分だけ打ち消す:
	//         radialSpeed = _vel.Dot(dir)
	//         radialSpeed > 0 (外向き)のときだけ _vel から dir * radialSpeed を引く
	//         → 残った接線方向の速度で振り子のように振れる
	//    (c) (任意)張力で少し内向きに引く: _vel から dir * 引き込み力(DebugParams) * _dt を引く

	// ※ 内側(dist <= m_length)は自由に動ける = 不等式拘束。だからワイヤーに向かって寄れる
}

const Math::Vector3& WireAction::GetAnchor() const
{
	// m_anchor を返すだけ
	return m_anchor;
}
