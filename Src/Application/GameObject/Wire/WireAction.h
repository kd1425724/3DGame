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
	// スイング中はアンカーへ、グラップル突撃中は対象へ、とfrom/toは呼び出し側が決める。
	//
	// 【1枚の板ではなく複数の節に分けて描く理由が2つある】
	//   ①たわみ … 張っていないときに垂らすには折れ線でないと表現できない
	//   ②模様の繰り返し … 板1枚だと、長いワイヤーでテクスチャが引き伸ばされて
	//     撚りが見えなくなる。節に分ければ各節が模様を1回ずつ持つので密度が保たれる。
	//     UVをいじる方法(SetUVTiling)もあるが、あれはシェーダのグローバル状態なので
	//     戻し忘れると以降の全描画が狂う。節に分ければそこに触らずに済む
	void Draw(const Math::Vector3& _from, const Math::Vector3& _to);

	// ワイヤー先端のフックを描く。_from は射出口(手元)で、フックの向きを決めるのに使う。
	// ※ Drawと分けてあるのは、グラップル突撃の線(対象へ引く線)にはフックを出さないため
	void DrawHook(const Math::Vector3& _from);

	// 指定方向へレイを飛ばし、ワイヤーを取り付けられる面(建物TypeBump / 地面TypeGround)の
	// 最も手前の交点を返す。当たらなければ false。
	// 「何をアンカーにできるか」の定義をここ1箇所に置くための共通処理で、
	// Shoot(実際に取り付ける) と アンカー探索(どこへ撃つか選ぶ) の両方から使う
	static bool CastAnchor(const Math::Vector3& _from, const Math::Vector3& _dir, float _maxLength,
		Math::Vector3& _outPos, bool& _outIsGround, float& _outDist);

	// 照準方向へワイヤーを撃ち、当たった地形をアンカー(固定点)にする。
	// アンカーはこの瞬間にレイで確定するが、拘束はまだ効かせない：フックが飛んで
	// 着弾するまで待つ(UpdateFlight)。「撃ったのに即座に引っ張られる」と因果が逆に見えるため
	//  _from      ... ワイヤーの発射位置(手元など)
	//  _dir       ... 発射方向(正規化済みを想定)
	//  _maxLength ... ワイヤーが届く最大距離
	//  戻り値     ... 命中してアンカーが決まったか(=飛行を開始したか)
	bool Shoot(const Math::Vector3& _from, const Math::Vector3& _dir, float _maxLength);

	// 敵の関節へ撃つ(ロックオン中の的へのアンカー射出)。
	// 地形と違いアンカーが【動く】ので、相手と関節番号を覚えて毎フレーム追従させる。
	// レイは飛ばさない：狙う先はロックオンで既に確定しているため、途中に何があっても掛かる
	//  _spTarget    ... 刺さる相手(敵)
	//  _jointIndex  ... Enemy::kJointDefs の添字
	//  _jointPos    ... 撃った時点の関節のワールド座標(飛行の到達点として使う)
	bool ShootAtJoint(const Math::Vector3& _from, const std::shared_ptr<KdGameObject>& _spTarget,
		int _jointIndex, const Math::Vector3& _jointPos);

	// 敵の関節に刺さっている(または刺さりに飛んでいる)か
	bool IsJointAnchor() const { return m_anchorJointIndex >= 0; }

	// 飛行(発射から着弾まで)を1フレーム進め、到達したら拘束を開始する。
	//
	// 【なぜ UpdateSwingAll と別の入口なのか】
	//   UpdateSwingAll は「繋がっているワイヤーが1本も無ければ即return」する。
	//   飛行中はまだ繋がっていないので、そこに混ぜると飛行が進まない。
	//   繋がっていない時も呼ばれる必要があるため独立させてある。
	//   また、着弾したフレームにそのままスイングへ入れるよう、キャラ側の
	//   「ワイヤー中か」の分岐より前に呼ぶこと
	//
	//  _bodyPos   ... 現在のキャラの位置。着弾時のワイヤー長をここから決める
	//  _muzzlePos ... 射出口。巻き戻し中はフックがここへ帰ってくる
	//  戻り値     ... このフレームで着弾したか(着弾の演出を出すのに使う)
	bool UpdateHookMotion(const Math::Vector3& _bodyPos, const Math::Vector3& _muzzlePos, float _dt);

	// 撃った瞬間の射出点。着弾の火花を飛ばす向き(＝フックが進んだ向き)を出すのに使う
	const Math::Vector3& GetLaunchPos() const { return m_launchPos; }

	// ワイヤーを外す(拘束を解除する)。飛行中のフックも取り消す。
	//  _animate ... trueならフックが射出口へ帰る見た目を再生する(拘束は即座に切れる)。
	//
	// 【既定をfalseにしている理由】
	//   自動リリース(手元とアンカーの間が壁で遮られた場合)で演出を出すと、
	//   フックが【壁を貫通して】戻ってくる。その遮蔽を避けるための機能なので本末転倒になる。
	//   突撃への移行やリスポーンのリセットも、見た目を引きずらせたくないので即座に消す。
	//   演出を出すのは「プレイヤーが自分でボタンを離した」時だけ
	void Release(bool _animate = false);

	// 今ワイヤーが繋がっているか
	bool IsAttached() const;

	// 飛行中(撃ったが、まだ着弾していない)か
	bool IsFlying() const { return m_isFlying; }

	// 巻き戻し中(外れた後、フックが射出口へ帰る見た目を再生中)か。物理には一切影響しない
	bool IsRetracting() const { return m_isRetracting; }

	// 見た目の線とフックを描くべき状態か。
	// ※ 「拘束が効いているか」ではないので物理の判断には使わないこと(巻き戻し中も真になる)
	bool IsVisible() const { return m_isFlying || m_isAttached || m_isRetracting; }

	// フック先端の現在位置。飛行中は射出点からアンカーへの途中、着弾後はアンカー、
	// 巻き戻し中は射出口へ帰る途中。UpdateHookMotionが毎フレーム決めた値を返す
	const Math::Vector3& GetHookPos() const { return m_hookPos; }

	// 毎フレームの拘束処理。_pos / _vel を拘束後の値に書き換える
	//  _reelInput ... +1でたぐり寄せ(縮む) / -1で伸ばす / 0で維持
	void Update(Math::Vector3& _pos, Math::Vector3& _vel, float _dt, float _reelInput);

	// アンカー(ワイヤーの先端)を取得する(描画側でワイヤーを引くのに使う)
	const Math::Vector3& GetAnchor() const;

	// --- デバッグ表示用。プレイヤーが閉じ込められている範囲を可視化するのに使う ---

	// 現在のワイヤー長(＝1本で拘束しているときの球の半径)
	float GetLength() const { return m_length; }

	// 2本を1つの球にまとめている状態か。まとめている側(先頭のワイヤー)だけがtrue
	bool IsMerged() const { return m_hasMerged; }

	// まとめた球の中心と半径(IsMerged()がtrueのときだけ意味がある)
	const Math::Vector3& GetMergedPivot() const { return m_mergedPivot; }
	float GetMergedRadius() const { return m_mergedRadius; }

	// アンカーが地面(TypeGround)に刺さっているか。
	// ワイヤー中は原則「着地しない」で地面スレスレを飛べるようにするが、
	// 地面そのものに刺した場合はそこへ降りるのが正しいので着地を許す判断に使う
	bool IsAnchorGround() const { return m_anchorIsGround; }

