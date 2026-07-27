//=====================================================================
//
//  WireAction ── ワイヤーアクションの物理(距離拘束)＋見た目を担う部品
//
//  ・KdGameObject派生。ただしシーンのオブジェクトリストには入れず、Playerなどが
//    「部品」として所有し(立体機動装置に合わせて2本)、親がUpdateSwingAll/Draw/Shootを直接呼んで駆動する
//    (KdGameObjectを継承するのは「オブジェクトは全てKdGameObject」方針に合わせるため)
//  ・プレイヤーの位置と3D速度を渡すと、ワイヤーの距離拘束を解いて返す
//  ・見た目(ワイヤーの線の描画)もここに持つ(Draw)
//
//=====================================================================
#pragma once

class KdSquarePolygon;
class CharaBase;

// ※ KdGameObjectはPch.hの強制インクルードで見えているため、継承でも明示includeは不要
class WireAction : public KdGameObject
{
public:

	// 板ポリ(見た目)をunique_ptr(前方宣言)で持つため、ctor/dtorは.cpp側で定義する
	WireAction();
	~WireAction();

	// ワイヤー中のスイング物理を、繋がっている全ワイヤーぶんまとめて1フレーム進める。
	// _body(どのキャラでも)の速度・位置を動かす：
	//   遮蔽判定 → 重力 → 積分 → 巻き取り → 距離拘束 → 操舵/漕ぎ → 当たり解決 → SetPos
	// 入力・狙いはキャラ側が決めてShoot/この呼び出しを行う(=部品として再利用可能)
	//  _moveInput.x ... 操舵(左右)。振り子の進行方向を横へ曲げる
	//  _moveInput.y ... 前方への漕ぎ。進行方向の接線へ加速する(ワイヤーは縮まない)
	//
	// 【なぜ static でまとめて受けるのか】
	//   重力・積分・当たり解決は「1フレームに1回だけ」でなければならない。
	//   ワイヤー1本ごとに呼ぶ形だと、2本掛けたときに重力が二重に掛かり積分も二重に進む。
	//   フレーム全体の流れはここが主導し、ワイヤー個々の処理(遮蔽/巻き取り/拘束)だけを
	//   本数ぶん回す、という分け方にしてある。1本でも2本でも同じ経路を通るので、
	//   本数を増やしても手触りの基準は1つのまま
	//
	// ※ たぐり寄せ(リール)は 2026/07/19 に一旦廃止。前進入力がワイヤーを縮めてしまい
	//    「前に行きたいのにアンカー側へ寄る」原因になっていたため。あとで別入力に割り当て直す予定
	// ※ Space/Ctrl単独の上下噴射も 2026/07/20 に廃止(ユーザー指示)。
	//    上への推進は「加速ボタン(右クリック)＋Space」に一本化した。
	//    噴射の入口が複数あると、同時押しで二重に効いて強さが読めなくなるため
	static void UpdateSwingAll(CharaBase& _body, float _dt, const Math::Vector2& _moveInput,
		WireAction* const* _wires, int _count);

	// ワイヤーの線(手元from→終点to)を描く。見た目もワイヤーの責務としてここに持つ。
	// スイング中はアンカーへ、グラップル突撃中は対象へ、とfrom/toは呼び出し側が決める
	void Draw(const Math::Vector3& _from, const Math::Vector3& _to);

	// 照準方向へワイヤーを撃ち、当たった地形をアンカー(固定点)にする
	//  _from      ... ワイヤーの発射位置(手元など)
	//  _dir       ... 発射方向(正規化済みを想定)
	//  _maxLength ... ワイヤーが届く最大距離
	//  戻り値     ... 命中してアンカーが決まったか
	bool Shoot(const Math::Vector3& _from, const Math::Vector3& _dir, float _maxLength);

	// ワイヤーを外す(拘束を解除する)
	void Release();

	// 今ワイヤーが繋がっているか
	bool IsAttached() const;

	// 毎フレームの拘束処理。_pos / _vel を拘束後の値に書き換える
	//  _reelInput ... +1でたぐり寄せ(縮む) / -1で伸ばす / 0で維持
	void Update(Math::Vector3& _pos, Math::Vector3& _vel, float _dt, float _reelInput);

	// アンカー(ワイヤーの先端)を取得する(描画側でワイヤーを引くのに使う)
	const Math::Vector3& GetAnchor() const;

	// アンカーが地面(TypeGround)に刺さっているか。
	// ワイヤー中は原則「着地しない」で地面スレスレを飛べるようにするが、
	// 地面そのものに刺した場合はそこへ降りるのが正しいので着地を許す判断に使う
	bool IsAnchorGround() const { return m_anchorIsGround; }

private:

	// --- 以下は UpdateSwingAll が本数ぶん回す「ワイヤー1本ぶんの処理」 ---

	// 手元とアンカーの間が壁で遮られ続けていたら外す(デバウンス付き)。
	// 外れたら false を返す(呼び出し側はそのワイヤーを以後スキップする)
	bool UpdateOcclusion(const CharaBase& _body, float _dt);

	// 巻き取り(ウインチ)。ワイヤー長を縮める。
	// 【重要】拘束を解く"前"に呼ぶこと。縮めてから拘束を解くと「張ったまま巻き取られる」状態になる
	void Winch(float _dt);

	// 補助的な半径方向の引き(既定は無効。調整値0で効かない)
	void ApplyRadialPull(const Math::Vector3& _pos, Math::Vector3& _vel, float _dt) const;

	// --- 必要なメンバのたたき台(自由に増減してよい) ---

	// 繋がっているか
	bool m_isAttached = false;

	// 固定点(ワイヤーの先端)
	Math::Vector3 m_anchor;

	// 拘束半径(=現在のワイヤー長)。これより外へは出られない
	float m_length = 0.0f;

	// 撃った瞬間のワイヤー長(リールアウトの上限。これ以上は伸ばせない)
	float m_maxLength = 0.0f;

	// 手元〜アンカー間が壁で遮蔽され続けている時間(デバウンス用)。
	// 一瞬のかすりで外れないよう、一定時間続いたら自動リリースする
	float m_occludedTime = 0.0f;

	// アンカーが地面(TypeGround)か。建物(TypeBump)ならfalse。Shootで判定して覚えておく。
	// ワイヤー中は原則「着地しない」で地面スレスレを飛べるようにするが、
	// 地面そのものに刺した場合はそこへ降りていくのが正しいので、通常どおり着地させる
	bool m_anchorIsGround = false;

	// ※ 自動離脱まわりの状態(m_releasePending / m_releasePendingTime / m_passedTime /
	//    m_prevVelY / m_swingTime)は 2026/07/20 に撤去した(ユーザー指示)。
	//    自動で外れる条件が「壁で遮蔽された時」だけになり、追い越し・到達距離・弧の底の
	//    判定が全部なくなったので、それらを測るための状態も丸ごと不要になった。
	//    戻すならこのコミットをrevertする

	// ワイヤーの見た目(板ポリを線に沿わせカメラへ向ける軸固定ビルボード)
	std::unique_ptr<KdSquarePolygon> m_upPoly;
};
