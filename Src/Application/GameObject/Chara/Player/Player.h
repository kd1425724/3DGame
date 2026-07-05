#pragma once

#include "../CharaBase.h"

class CameraBase;

//====================================================
//
// テスト用プレイヤーキャラクター
//  ・見た目はBlock.gltf、他と見分けが付くように水色で表示
//  ・WASDで移動(基準カメラが設定されていればその水平方向の向きを基準に移動する)
//  ・地面(Ground)に自動で立つ(CharaBase::GroundCheckのレイ判定を使用)
//
//====================================================
class Player : public CharaBase
{
public:

	Player()				{}
	~Player()	override	{}

	void Init()			override;
	void Update()		override;
	void PostUpdate()	override;

	// 移動方向の基準にするカメラを設定する(未設定ならワールド座標軸のまま移動する)
	void SetCameraReference(const std::shared_ptr<CameraBase>& camera) { m_wpCamera = camera; }

private:

	// 1秒あたりの移動速度
	float m_moveSpeed = 5.0f;

	// 移動方向の基準にするカメラ
	std::weak_ptr<CameraBase> m_wpCamera;
};
