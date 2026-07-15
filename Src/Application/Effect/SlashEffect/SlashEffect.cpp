#include "SlashEffect.h"

#include "../../main.h"   // Application::GetDeltaTime
#include "../../Debug/DebugParams/DebugParams.h"

// 共有の板ポリ実体。KdSquarePolygon(前方宣言)の破棄には完全な型が要るので.cpp側に置く
std::unique_ptr<KdSquarePolygon> SlashEffect::s_upPoly;

KdSquarePolygon* SlashEffect::GetSharedPoly()
{
	// 初回だけ生成。斬撃テクスチャの板ポリ(カメラを向く点ビルボード)。
	// テクスチャはKdAssetsのキャッシュから取り、板ポリに渡す(存在しない_mtrf/_emi/_nmlを探しに行かない)
	if (!s_upPoly)
	{
		std::shared_ptr<KdTexture> spTex = KdAssets::Instance().m_textures.GetData("Asset/Textures/Effect/Slash.png");
		s_upPoly = std::make_unique<KdSquarePolygon>(spTex);
		s_upPoly->Set2DObject(false);
		s_upPoly->SetBillboardMode(KdPolygon::BillboardMode::eScreen);
	}
	return s_upPoly.get();
}

SlashEffect::SlashEffect(const Math::Vector3& _pos, float _rot)
	: m_pos(_pos), m_rot(_rot)
{
	m_life = DebugParams::Instance().Float(U8("斬撃/寿命"), 0.14f, 0.03f, 1.0f);
}

SlashEffect::~SlashEffect() = default;

void SlashEffect::Update()
{
	m_age += Application::Instance().GetDeltaTime();
	if (m_age >= m_life) { m_isExpired = true; }   // 寿命が尽きたらEffectManagerが外す
}

void SlashEffect::DrawUnLit()
{
	KdSquarePolygon* pPoly = GetSharedPoly();
	if (!pPoly) { return; }

	float baseSize = DebugParams::Instance().Float(U8("斬撃/サイズ"), 2.2f, 0.3f, 8.0f);

	float t = (m_life > 0.0f) ? (m_age / m_life) : 1.0f;   // 0→1
	float size  = baseSize * (0.7f + 0.6f * t);            // 少し拡大しながら
	float alpha = 1.0f - t;                                // フェードアウト
	pPoly->SetScale(Math::Vector2(size, size));

	// 面内回転(散らした向き)＋位置。カメラへの正対はeScreenビルボードに任せる
	Math::Matrix world = Math::Matrix::CreateRotationZ(m_rot);
	world.Translation(m_pos);

	Math::Color   col(0.75f, 0.9f, 1.0f, alpha);
	Math::Vector3 emissive(0.5f, 0.8f, 1.0f);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*pPoly, world, col, emissive);
}
