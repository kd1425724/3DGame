#pragma once

//====================================================
//
// EffectBase ── 短命VFXの基底(KdGameObject派生)
//
//  ・エフェクトもゲームオブジェクトとして扱う。KdGameObjectの
//    Update()/DrawUnLit()/IsExpired()/m_isExpired をそのまま使い、派生が実装する
//  ・寿命が尽きたら m_isExpired = true にする(EffectManagerがリストから外す)
//  ・EffectManagerが shared_ptr<EffectBase> のリストで一元管理する
//  ・将来、位置や当たり判定を持つエフェクト(ダメージ衝撃波など)にも拡張できる
//
//  ※ KdGameObjectはPch強制インクルードで見えるので明示includeは不要
//
//====================================================
class EffectBase : public KdGameObject
{
public:

	// エフェクトはワールドに配置される当たり対象ではないのでタグは持たない
	ObjectTag GetObjectTag() override { return ObjectTag::None; }

	// Update()/DrawUnLit() は KdGameObject の仮想関数を派生でoverrideする
	// (dtは他オブジェクト同様 Application::GetDeltaTime() から取る)
};
