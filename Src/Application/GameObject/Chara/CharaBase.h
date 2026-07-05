#pragma once

//====================================================
//
// 全キャラクター(Player/Enemyなど)の基底クラス
//  ・モデル表示(色味を変えて種類を見分けられるようにcolRateを持つ)
//  ・下方向へのレイ判定で地面(KdCollider::TypeGround)に立つ機能
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

	// 現在地から下方向にレイを飛ばし、地面(KdCollider::TypeGround)に当たったらその高さに立たせる
	void GroundCheck();

	// 表示用モデルワーク
	KdModelWork m_modelWork;

	// 描画時の色味(サブクラスで変更して他のキャラクターと見分けられるようにする)
	Math::Color m_color = kWhiteColor;
};
