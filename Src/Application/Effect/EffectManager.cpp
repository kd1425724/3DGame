#include "EffectManager.h"

#include "EffectBase.h"
#include "SlashEffect.h"

// unique_ptr<EffectBase>(前方宣言)を持つvectorの破棄には完全な型が要るので
// デストラクタは.cpp側(EffectBaseが見える)で定義する
EffectManager::~EffectManager() = default;

void EffectManager::Add(std::unique_ptr<EffectBase> _effect)
{
	if (_effect) { m_effects.push_back(std::move(_effect)); }
}

void EffectManager::SpawnSlash(const Math::Vector3& _pos)
{
	// 面内回転は発生数と位置から散らして、毎回違う向きに見せる
	int seed = (int)(m_spawnCounter++ * 61) + (int)(_pos.x * 17.0f) + (int)(_pos.z * 29.0f);
	float rot = DirectX::XMConvertToRadians((float)(((seed % 360) + 360) % 360));

	Add(std::make_unique<SlashEffect>(_pos, rot));
}

void EffectManager::Update(float _dt)
{
	// 経過を進め、終わったものはswap&popで取り除く(順序は問わない)
	for (size_t i = 0; i < m_effects.size(); )
	{
		m_effects[i]->Update(_dt);
		if (m_effects[i]->IsFinished())
		{
			m_effects[i] = std::move(m_effects.back());
			m_effects.pop_back();
		}
		else { ++i; }
	}
}

void EffectManager::DrawUnLit()
{
	for (auto& up : m_effects)
	{
		up->DrawUnLit();
	}
}

void EffectManager::Clear()
{
	m_effects.clear();
}
