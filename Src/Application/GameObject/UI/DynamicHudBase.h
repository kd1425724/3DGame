#pragma once

//====================================================
//
// DynamicHudBase ── コード駆動の動的HUD要素の基底(KdGameObject派生)
//
//  ・実データを毎フレーム読んで描くHUD(速度メーター/HPバー/撃破数など)の共通基底。
//    シーンのオブジェクトリストに入れ、BaseScene::DrawSprite(2D描画パス)で描かれる
//    → 派生は DrawHud() に2D描画(KdSpriteShader)を書くだけでよい。
//    スプライトのBegin/EndはDrawSpriteパスが囲むので派生側では呼ばない
//  ・エディタ配置の静止画HudSprite(別系統)とは別。こちらはコード駆動の動的HUD
//
//====================================================
class DynamicHudBase : public KdGameObject
{
public:

	// BaseScene::DrawSprite の Begin〜End 内で呼ばれる。中身は DrawHud() に委譲する
	void DrawSprite() override
	{
		if (!m_visible) { return; }
		DrawHud();
	}

	void SetVisible(bool _visible) { m_visible = _visible; }

protected:

	// 派生クラスが実際の2D描画を実装する(スプライトのBegin/Endは呼ばない)
	virtual void DrawHud() {}

	// HUD要素の表示ON/OFF
	bool m_visible = true;
};
