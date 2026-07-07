#include "MagicBase.h"

#include "../../main.h"

void MagicBase::Update()
{
	// エフェクトの位置を毎フレーム適用し直す
	// (後半に遅れて生成されるノードにも位置が反映され、原点(0,0,0)に出るのを防ぐ)
	std::shared_ptr<KdEffekseerObject> spEffect = m_wpEffect.lock();
	if (spEffect)
	{
		spEffect->SetWorldMatrix(m_effectMatrix);
	}

	// 寿命を進め、時間が来たら停止して消滅する
	m_elapsed += Application::Instance().GetDeltaTime();
	if (m_elapsed >= m_lifeTime)
	{
		StopAndExpire();
	}
}

void MagicBase::PlayEffect(const std::string& _effectPath, const Math::Matrix& _worldMatrix)
{
	m_effectMatrix = _worldMatrix;

	m_wpEffect = KdEffekseerManager::GetInstance().Play(_effectPath, _worldMatrix.Translation());

	std::shared_ptr<KdEffekseerObject> spEffect = m_wpEffect.lock();
	if (spEffect)
	{
		spEffect->SetWorldMatrix(m_effectMatrix);
	}
}

void MagicBase::StopAndExpire()
{
	std::shared_ptr<KdEffekseerObject> spEffect = m_wpEffect.lock();
	if (spEffect)
	{
		spEffect->StopEffect();
	}

	m_isExpired = true;
}
