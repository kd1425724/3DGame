#pragma once

#include "../DynamicHudBase.h"

//====================================================
//
// GameHud ── ゲーム本編の動的HUD(コード駆動)。今はプレイヤーの速度メーター
//
//  ・DynamicHudBase(KdGameObject派生)を継承。DrawHud()に2D描画を書く
//  ・シーンに常駐し、BaseScene::DrawSprite(2D描画パス)で描かれる
//    (シングルトンではなく、GameScene::Initでシーンに1つ追加する)
//  ・エディタで配置するHudSprite(静止画)とは別系統。こちらは実データを毎フレーム読む動的HUD
//  ・描画は2D箱(KdSpriteShader::DrawBox)で行うので画像アセット不要
//  ・表示位置/サイズ/最大速度はDebugParams、表示ON/OFFはDebugFlagsで調整
//
//====================================================
class GameHud : public DynamicHudBase
{
protected:

	// 動的HUDを描く(DynamicHudBase::DrawSpriteから呼ばれる)
	void DrawHud() override;

private:

	// プレイヤーの速度メーター(横バー)を描く
	void DrawSpeedMeter();

	// 画面中央のレティクル(照準)を描く。種類A/B/CはDebugParamsで切り替えられる
	void DrawReticle();
};
