#pragma once

#include "EffectBase.h"

//====================================================
//
// SlashEffect ── 斬った位置に一瞬出る斬撃VFX(EffectBase派生)
//
//  ・カメラを向くビルボードの板ポリを、面内回転させつつ拡大＋フェードで描く
//  ・板ポリは全SlashEffectで1枚を共有する(毎回作らない)
//
//====================================================
class KdSquarePolygon;

class SlashEffect : public EffectBase
{
public:

	// 位置と面内回転(ラジアン)を指定して生成。寿命はDebugParamsから取る
	SlashEffect(const Math::Vector3& _pos, float _rot);
	~SlashEffect() override;

	void Update(float _dt) override;
	void DrawUnLit() override;
	bool IsFinished() const override { return m_age >= m_life; }

private:

	Math::Vector3 m_pos;
	float m_rot  = 0.0f;   // 面内回転(ラジアン)
	float m_age  = 0.0f;   // 経過
	float m_life = 0.14f;  // 寿命(秒)

	// 全SlashEffectで共有する板ポリ(初回描画時に生成)
	static KdSquarePolygon* GetSharedPoly();
	static std::unique_ptr<KdSquarePolygon> s_upPoly;
};
