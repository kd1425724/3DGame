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

	// 種別タグ：シーン内からEnemyを探すときの判定に使う(dynamic_pointer_castの代わり)
	ObjectTag GetObjectTag() override { return ObjectTag::Enemy; }

	// 攻撃(レーザー等)に当たったときに呼ばれる：消滅する
	void OnHit(KdGameObject* _other) override;

	// 追従・接触判定の対象を設定する
	void SetTarget(const std::shared_ptr<KdGameObject>& target) { m_wpTarget = target; }

private:

	// ※ 移動速度・旋回速度はDebugParams("敵/移動速度"・"敵/旋回速度")で調整する

	// この距離より対象に近づいたら接触したとみなす(コライダー登録にも使うためメンバで保持)
	float m_hitRadius = 0.6f;

	// 追従・接触判定の対象
	std::weak_ptr<KdGameObject> m_wpTarget;
};
