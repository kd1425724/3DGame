#include "MathAPI.h"

namespace MathAPI
{
	Math::Vector3 ApproachByLerp(const Math::Vector3& _current, const Math::Vector3& _target, float _rate)
	{
		return Math::Vector3::Lerp(_current, _target, _rate);
	}

	float ApproachByLerp(float _current, float _target, float _rate)
	{
		return _current + (_target - _current) * _rate;
	}

	float RotateToDirection(float _nowAngleDeg, const Math::Vector3& _toDir, float _maxAngleSpeedDeg)
	{
		// 【2026/07/27 実装を差し替えた】
		// 旧実装は「①現在角から向きベクトルを作る ②内積をacosに通してなす角を出す
		// ③外積のY成分の符号で左右を決める」という手順だった。CharaBase::UpdateFacing が
		// 「atan2で目標角を出し、差を最短方向へ畳んで詰める」という別のやり方で同じことを
		// していたため、後者に寄せて1本にまとめた。旧実装は git 履歴に残っている。
		//
		// 旧実装には2つの弱点があった:
		//  ・acos に渡していたのが「3Dの内積」だったので、y成分を持つベクトルを渡すと
		//    水平の角度差より大きい値が出る(現在の呼び出し元=Enemyは水平ベクトルを渡すので影響なし)
		//  ・0.1度未満を切り捨てるデッドゾーンがあり、わずかな向きのズレが永久に残っていた

		// 水平成分だけを見る(Y軸回転なので上下の傾きは関係ない)
		const Math::Vector3 dir = GetSafeNormalXZ(_toDir);
		if (dir == Math::Vector3::Zero) { return _nowAngleDeg; }

		const float targetDeg = DirToYawDeg(dir);

		// 最短方向へ、1回の上限まで詰めてから 0〜360 に収める
		return ClampAngleDeg(MoveTowardsAngleDeg(_nowAngleDeg, targetDeg, _maxAngleSpeedDeg));
	}
}
