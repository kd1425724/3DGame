//=====================================================================
//
//  WireAction ── ワイヤーアクションの物理(距離拘束)＋見た目を担う部品
//
//  ・KdGameObject派生。ただしシーンのオブジェクトリストには入れず、Playerなどが
//    「部品」として1つ所有し、親がUpdateSwing/Draw/Shootを直接呼んで駆動する
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

	// ワイヤー中のスイング物理。接続中に毎フレーム呼び、_body(どのキャラでも)の速度・位置を
	// 動かす：重力→積分→距離拘束→操舵/漕ぎ→自動リリース判定→当たり解決→SetPos。
	// 入力・狙いはキャラ側が決めてShoot/この呼び出しを行う(=部品として再利用可能)
	//  _moveInput.x ... 操舵(左右)。振り子の進行方向を横へ曲げる
	//  _moveInput.y ... 前方への漕ぎ。進行方向の接線へ加速する(ワイヤーは縮まない)
	// ※ たぐり寄せ(リール)は 2026/07/19 に一旦廃止。前進入力がワイヤーを縮めてしまい
	//    「前に行きたいのにアンカー側へ寄る」原因になっていたため。あとで別入力に割り当て直す予定
	// ※ Space/Ctrl単独の上下噴射も 2026/07/20 に廃止(ユーザー指示)。
	//    上への推進は「加速ボタン(右クリック)＋Space」に一本化した。
	//    噴射の入口が複数あると、同時押しで二重に効いて強さが読めなくなるため
	void UpdateSwing(CharaBase& _body, float _dt, const Math::Vector2& _moveInput);

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

private:

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

	// 離脱条件は揃ったが「上を向く」のを待っている状態と、その待ち時間。
	// 下降中に切るとそのまま落ちるだけになるので、上昇に転じてから実際に外す
	// (アンカーが下や真横だと永久に上を向かないので待ちには上限を設ける)
	bool  m_releasePending = false;
	float m_releasePendingTime = 0.0f;

	// アンカーを追い越して(遠ざかり始めて)からの経過時間。
	// 追い越した瞬間に外すと「超えてすぐ切れる」不自然さが出るので、
	// 一定時間そのまま引かれ続けてから外す(遮蔽判定と同じデバウンスの考え方)
	float m_passedTime = 0.0f;

	// 弧の底を通過した瞬間(下降→上昇へ転じた瞬間)を検出するための、前フレームの垂直速度
	float m_prevVelY = 0.0f;

	// このワイヤーに繋がってからの経過時間。撃った直後に自動リリースが暴発しないよう、
	// 一定時間が経つまでは底の通過を判定しない
	float m_swingTime = 0.0f;

	// ワイヤーの見た目(板ポリを線に沿わせカメラへ向ける軸固定ビルボード)
	std::unique_ptr<KdSquarePolygon> m_upPoly;
};
