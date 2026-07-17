#include "MathAPI.h"

namespace MathAPI
{
	Math::Vector3 ApproachByLerp(const Math::Vector3& current, const Math::Vector3& target, float rate)
	{
		return Math::Vector3::Lerp(current, target, rate);
	}

	float ApproachByLerp(float current, float target, float rate)
	{
		return current + (target - current) * rate;
	}

	float RotateToDirection(float nowAngleDeg, const Math::Vector3& toDir, float maxAngleSpeedDeg)
	{
		// 現在向いている方向ベクトルを、現在の角度から求める
		Math::Matrix nowRotMat = Math::Matrix::CreateRotationY(DirectX::XMConvertToRadians(nowAngleDeg));
		Math::Vector3 nowDir = Math::Vector3::TransformNormal(Math::Vector3(0, 0, 1), nowRotMat);

		// 内積：ベクトルA * ベクトルB * COS(なす角) = COS(なす角)(お互い単位ベクトルのため)
		float dot = nowDir.Dot(toDir);

		// 誤差でacosの定義域(-1~1)から外れてNaNにならないようにクランプ
		dot = std::clamp(dot, -1.0f, 1.0f);

		// COSをACOSで計算すると角度(ラジアン)になる
		float angle = DirectX::XMConvertToDegrees(acos(dot));

		float newAngleDeg = nowAngleDeg;

		// 求めた角度が少しでもあるなら回転させる
		if (angle >= 0.1f)
		{
			// 1回の回転量に上限を設ける
			if (angle > maxAngleSpeedDeg)
			{
				angle = maxAngleSpeedDeg;
			}

			// 外積：2つのベクトルに対し垂直なベクトルを算出。Y成分の符号で回転方向を判定する
			Math::Vector3 cross = nowDir.Cross(toDir);

			if (cross.y > 0)
			{
				// 外積が上を向いている
				newAngleDeg += angle;
				if (newAngleDeg > 360.0f)
				{
					newAngleDeg -= 360.0f;
				}
			}
			else
			{
				// 外積が下を向いている
				newAngleDeg -= angle;
				if (newAngleDeg < 0.0f)
				{
					newAngleDeg += 360.0f;
				}
			}
		}

		return newAngleDeg;
	}
}
