#pragma once

#include "../CharaBase.h"

//====================================================
//
// テスト用プレイヤーキャラクター
//  ・見た目はBlock.gltf、他と見分けが付くように水色で表示
//  ・WASDで水平移動
//  ・地面(Ground)に自動で立つ(CharaBase::GroundCheckのレイ判定を使用)
//
//====================================================
class Player : public CharaBase
{
public:

	Player()				{}
	~Player()	override	{}

	void Init()		override;
	void Update()	override;

private:

	// 1秒あたりの移動速度
	float m_moveSpeed = 5.0f;
};
