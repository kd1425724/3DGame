#pragma once

//====================================================
//
// EffectManager ── EffectBase派生のVFXを一元管理するシングルトン
//
//  ・Add()で任意のEffectBase(KdGameObject派生)を預かり、毎フレームUpdate→IsExpiredで除去する
//  ・SpawnSlash()等の便利関数で具体エフェクト(SlashEffect)を生成して追加する
//  ・更新/描画はシーン(BaseScene)から毎フレーム呼ぶ。CameraShakeと同じ運用
//
//====================================================
// vector<unique_ptr<EffectBase>>を保持するため、基底の完全な定義がここで必要
#include "EffectBase.h"

class EffectManager
{
public:

	static EffectManager& Instance()
	{
		static EffectManager instance;
		return instance;
	}

	// 任意のエフェクト(EffectBase派生)を預けて管理させる
	void Add(const std::shared_ptr<EffectBase>& _effect);

	// 便利関数：斬った位置に斬撃(SlashEffect)を1つ出す(面内回転は散らす)
	void SpawnSlash(const Math::Vector3& _pos);

	// 便利関数：加速中の噴射(BoostEffect)を1粒出す。
	//  _pos … 発生位置(プレイヤーの少し後ろ) / _accelDir … 加速している向き
	// 粒は加速と逆向きに流れるので、後方へ吹き出しているように見える
	void SpawnBoost(const Math::Vector3& _pos, const Math::Vector3& _accelDir);

	// 管理中のエフェクトの経過を進め、終わったもの(IsExpired)を外す。シーンから毎フレーム呼ぶ
	// (各エフェクトはKdGameObject同様、dtをApplicationから自分で取る)
	void Update();

	// 陰影なしパスで管理中のエフェクトを描画する。シーンのUnLitパスから呼ぶ
	void DrawUnLit();

	// 全エフェクトを消す(シーン切替やリスポーンで使う)
	void Clear();

private:

	EffectManager() = default;
	~EffectManager();
	EffectManager(const EffectManager&) = delete;
	void operator=(const EffectManager&) = delete;

	// 管理中のエフェクト(KdGameObject派生なのでshared_ptrで保持)
	std::vector<std::shared_ptr<EffectBase>> m_effects;

	// SpawnSlashの面内回転を散らすための種(発生ごとに増やす)
	unsigned int m_spawnCounter = 0;
};
