#pragma once

//====================================================
//
// GameHud ── ゲーム本編の画面HUD(コード駆動)を描くシングルトン
//
//  ・エディタで配置するHudSprite(静止画)とは別系統。こちらは実データを
//    毎フレーム読んで描く動的なHUD(まずはプレイヤーの速度メーター)
//  ・描画は2D箱(KdSpriteShader::DrawBox)で行うので画像アセット不要
//  ・表示位置/サイズ/最大速度はDebugParams、表示ON/OFFはDebugFlagsで調整
//
//  ※ シングルトンだがコンストラクタからInitは呼ばない(自己再入回避)
//
//====================================================
class GameHud
{
public:

	static GameHud& Instance()
	{
		static GameHud instance;
		return instance;
	}

	// HUDを描画する(Application::DrawSpriteから呼ぶ。内部でスプライトのBegin/Endを行う)
	void Draw();

private:

	GameHud() = default;
	~GameHud() = default;
	GameHud(const GameHud&) = delete;
	void operator=(const GameHud&) = delete;

	// プレイヤーの速度メーター(横バー)を描く
	void DrawSpeedMeter();
};
