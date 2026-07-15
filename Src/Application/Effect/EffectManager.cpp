#include "EffectManager.h"

#include "../Debug/DebugParams/DebugParams.h"

// unique_ptr<KdSquarePolygon>(前方宣言)の破棄にはKdSquarePolygonの完全な定義が要るので
// デストラクタは.cpp側(Pch経由で板ポリが見える)で定義する
EffectManager::~EffectManager() = default;

void EffectManager::Init()
{
	// 斬撃テクスチャの板ポリ(カメラを向く点ビルボード)
	m_upSlashPoly = std::make_unique<KdSquarePolygon>("Asset/Textures/Effect/Slash.png");
	m_upSlashPoly->Set2DObject(false);
	m_upSlashPoly->SetBillboardMode(KdPolygon::BillboardMode::eScreen);
}

void EffectManager::SpawnSlash(const Math::Vector3& pos)
{
	SlashFX fx;
	fx.pos = pos;
	// 面内回転は発生数と位置から散らして、毎回違う向きに見せる
	int seed = (int)(m_spawnCounter++ * 61) + (int)(pos.x * 17.0f) + (int)(pos.z * 29.0f);
	fx.rot  = DirectX::XMConvertToRadians((float)(((seed % 360) + 360) % 360));
	fx.age  = 0.0f;
	fx.life = DebugParams::Instance().Float(U8("斬撃/寿命"), 0.14f, 0.03f, 1.0f);
	m_slashes.push_back(fx);
}

void EffectManager::Update(float dt)
{
	// 経過を進め、寿命が尽きたものはswap&popで取り除く(順序は問わない)
	for (size_t i = 0; i < m_slashes.size(); )
	{
		m_slashes[i].age += dt;
		if (m_slashes[i].age >= m_slashes[i].life)
		{
			m_slashes[i] = m_slashes.back();
			m_slashes.pop_back();
		}
		else { ++i; }
	}
}

void EffectManager::DrawUnLit()
{
	if (!m_upSlashPoly) { return; }

	float baseSize = DebugParams::Instance().Float(U8("斬撃/サイズ"), 2.2f, 0.3f, 8.0f);

	for (auto& s : m_slashes)
	{
		float t = (s.life > 0.0f) ? (s.age / s.life) : 1.0f;   // 0→1
		float size  = baseSize * (0.7f + 0.6f * t);            // 少し拡大しながら
		float alpha = 1.0f - t;                                // フェードアウト
		m_upSlashPoly->SetScale(Math::Vector2(size, size));

		// 面内回転(散らした向き)＋位置。カメラへの正対はeScreenビルボードに任せる
		Math::Matrix world = Math::Matrix::CreateRotationZ(s.rot);
		world.Translation(s.pos);

		Math::Color   col(0.75f, 0.9f, 1.0f, alpha);
		Math::Vector3 emissive(0.5f, 0.8f, 1.0f);
		KdShaderManager::Instance().m_StandardShader.DrawPolygon(*m_upSlashPoly, world, col, emissive);
	}
}

void EffectManager::Clear()
{
	m_slashes.clear();
}
