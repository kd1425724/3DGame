#include "BoostEffect.h"

#include "../../main.h"   // Application::GetDeltaTime
#include "../../Debug/DebugParams/DebugParams.h"

// 共有の板ポリ実体。KdSquarePolygon(前方宣言)の破棄には完全な型が要るので.cpp側に置く
std::unique_ptr<KdSquarePolygon> BoostEffect::s_upPoly;

KdSquarePolygon* BoostEffect::GetSharedPoly()
{
	// 初回だけ生成。粒用の丸いテクスチャ(中心が濃く外周が透明になる減衰)を使う。
	// ※ 最初はSystem/WhiteNoise.pngを流用したが、あれはノイズ画像でアルファも無く、
	//    四角いノイズの塊がそのまま出て見た目が破綻したため専用テクスチャを用意した
	if (!s_upPoly)
	{
		std::shared_ptr<KdTexture> spTex = KdAssets::Instance().m_textures.GetData("Asset/Textures/Effect/Particle.png");
		s_upPoly = std::make_unique<KdSquarePolygon>(spTex);
		s_upPoly->Set2DObject(false);
		s_upPoly->SetBillboardMode(KdPolygon::BillboardMode::eScreen);
	}
	return s_upPoly.get();
}

BoostEffect::BoostEffect(const Math::Vector3& _pos, const Math::Vector3& _vel)
	: m_pos(_pos), m_vel(_vel)
{
	m_life = DebugParams::Instance().Float(U8("加速エフェクト/寿命"), 0.35f, 0.05f, 2.0f);
}

BoostEffect::~BoostEffect() = default;

void BoostEffect::SetColorOverride(const Math::Vector3& _color)
{
	m_colorOverride = _color;
	m_hasColorOverride = true;
}

void BoostEffect::Update()
{
	float dt = Application::Instance().GetDeltaTime();

	// 置かれた場所から後方へ流れていく
	m_pos += m_vel * dt;

	m_age += dt;
	if (m_age >= m_life)
	{
		m_isExpired = true;
	}   // 寿命が尽きたらEffectManagerが外す
}

void BoostEffect::DrawUnLit()
{
	KdSquarePolygon* pPoly = GetSharedPoly();
	if (!pPoly) { return; }

	float baseSize = DebugParams::Instance().Float(U8("加速エフェクト/サイズ"), 0.18f, 0.02f, 3.0f);
	float density  = DebugParams::Instance().Float(U8("加速エフェクト/濃さ"),   0.6f,  0.05f, 1.0f);

	float t = (m_life > 0.0f) ? (m_age / m_life) : 1.0f;   // 0→1
	float size  = baseSize * (1.0f - 0.6f * t);            // だんだん縮む
	float alpha = (1.0f - t) * density;                    // フェードアウト(全体の濃さも掛ける)
	pPoly->SetScale(Math::Vector2(size, size));

	Math::Matrix world = Math::Matrix::Identity;
	world.Translation(m_pos);

	// 【2026/07/31 訂正】ここには以前「色を決めているのは基本色ではなく emissive のほう」
	//   と書いてあったが、UnLitパスでは【誤り】だった。実測して確かめた事実：
	//     ・KdStandardShader_PS_UnLit.hlsl は
	//         outColor = 基本色テクスチャ * 頂点色 * g_BaseColor
	//         if (g_OnlyEmissie) 上書き else 【加算】 発光テクスチャ * g_Emissive * 頂点色
	//     ・g_OnlyEmissie は定義だけで【どこからも設定されておらず常に0】＝必ず加算になる
	//     ・発光テクスチャは未設定なので白が入る(KdStandardShader.cpp の WriteMaterial)
	//     ・Particle.png は【全ピクセルが純白(1,1,1)】で、形はアルファだけで作られている
	//   つまり 白 + 発光色 は必ず1.0で飽和し、【発光色を何にしても白にしかならない】。
	//   ジェットの色(0.65,0.85,1.0)もこれまで一度も効いていなかった。
	//
	//   正しい染め方は基本色(colRate)のほう。ここは基本色テクスチャに掛け算されるので、
	//   純白のテクスチャがそのまま指定した色になる。発光は0にして飽和を避ける
	Math::Vector3 tint = m_hasColorOverride
		? m_colorOverride
		: DebugParams::Instance().Vector3Param(U8("加速エフェクト/色"), Math::Vector3(1.0f, 1.0f, 1.0f));

	Math::Color col(tint.x, tint.y, tint.z, alpha);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*pPoly, world, col, Math::Vector3::Zero);
}
