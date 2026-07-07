#pragma once

#include "../MagicBase.h"

//====================================================
//
// レーザーの発射体(当たり判定つき)
//  ・MagicBaseを継承し、エフェクト保持・寿命・位置再適用は基底に任せる
//  ・固有の処理として、前方に攻撃判定(球)を張り、Enemyに当たったら消滅させる
//
// 使い方(Player等から)：
//   auto shot = std::make_shared<LaserShot>();
//   shot->Fire(firePos, front);                 // エフェクト再生 + 判定準備
//   SceneManager::Instance().AddObject(shot);   // シーンに追加(以降Update/PostUpdateが回る)
//
//====================================================
class LaserShot : public MagicBase
{
public:

	LaserShot()				{}
	~LaserShot() override	{}

	// 発射する(発射位置と正面方向を指定。エフェクト再生と判定の初期化を行う)
	void Fire(const Math::Vector3& _pos, const Math::Vector3& _dir);

	void PostUpdate()	override;
	void DrawDebug()	override;

private:

	// 正面方向(攻撃判定の中心を前方にずらすのに使う)
	Math::Vector3 m_dir = Math::Vector3::Backward;

	// 攻撃判定の球：発射位置から前方m_hitOffsetの位置に半径m_hitRadiusの球を置く
	float m_hitRadius = 1.5f;
	float m_hitOffset = 2.0f;
};
