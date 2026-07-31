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

	// 色を粒ごとに上書きする(渡さなければ噴射ジェットの色をDebugParamsから読む)。
	//
	// 【なぜ必要になったか】
	//   この粒は噴射ジェットとして作られたが、火花にも流用している。
	//   色はDebugParams「加速エフェクト/発光色」を全粒で共有していたため、
	//   火花もジェットと同じ淡い青白＝銀色に見えていた(ユーザー指摘)。
	//   用途ごとに色を変えられるようにする
	void SetEmissiveOverride(const Math::Vector3& _emissive);

	void Update() override;      // 流しながら経過を進め、寿命で m_isExpired = true
	void DrawUnLit() override;   // 縮みながらフェードして描画

private:

	Math::Vector3 m_pos;
	Math::Vector3 m_vel;   // 流れる速度
	float m_age  = 0.0f;
	float m_life = 0.35f;

	// 色の上書き。未設定ならDebugParamsの噴射色を使う
	bool m_hasEmissiveOverride = false;
	Math::Vector3 m_emissiveOverride = {};

	// 全BoostEffectで共有する板ポリ(初回描画時に生成)
	static KdSquarePolygon* GetSharedPoly();
	static std::unique_ptr<KdSquarePolygon> s_upPoly;
};
