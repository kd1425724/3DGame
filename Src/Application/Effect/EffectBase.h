#pragma once

//====================================================
//
// EffectBase ── 短命VFX1つぶんの基底クラス
//
//  ・派生クラス(SlashEffect等)が見た目・寿命を実装する
//  ・EffectManagerが unique_ptr<EffectBase> のリストで一元管理する
//    (毎フレームUpdate→IsFinishedで除去、UnLitパスでDrawUnLit)
//
//====================================================
class EffectBase
{
public:

	virtual ~EffectBase() {}

	// 経過を進める(寿命やアニメを進行)
	virtual void Update(float _dt) = 0;

	// 陰影なしパスで描画する
	virtual void DrawUnLit() = 0;

	// 寿命が尽きたか(trueならEffectManagerがリストから外す)
	virtual bool IsFinished() const = 0;
};
