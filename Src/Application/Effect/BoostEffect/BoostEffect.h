#pragma once

#include "../EffectBase.h"

//====================================================
//
// BoostEffect ── 加速中に後方へ流れる噴射VFX(EffectBase派生)
//
//  ・加速している間、プレイヤーの少し後ろに連続で置かれる粒
//  ・置かれた後は「加速と逆向き」に流れながら縮んでフェードする
//    (自分は前へ進むので、その場に置いて逆へ流すと後方へ吹き出しているように見える)
//  ・板ポリは全BoostEffectで1枚を共有する(毎回作らない)＝SlashEffectと同じ流儀
//
//====================================================
class KdSquarePolygon;

class BoostEffect : public EffectBase
{
public:

	// 粒の用途。この粒は噴射ジェットとして作られたが火花にも流用しているので、
	// テクスチャ・色・大きさ・濃さ・寿命を用途ごとに分ける
	enum class Style
	{
		Jet,     // 加速の噴射。ふんわりした丸い粒(Particle.png)
		Spark,   // 火花。硬い芯を持つ粒(Spark.png)
	};

	// _pos   … 発生位置(プレイヤーの少し後ろ)
	// _vel   … 粒が流れていく速度(加速方向の逆向きを想定)
	// _style … 用途。寿命を生成時に決めるのでコンストラクタで受け取る
	BoostEffect(const Math::Vector3& _pos, const Math::Vector3& _vel, Style _style = Style::Jet);
	~BoostEffect() override;

	void Update() override;      // 流しながら経過を進め、寿命で m_isExpired = true
	void DrawUnLit() override;   // 縮みながらフェードして描画

private:

	Math::Vector3 m_pos;
	Math::Vector3 m_vel;   // 流れる速度
	float m_age  = 0.0f;
	float m_life = 0.35f;
	Style m_style = Style::Jet;

	// 用途ごとに共有する板ポリ(初回描画時に生成)。テクスチャが違うので別々に持つ
	static KdSquarePolygon* GetSharedPoly(Style _style);
	static std::unique_ptr<KdSquarePolygon> s_upPolyJet;
	static std::unique_ptr<KdSquarePolygon> s_upPolySpark;
};
