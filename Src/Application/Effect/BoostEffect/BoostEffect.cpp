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

	// 【注意】このシェーダは発光色で最終色を"上書き"する
	//   KdStandardShader_PS_Lit.hlsl:  outColor = 発光テクスチャ * g_Emissive * 頂点色
	// つまり見た目の色を決めているのは基本色(col.rgb)ではなく emissive のほう。
	// 最初 emissive を青寄り(0.25,0.5,0.7)にしていたため、col を白にしても
	// 青い玉のままだった。色は emissive 側で調整すること
	Math::Vector3 emissive = DebugParams::Instance().Vector3Param(
		U8("加速エフェクト/発光色"), Math::Vector3(0.65f, 0.85f, 1.0f));

	Math::Color col(1.0f, 1.0f, 1.0f, alpha);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*pPoly, world, col, emissive);
}
