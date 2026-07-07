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

	// 発射したレーザーエフェクトを一定時間で強制停止する処理
	// (エフェクト素材側に長寿命/無限生成ノードが残っていても、ゲーム側で必ず打ち切る)
	void UpdateFiredLasers();

	// 1秒あたりの移動速度
	float m_moveSpeed = 5.0f;

	// 移動方向の基準にするカメラ
	std::weak_ptr<CameraBase> m_wpCamera;

	// 発射中のレーザー1発分(エフェクト実体への弱参照 + 発射時のワールド行列 + 経過時間)
	// ※ 行列を保持して毎フレーム適用し直すのは、後半に遅れて生成されるノードが
	//    位置未適用のまま原点(0,0,0)に出るのを防ぐため
	struct FiredLaser
	{
		std::weak_ptr<KdEffekseerObject> effect;
		Math::Matrix worldMatrix = Math::Matrix::Identity;
		float elapsed = 0.0f;
	};

	// 発射中のレーザー一覧(経過時間がm_laserStopTimeを超えたら停止して除外する)
	std::vector<FiredLaser> m_firedLasers;

	// レーザーを発射してから強制停止するまでの時間(秒)
	float m_laserStopTime = 2.0f;
};
