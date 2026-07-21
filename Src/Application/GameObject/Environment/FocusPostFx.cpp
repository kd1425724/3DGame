#include "FocusPostFx.h"

#include "../../main.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Debug/DebugFlags/DebugFlags.h"

void FocusPostFx::Update()
{
	KdPostProcessShader& pp = KdShaderManager::Instance().m_postProcessShader;

	// この演出のON/OFF(切ると常に通常の見た目へ戻る)
	bool enable = DebugFlags::Instance().Get(U8("演出/空中スロー演出"), true);

	// スローが掛かっているか(timeScale<1)。掛かっていれば演出目標=1、なければ0
	bool  slowing = enable && (Application::Instance().GetTimeScale() < 0.999f);
	float target  = slowing ? 1.0f : 0.0f;

	// 実時間で目標へ滑らかに寄せる(スロー中でも演出の遷移は現実の速度で進む)
	float dt     = Application::Instance().GetRealDeltaTime();
	float follow = DebugParams::Instance().Float(U8("スロー演出/追従速度"), 8.0f, 0.5f, 30.0f);
	float t = follow * dt;
	if (t > 1.0f)
	{
		t = 1.0f;
	}
	m_blend += (target - m_blend) * t;

	// ===== 被写界深度(DoF) =====
	// near/far … ビュー深度(ワールド単位)を 0..1 に写す範囲
	// focus/range … その 0..1 空間での焦点位置と、そこからボケ始めるまでの幅
	// (奥の幅を大きくするとほぼ全面くっきり、小さくすると焦点の奥がボケる)
	float nearClip  = DebugParams::Instance().Float(U8("スロー演出/近クリップ"),   0.0f,   0.0f,  50.0f);
	float farClip   = DebugParams::Instance().Float(U8("スロー演出/遠クリップ"), 100.0f,  10.0f, 500.0f);
	float focus     = DebugParams::Instance().Float(U8("スロー演出/焦点(0-1)"),    0.08f,  0.0f,   1.0f);
	float foreRange = DebugParams::Instance().Float(U8("スロー演出/手前のボケ幅"), 0.3f,   0.02f,  2.0f);
	// 奥のボケ幅：通常は十分大きく(=くっきり)、スロー時に小さく(=背景ボケ)。m_blendで補間
	float backSharp = DebugParams::Instance().Float(U8("スロー演出/奥の幅(通常)"),  20.0f, 0.1f, 100.0f);
	float backSlow  = DebugParams::Instance().Float(U8("スロー演出/奥の幅(スロー)"), 0.22f, 0.02f, 5.0f);
	float backRange = backSharp + (backSlow - backSharp) * m_blend;

	pp.SetNearClippingDistance(nearClip);
	pp.SetFarClippingDistance(farClip);
	pp.SetFocusDistance(focus);
	pp.SetFocusRange(foreRange, backRange);

	// ===== Bloom強調(高輝度しきい値。低いほど広い範囲が光る) =====
	// エフェクトやemissive素材の発光を、スロー中だけ強める演出
	// ※ ネオンの夜のときは 0.6 / 0.35 まで下げて明るい面を広く滲ませていたが、
	//    2026/07/21に光と影を初期配布の値へ戻した際、この既定値に揃え直した
	//    (平行光2.25の白と低いしきい値が組むと画面が強く白飛びするため)
	float brightNormal = DebugParams::Instance().Float(U8("スロー演出/発光しきい値(通常)"),   1.2f, 0.1f, 3.0f);
	float brightSlow   = DebugParams::Instance().Float(U8("スロー演出/発光しきい値(スロー)"), 0.75f, 0.1f, 3.0f);
	float threshold = brightNormal + (brightSlow - brightNormal) * m_blend;
	pp.SetBrightThreshold(threshold);
}

void FocusPostFx::DrawSprite()
{
	// スロー中(timeScale<1)だけ、遅さに応じて画面を少し暗くする(バレットタイム感)。
	// ※ BaseScene::DrawSpriteのm_spriteShader.Begin〜End間で呼ばれる。この中で描画する
	if (!DebugFlags::Instance().Get(U8("演出/スロー暗転"), true)) { return; }

	float timeScale = Application::Instance().GetTimeScale();

	// スロー量(0=等速〜1に近いほど遅い)に暗さ係数を掛けて不透明度を決める
	float darkness = DebugParams::Instance().Float(U8("演出/スロー暗さ"), 0.4f, 0.0f, 1.0f);
	float alpha = (1.0f - timeScale) * darkness;
	if (alpha < 0.01f) { return; }                 // ほぼ等速なら暗幕は出さない
	if (alpha > 0.85f)
	{
		alpha = 0.85f;
	}   // 暗くしすぎない上限

	// 画面サイズいっぱいの黒い箱を中央(2D原点=画面中央)に描く
	auto spBackBuffer = KdDirect3D::Instance().GetBackBuffer();
	int w = spBackBuffer ? (int)spBackBuffer->GetWidth()  : 1280;
	int h = spBackBuffer ? (int)spBackBuffer->GetHeight() : 720;

	Math::Color col(0.0f, 0.0f, 0.0f, alpha);

	// 半透明で重ねるためAlphaブレンドを張る(スプライトのBegin/Endはブレンドを設定しないため)
	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Alpha);
	KdShaderManager::Instance().m_spriteShader.DrawBox(0, 0, w / 2 + 2, h / 2 + 2, &col, true);
	KdShaderManager::Instance().UndoBlendState();
}
