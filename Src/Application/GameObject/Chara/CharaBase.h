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

	// 水平方向(xz)の速さを返す。HUDの速度メーターなど表示用
	float GetHorizontalSpeed() const
	{
		return sqrtf(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
	}

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

	// 高速移動で壁(TypeBump)を1フレームで飛び越える(トンネリング)のを防ぐ連続当たり判定。
	// フレーム開始位置fromPosから移動後posへ体の高さで水平レイを飛ばし、途中に壁があれば
	// その手前(半径ぶん外側)で止め、壁へ向かう水平速度を消す。ResolveBumpの前に呼ぶ安全弁。
	// ※ ワイヤーのスイング/フリングなど水平速度が大きいときに効く(低速時はResolveBumpで足りる)
	void ResolveBumpSweep(const Math::Vector3& fromPos, Math::Vector3& pos);

	// 接地中ならジャンプする(垂直速度に初速を与える。初速はDebugParams"キャラ/ジャンプ力")
	void Jump();

	// 接地しているか
	bool IsGrounded() const { return m_isGrounded; }

	// 直近フレームの着地/壁ヒットの衝撃の大きさを取り出す(読むと0に戻すConsume方式)。
	// カメラの手応え(CameraShake)の発火に使う。ResolveGround/ResolveBumpSweepが記録する。
	// ※ 記録は全キャラ共通だが、実際にカメラを揺らすのはPlayerだけ(Enemyは呼ばない)
	float ConsumeLandingImpact() { float v = m_landingImpact; m_landingImpact = 0.0f; return v; }
	float ConsumeWallImpact()    { float v = m_wallImpact;    m_wallImpact = 0.0f;    return v; }

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

	// 手応え用：直近フレームに発生した着地/壁ヒットの衝撃(速度の大きさ)。
	// ResolveGround(着地遷移時)/ResolveBumpSweep(壁で止めた時)が記録し、Consume〜で読み取る
	float m_landingImpact = 0.0f;
	float m_wallImpact = 0.0f;

	// ※ 重力加速度・ジャンプ初速はDebugParams("キャラ/重力"・"キャラ/ジャンプ力")で調整する
};
