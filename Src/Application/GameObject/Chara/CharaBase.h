#pragma once

//====================================================
//
// 全キャラクター(Player/Enemyなど)の基底クラス
//  ・モデル表示(色味を変えて種類を見分けられるようにcolRateを持つ)
//  ・重力＋垂直速度を持ち、下方向レイで地面(KdCollider::TypeGround)に着地する
//  ・ジャンプ対応：上昇中や空中では地面に吸着せず、落下して着地したときだけ立たせる
//
//====================================================
class CharaBase : public KdGameObject
{
public:

	CharaBase()				{}
	virtual ~CharaBase() override {}

	void SetAsset(const std::string& assetName) override;

	void DrawLit() override;

protected:

	// 重力を適用して垂直移動し、落下中に地面(TypeGround)へ着地したらその高さに立たせる
	// ※ PostUpdate等の「world状態の解決」フェーズで毎フレーム呼ぶ想定
	void GroundCheck();

	// 移動後の位置posを受け取り、地面(TypeGround)に潜っていたら立つ高さへ押し上げて
	// 縦の落下を止める。接地状態(m_isGrounded)も更新する。
	// ※ 重力・移動の積分は呼び出し側で済ませておくこと(この関数は地面との"解決"だけ行う)
	// ※ ワイヤー中など、GroundCheckを通さず自前で移動するときのすり抜け防止にも使う
	void ResolveGround(Math::Vector3& pos);

	// 体を球で近似し、Block等(TypeBump)にめり込んでいたら水平方向へ押し出す(壁として働く)
	// ※ 縦の乗り上げ/着地はResolveGroundが担当。ワイヤー中もすり抜け防止に使う
	void ResolveBump(Math::Vector3& pos);

	// 接地中ならジャンプする(垂直速度に初速を与える。初速はDebugParams"キャラ/ジャンプ力")
	void Jump();

	// 接地しているか
	bool IsGrounded() const { return m_isGrounded; }

	// 表示用モデルワーク
	KdModelWork m_modelWork;

	// 描画時の色味(サブクラスで変更して他のキャラクターと見分けられるようにする)
	Math::Color m_color = kWhiteColor;

	// 速度(3D)。重力・ジャンプ・移動・ワイヤーが共通で使う
	// ・y … 重力/ジャンプ(GroundCheckが積分し、着地で0に戻す)
	// ・x,z … 水平移動。各キャラのUpdateが設定する(Enemyは使わず0のまま)
	// GroundCheckがこの速度でまとめて位置を進めるので、スイング後の勢いも自然に残る
	Math::Vector3 m_velocity = {};

	// 接地しているか(GroundCheckで毎フレーム更新)
	bool m_isGrounded = false;

	// ※ 重力加速度・ジャンプ初速はDebugParams("キャラ/重力"・"キャラ/ジャンプ力")で調整する
};
