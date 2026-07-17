#include "GameHud.h"

#include "../../../Scene/SceneManager.h"
#include "../../../GameObject/Chara/CharaBase.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugFlags/DebugFlags.h"

void GameHud::DrawHud()
{
	// ※ スプライトのBegin/EndはBaseScene::DrawSpriteが囲むので、ここでは描くだけ
	DrawSpeedMeter();
	DrawReticle();
}

void GameHud::DrawReticle()
{
	// 表示ON/OFF(DebugFlags「HUD/レティクル表示」)
	if (!DebugFlags::Instance().Get(U8("HUD/レティクル表示"), true)) { return; }

	// 種類を切り替えて見比べられるようにする(0=環A / 1=菱形B / 2=コンパスC)
	int type = DebugParams::Instance().Int(U8("HUD/レティクル種類(0環1菱2羅)"), 0, 0, 2);
	if (type < 0) { type = 0; }
	if (type > 2) { type = 2; }

	static const char* kPaths[3] =
	{
		"Asset/Textures/UI/ReticleRing.png",		// A 照準環
		"Asset/Textures/UI/ReticleDiamond.png",	// B 菱形
		"Asset/Textures/UI/ReticleCompass.png",	// C コンパス
	};

	// テクスチャはKdAssetsキャッシュから取得(Targetingのマーカーと同じ方式)
	std::shared_ptr<KdTexture> spTex = KdAssets::Instance().m_textures.GetData(kPaths[type]);
	if (!spTex) { return; }

	// 表示サイズ(px)と不透明度をDebugParamsで調整。座標は画面中央が原点(0,0)、ピボット中心で中央に出る
	int   size  = static_cast<int>(DebugParams::Instance().Float(U8("HUD/レティクルサイズ"), 56.0f, 8.0f, 256.0f));
	float alpha = DebugParams::Instance().Float(U8("HUD/レティクル不透明度"), 0.9f, 0.0f, 1.0f);

	Math::Color col(1.0f, 1.0f, 1.0f, alpha);
	KdShaderManager::Instance().m_spriteShader.DrawTex(spTex.get(), 0, 0, size, size, nullptr, &col);
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
