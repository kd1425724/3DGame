#pragma once

//====================================================
//
// デフォルトでは用意されていない計算をまとめた関数群
//  ・一定の割合で目標値に近づける(Lerp)処理(Vector3版/float版)
//  ・Y軸回転で目標方向を向く処理(内積で角度差・外積で回転方向を判定)
//
//====================================================
namespace MathAPI
{
	// 現在値をtargetの方向へrate(0~1)の割合で近づける(Lerpによる定数減衰接近)
	Math::Vector3 ApproachByLerp(const Math::Vector3& current, const Math::Vector3& target, float rate);

	// float版
	float ApproachByLerp(float current, float target, float rate);

	// 現在向いている角度(Y軸、度：0~360を想定)を、目標方向(ワールド座標系)へ
	// 最大maxAngleSpeed度/回まで回転させた新しい角度を返す(敵がプレイヤー方向を向く等に使用)
	// ・内積でなす角(cos)を求め、それをそのまま回転量として使う
	// ・外積のY成分の符号で、右回り/左回りどちらに回すべきかを判定する
	float RotateToDirection(float nowAngleDeg, const Math::Vector3& toDir, float maxAngleSpeedDeg);
}
