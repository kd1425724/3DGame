#include "CameraShake.h"

#include "../../Debug/DebugParams/DebugParams.h"

void CameraShake::AddTrauma(float _amount)
{
	m_trauma += _amount;
	if (m_trauma > 1.0f)
	{
		m_trauma = 1.0f;
	}
	if (m_trauma < 0.0f)
	{
		m_trauma = 0.0f;
	}
}

void CameraShake::Update(float _dt)
{
	m_time += _dt;

	float decay = DebugParams::Instance().Float(U8("カメラ/シェイク減衰"), 3.0f, 0.1f, 20.0f);
	m_trauma -= decay * _dt;
	if (m_trauma < 0.0f)
	{
		m_trauma = 0.0f;
	}
}

Math::Vector3 CameraShake::GetOffset() const
{
	if (m_trauma <= 0.0f) { return Math::Vector3::Zero; }

	float maxPos = DebugParams::Instance().Float(U8("カメラ/シェイク強さ"), 0.3f, 0.0f, 2.0f);

	// 体感に合わせてtraumaは2乗する(小さい揺れは控えめ、強い衝撃ははっきり)
	float shake = m_trauma * m_trauma;

	// 高めの周波数で細かく揺らす(軸ごとに周波数と位相をずらして単調な往復に見せない)
	float ox = maxPos * shake * sinf(m_time * 47.0f);
	float oy = maxPos * shake * sinf(m_time * 59.0f + 1.7f);
	return Math::Vector3(ox, oy, 0.0f);
}
