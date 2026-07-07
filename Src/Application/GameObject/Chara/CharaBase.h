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

	// 接地中ならジャンプする(垂直速度に初速を与える。初速はDebugParams"キャラ/ジャンプ力")
	void Jump();

	// 接地しているか
	bool IsGrounded() const { return m_isGrounded; }

	// 表示用モデルワーク
	KdModelWork m_modelWork;

	// 描画時の色味(サブクラスで変更して他のキャラクターと見分けられるようにする)
	Math::Color m_color = kWhiteColor;

	// 垂直方向の速度(上向きが+)。重力とジャンプで変化する
	float m_verticalVelocity = 0.0f;

	// 接地しているか(GroundCheckで毎フレーム更新)
	bool m_isGrounded = false;

	// ※ 重力加速度・ジャンプ初速はDebugParams("キャラ/重力"・"キャラ/ジャンプ力")で調整する
};
