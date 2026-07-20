//=====================================================================
//
//  WallAction ── 壁走り／壁ジャンプを担う部品
//
//  ・KdGameObject派生。ただしシーンのオブジェクトリストには入れず、Playerなどが
//    「部品」として1つ所有し、親がUpdateを直接呼んで駆動する(WireActionと同じ流儀)
//  ・当たり判定は撃たない。CharaBase::ResolveBumpが押し出しのために既に求めている
//    壁の法線(m_isTouchingWall / m_wallNormal)を読むだけで成立する＝追加の負荷が無い
//  ・壁走り中は重力を止め(m_gravityScale=0)、ゆっくり下降させる
//
//  【操作】
//   ・壁走り … 自動。空中で壁に十分な速度で沿って接触すると発動する
//   ・壁ジャンプ … 壁走り中にJump(Space)
//
//=====================================================================
#pragma once

class CharaBase;

// ※ KdGameObjectはPch.hの強制インクルードで見えているため、継承でも明示includeは不要
class WallAction : public KdGameObject
{
public:

	WallAction() {}
	~WallAction() override {}

	// 壁走りの更新。毎フレーム呼ぶ。
	//  ・条件が揃えば自動で壁走りを開始し、走行中は_bodyの速度と重力倍率を書き換える
	//  ・走行中にJumpが押されたら壁ジャンプして走行を終える
	//  ・_wishDir が壁を向いていれば、ずり落ちる代わりによじ登る
	// ※ 「意思決定」なので親のUpdate()から呼ぶ。実際の移動・当たり解決は
	//    PostUpdateのGroundCheckが行う(CLAUDE.mdの Update=意思決定 / PostUpdate=解決 に従う)
	//  _wishDir ... 移動入力の向き(カメラ基準・水平・正規化済み)。無入力ならゼロ
	void Update(CharaBase& _body, float _dt, const Math::Vector3& _wishDir);

	// 今壁を走っているか(親が通常移動やジャンプを止める判断に使う)
	bool IsRunning() const { return m_isRunning; }

	// 今よじ登っているか(壁を向いて前入力している状態)。エフェクトや表示の出し分け用
	bool IsClimbing() const { return m_isClimbing; }

	// 壁走りを強制終了する(ワイヤー発射・突撃・リスポーンなど、他の行動へ移る時に呼ぶ)
	void Cancel(CharaBase& _body);

	// 走っている壁の法線(水平・正規化済み)。エフェクトやカメラの傾きに使う想定
	const Math::Vector3& GetWallNormal() const { return m_wallNormal; }

private:

	// 壁走りを開始できるか調べる(空中・速度・壁に沿っているか・同じ壁の再吸着でないか)
	bool CanStart(const CharaBase& _body) const;

	// 壁走りを始める
	void Start(CharaBase& _body);

	// 壁走りを終える(重力を戻す)。_lockSameWall=trueなら今の壁への再吸着を禁止する
	void Stop(CharaBase& _body, bool _lockSameWall);

	// 壁ジャンプ：壁の法線方向へ蹴り出し＋上向き＋壁に沿った勢いを一部維持する
	void WallJump(CharaBase& _body);

	// 壁を擦っている火花を出す(壁走り中であることを見て分かるようにするため)。
	// 時間あたりの個数で制御するのでフレームレートに依らない
	void SpawnFx(const CharaBase& _body, float _dt);

	// 走行中か
	bool m_isRunning = false;

	// よじ登り中か(壁を向いて前入力している)。走行中だけ意味を持つ
	bool m_isClimbing = false;

	// 走り出してからの経過時間(上限を超えたら剥がれる)
	float m_runTime = 0.0f;

	// 火花の発生間隔を測るタイマー(時間あたりの個数を一定に保つ)
	float m_fxTimer = 0.0f;

	// 走っている壁の法線(水平・正規化済み)
	Math::Vector3 m_wallNormal = {};

	// 壁から離れてから再び壁走りできるまでの残り時間。
	// これが無いと壁ジャンプした直後に同じ壁へ吸着し直してしまう
	float m_cooldown = 0.0f;

	// 最後に壁へ触れてからの経過時間と、その時の法線。
	// ResolveBumpは「めり込んでいる時」だけ接触を立てるが、押し出した直後は
	// めり込みが0になるので、壁沿いに動いていると接触フラグが点滅する。
	// そのまま条件にすると壁走りがほとんど始まらないため、少しの間は
	// 「触れている」とみなす猶予(接地のコヨーテタイムと同じ考え方)
	float m_sinceTouch = 999.0f;
	Math::Vector3 m_lastTouchNormal = {};

	// 直前に蹴った/剥がれた壁の法線と、その壁を禁止しているか。
	// 同じ壁に何度も張り付いて無限に登れてしまうのを防ぐ。
	// 接地する、または別の壁に取り付くと解除される
	// (=2枚の壁を交互に蹴ってジグザグに登るのは成立する)
	bool m_hasBannedWall = false;
	Math::Vector3 m_bannedWallNormal = {};
};