private:

	// --- 以下は UpdateSwingAll が本数ぶん回す「ワイヤー1本ぶんの処理」 ---

	// 手元とアンカーの間が壁で遮られ続けていたら外す(デバウンス付き)。
	// 外れたら false を返す(呼び出し側はそのワイヤーを以後スキップする)
	bool UpdateOcclusion(const CharaBase& _body, float _dt);

	// 関節に刺さっているアンカーを、今の関節位置へ追従させる。
	// 相手が消えた／関節が壊れたら外して false を返す。
	// 地形アンカー(添字-1)のときは何もせず true
	//
	// 【なぜ必要か】このクラスは元々「アンカーは動かない」前提で書かれていた
	// (地形=建物と地面しか刺さらなかったため)。敵に刺さるようになって前提が崩れたので、
	// 拘束を解く【前】に毎フレーム位置を合わせ直す
	bool UpdateAnchorFollow();

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

	// アンカーを追従させる相手(敵)と、その関節番号。地形に刺さっているときは添字-1。
	// 【なぜweak_ptrか】ワイヤーが敵の寿命を延ばしてはいけない(倒したのに残る)
	std::weak_ptr<KdGameObject> m_wpAnchorTarget;
	int m_anchorJointIndex = -1;

	// アンカーが地面(TypeGround)か。建物(TypeBump)ならfalse。Shootで判定して覚えておく。
	// ワイヤー中は原則「着地しない」で地面スレスレを飛べるようにするが、
	// 地面そのものに刺した場合はそこへ降りていくのが正しいので、通常どおり着地させる
	bool m_anchorIsGround = false;

	// --- 発射から着弾までの飛行。見た目だけで、当たり判定はやり直さない ---
	//
	// 【なぜ飛翔体を実体化しないのか】
	//   地形アンカーにできるのは建物(TypeBump)と地面(TypeGround)だけで、どちらも動かない。
	//   さらに向きの決定(キャラ側のアンカー探索)は撃つ瞬間に扇状のレイで行っている。
	//   つまりフックを実体として飛ばして毎フレーム判定しても、撃った瞬間のレイと
	//   必ず同じ答えになる。得るものが無く、「探索の答えが飛行中に無効になる」
	//   リスクだけが増えるので、飛ぶのは見た目(線とフック先端)のみとした。
	//
	//   🔴 【2026/08/02 追記】敵の関節に刺せるようになり、この前提は【関節アンカーには
	//   当てはまらない】。関節は歩行とアニメで毎フレーム動くため。
	//   ただし関節アンカーはレイで探さない(ロックオンで的が確定済み)ので、
	//   「飛行中に探索の答えが無効になる」問題は起きない。
	//   飛行中は到達点を毎フレーム今の関節位置へ更新して追いかける(UpdateAnchorFollow)
	bool m_isFlying = false;

	// 飛行の経過時間と、着弾までの所要時間
	float m_flightTime = 0.0f;
	float m_flightDuration = 0.0f;

	// 撃った瞬間の射出口。フック先端はここからアンカーへ向かって進む。
	// 射出口(手元)は毎フレーム動くが、フックが飛び始めた点は動かないので別に覚える
	Math::Vector3 m_launchPos = {};

	// --- 巻き戻し(外れた後、フックが射出口へ帰る見た目)。物理には一切影響しない ---
	// 拘束は Release の時点で既に切れていて、ここで動かすのは見た目だけ
	bool m_isRetracting = false;
	float m_retractTime = 0.0f;
	float m_retractDuration = 0.0f;

	// 外れた瞬間のフックの位置。ここから射出口へ向かって帰る
	Math::Vector3 m_retractFrom = {};

	// フック先端の現在位置。飛行/接続/巻き戻しのどの状態かに応じて
	// UpdateHookMotion が毎フレーム決める。描画側はこれを読むだけでよい
	Math::Vector3 m_hookPos = {};

	// ※ 自動離脱まわりの状態(m_releasePending / m_releasePendingTime / m_passedTime /
	//    m_prevVelY / m_swingTime)は 2026/07/20 に撤去した(ユーザー指示)。
	//    自動で外れる条件が「壁で遮蔽された時」だけになり、追い越し・到達距離・弧の底の
	//    判定が全部なくなったので、それらを測るための状態も丸ごと不要になった。
	//    戻すならこのコミットをrevertする

	// 2本掛けを「1つの球」で解くときの仮想アンカー(先頭のワイヤーが代表して持つ)。
	// 2本が揃った瞬間に1回だけ決めて、あとは半径を巻き取るだけにする。
	// 毎フレーム2本の長さから計算し直すと、左右の長さの差で支点がじわじわ動き
	// 「片方へ引かれる」「ガクッと跳ぶ」原因になる(実際にその不具合を出した)
	bool m_hasMerged = false;
	Math::Vector3 m_mergedPivot = {};
	float m_mergedRadius = 0.0f;

	// ワイヤー1節ぶんの板ポリ(線に沿わせカメラへ向ける軸固定ビルボード)。
	// 節の数だけ位置と長さを変えて使い回す(節ごとに実体を持つ必要はない)
	std::unique_ptr<KdSquarePolygon> m_upPoly;

	// 先端のフック用の板ポリ。ワイヤー本体とテクスチャが違うので別に持つ
	std::unique_ptr<KdSquarePolygon> m_upHookPoly;

	// 線の1節を描く(Drawが節の数だけ呼ぶ)。
	// 太さと色を引数で受けるのは、DebugParamsの読み取りを節ごとに繰り返さないため
	void DrawSegment(const Math::Vector3& _from, const Math::Vector3& _to,
		float _thickness, const Math::Color& _col, const Math::Vector3& _emissive);
};
