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

	// _pos … 発生位置(プレイヤーの少し後ろ)
	// _vel … 粒が流れていく速度(加速方向の逆向きを想定)
	BoostEffect(const Math::Vector3& _pos, const Math::Vector3& _vel);
	~BoostEffect() override;

	void Update() override;      // 流しながら経過を進め、寿命で m_isExpired = true
	void DrawUnLit() override;   // 縮みながらフェードして描画

private:

	Math::Vector3 m_pos;
	Math::Vector3 m_vel;   // 流れる速度
	float m_age  = 0.0f;
	float m_life = 0.35f;

	// 全BoostEffectで共有する板ポリ(初回描画時に生成)
	static KdSquarePolygon* GetSharedPoly();
	static std::unique_ptr<KdSquarePolygon> s_upPoly;
};
