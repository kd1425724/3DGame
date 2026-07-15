#include "GameHud.h"

#include "../../../Scene/SceneManager.h"
#include "../../../GameObject/Chara/CharaBase.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugFlags/DebugFlags.h"

void GameHud::Draw()
{
	// 2D箱の描画はスプライトシェーダのBegin/Endで囲む
	KdShaderManager::Instance().m_spriteShader.Begin();
	DrawSpeedMeter();
	KdShaderManager::Instance().m_spriteShader.End();
}

void GameHud::DrawSpeedMeter()
{
	if (!DebugFlags::Instance().Get(U8("HUD/速度メーター表示"), true)) { return; }

	// プレイヤーを種別タグで探し、水平速度を取得する
	std::shared_ptr<CharaBase> spPlayer =
		std::dynamic_pointer_cast<CharaBase>(SceneManager::Instance().FindObjectWithTag(KdGameObject::ObjectTag::Player));
	if (!spPlayer) { return; }

	float speed = spPlayer->GetHorizontalSpeed();

	// 位置・サイズ・最大速度はDebugParamsで調整(座標は画面中央が原点)
	int   x        = static_cast<int>(DebugParams::Instance().Float(U8("HUD/速度メーターX"),       -500.0f, -640.0f, 640.0f));
	int   y        = static_cast<int>(DebugParams::Instance().Float(U8("HUD/速度メーターY"),        300.0f, -360.0f, 360.0f));
	int   w        = static_cast<int>(DebugParams::Instance().Float(U8("HUD/速度メーター幅"),        240.0f,   10.0f, 800.0f));
	int   h        = static_cast<int>(DebugParams::Instance().Float(U8("HUD/速度メーター高さ"),       22.0f,    2.0f, 100.0f));
	float maxSpeed =                  DebugParams::Instance().Float(U8("HUD/速度メーター最大速度"),   25.0f,    1.0f, 100.0f);

	float ratio = speed / maxSpeed;
	if (ratio < 0.0f) { ratio = 0.0f; }
	if (ratio > 1.0f) { ratio = 1.0f; }

	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	// 背景バー(暗い箱)。DrawBoxは中心座標＋ハーフサイズ指定
	Math::Color bgColor(0.0f, 0.0f, 0.0f, 0.5f);
	sprite.DrawBox(x, y, w / 2, h / 2, &bgColor, true);

	// 塗りバー(左端から速度比ぶんだけ伸ばす。速いほど水色→黄色寄りに)
	int fillW = static_cast<int>(w * ratio);
	if (fillW > 0)
	{
		int left        = x - w / 2;
		int fillCenterX = left + fillW / 2;
		Math::Color fillColor(0.3f + 0.7f * ratio, 0.9f, 1.0f - 0.6f * ratio, 0.9f);
		sprite.DrawBox(fillCenterX, y, fillW / 2, h / 2, &fillColor, true);
	}

	// 枠(白い輪郭)
	Math::Color lineColor(1.0f, 1.0f, 1.0f, 0.8f);
	sprite.DrawBox(x, y, w / 2, h / 2, &lineColor, false);
}
