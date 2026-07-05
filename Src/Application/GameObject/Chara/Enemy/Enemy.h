#pragma once

#include "../CharaBase.h"

//====================================================
//
// テスト用敵キャラクター
//  ・見た目はBlock.gltf、他と見分けが付くように赤色で表示
//  ・追従対象(Player)にゆっくり近づく
//  ・追従対象に接触すると消滅する
//
//====================================================
class Enemy : public CharaBase
{
public:

	Enemy()				{}
	~Enemy()	override	{}

	void Init()			override;
	void Update()		override;
	void PostUpdate()	override;

	// 追従・接触判定の対象を設定する
	void SetTarget(const std::shared_ptr<KdGameObject>& target) { m_wpTarget = target; }

private:

	// 1秒あたりの移動速度(プレイヤーよりゆっくり)
	float m_moveSpeed = 1.5f;

	// 1秒あたりの最大回転速度(度)
	float m_turnSpeedDeg = 180.0f;

	// この距離より対象に近づいたら接触したとみなす
	float m_hitRadius = 0.6f;

	// 追従・接触判定の対象
	std::weak_ptr<KdGameObject> m_wpTarget;
};
