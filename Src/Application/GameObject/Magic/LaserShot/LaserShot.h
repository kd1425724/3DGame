#pragma once

#include "../MagicBase.h"

//====================================================
//
// レーザーの発射体(魔法陣チャージ→レーザー発射の2段階、当たり判定つき)
//  ・MagicBaseを継承し、エフェクト保持・寿命・位置再適用は基底に任せる
//  ・Fire()するとまず魔法陣(MagicCircle)を表示(チャージ)、m_chargeTime秒後に
//    その位置からレーザーを発射する。攻撃判定はレーザー発射中のみ有効
//
// 使い方(Player等から)：
//   auto shot = std::make_shared<LaserShot>();
//   shot->Fire(firePos, front);                 // 魔法陣を出してチャージ開始
//   SceneManager::Instance().AddObject(shot);   // シーンに追加(以降Update/PostUpdateが回る)
//
//====================================================
class LaserShot : public MagicBase
{
public:

	LaserShot()				{}
	~LaserShot() override	{}

	// 発射開始(発射位置と正面方向を指定。まず魔法陣を出してチャージする)
	void Fire(const Math::Vector3& _pos, const Math::Vector3& _dir);

	void Update()		override;
	void PostUpdate()	override;
	void DrawDebug()	override;

protected:

	// レーザー・魔法陣の両方を停止して消滅する(MagicBase::StopAndExpireを拡張)
	void StopAndExpire() override;

private:

	// 進行フェーズ：Charge=魔法陣表示中 / Fire=レーザー発射中
	enum class Phase { Charge, Fire };
	Phase m_phase = Phase::Charge;

	// 魔法陣(チャージ中の見た目)への弱参照
	std::weak_ptr<KdEffekseerObject> m_wpMagicCircle;

	// 魔法陣を表示してからレーザーを発射するまでの時間(秒)と経過
	float m_chargeTime = 1.0f;
	float m_chargeElapsed = 0.0f;

	// 正面方向(攻撃判定の中心を前方にずらすのに使う)
	Math::Vector3 m_dir = Math::Vector3::Backward;

	// 攻撃判定の球：発射位置から前方m_hitOffsetの位置に半径m_hitRadiusの球を置く
	float m_hitRadius = 1.5f;
	float m_hitOffset = 2.0f;
};
