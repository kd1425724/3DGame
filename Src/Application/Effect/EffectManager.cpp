#include "EffectManager.h"

#include "EffectBase.h"
#include "SlashEffect/SlashEffect.h"
#include "BoostEffect/BoostEffect.h"
#include "../Debug/DebugParams/DebugParams.h"

// unique_ptr<EffectBase>(前方宣言)を持つvectorの破棄には完全な型が要るので
// デストラクタは.cpp側(EffectBaseが見える)で定義する
EffectManager::~EffectManager() = default;

void EffectManager::Add(const std::shared_ptr<EffectBase>& _effect)
{
	if (_effect)
	{
		m_effects.push_back(_effect);
	}
}

void EffectManager::SpawnSlash(const Math::Vector3& _pos)
{
	// 面内回転は発生数と位置から散らして、毎回違う向きに見せる
	int seed = (int)(m_spawnCounter++ * 61) + (int)(_pos.x * 17.0f) + (int)(_pos.z * 29.0f);
	float rot = DirectX::XMConvertToRadians((float)(((seed % 360) + 360) % 360));

	Add(std::make_shared<SlashEffect>(_pos, rot));
}

void EffectManager::SpawnBoost(const Math::Vector3& _pos, const Math::Vector3& _accelDir)
{
	// 粒は加速と逆向きに流す＝自分は前へ進むので、後方へ吹き出して見える
	float flow = DebugParams::Instance().Float(U8("加速エフェクト/流れる速さ"), 6.0f, 0.0f, 40.0f);
	float scatter = DebugParams::Instance().Float(U8("加速エフェクト/散らばり"), 0.25f, 0.0f, 2.0f);

	// 毎回同じ位置だと1本の線に見えるので、発生ごとに少しずらす
	int seed = (int)(m_spawnCounter++ * 71);
	float sx = (float)(((seed * 13) % 200) - 100) / 100.0f;
	float sy = (float)(((seed * 29) % 200) - 100) / 100.0f;
	float sz = (float)(((seed * 47) % 200) - 100) / 100.0f;
	Math::Vector3 jitter(sx * scatter, sy * scatter, sz * scatter);

	Add(std::make_shared<BoostEffect>(_pos + jitter, -_accelDir * flow));
}

void EffectManager::Update()
{
	// 各エフェクトを更新(dtは各自Applicationから取る)。終わったもの(IsExpired)は
	// swap&popで取り除く(順序は問わない)
	for (size_t i = 0; i < m_effects.size(); )
	{
		m_effects[i]->Update();
		if (m_effects[i]->IsExpired())
		{
			m_effects[i] = std::move(m_effects.back());
			m_effects.pop_back();
		}
		else
		{
			++i;
		}
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
