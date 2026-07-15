#pragma once

//====================================================
//
// EffectManager ── 斬撃などの短命VFXを一元管理するシングルトン
//
//  ・どこからでも SpawnSlash(pos) で斬撃を出せる(Player/敵/他スキル共用)
//  ・板ポリ(カメラを向くビルボード)を共有し、発生中のVFXを寿命で自動消滅させる
//  ・更新/描画はシーン(BaseScene)から毎フレーム呼ぶ。CameraShakeと同じ運用
//
//  ※ シングルトンだがコンストラクタからInitは呼ばない(自己再入回避)。
//    Init()はApplication::Initから1回だけ呼ぶ
//
//====================================================
class KdSquarePolygon;

class EffectManager
{
public:

	static EffectManager& Instance()
	{
		static EffectManager instance;
		return instance;
	}

	// 板ポリ(斬撃テクスチャ)を生成する。Application::Initから1回だけ呼ぶ
	void Init();

	// 斬った位置に斬撃を1つ出す(面内回転は散らして毎回違う向きに見せる)
	void SpawnSlash(const Math::Vector3& pos);

	// 発生中のVFXの経過を進め、寿命が尽きたものを消す。シーンから毎フレーム呼ぶ
	void Update(float dt);

	// 陰影なしパスで発生中のVFXを描画する(拡大しながらフェード)。シーンのUnLitパスから呼ぶ
	void DrawUnLit();

	// 全VFXを消す(シーン切替やリスポーンで使う)
	void Clear();

private:

	EffectManager() = default;
	~EffectManager();
	EffectManager(const EffectManager&) = delete;
	void operator=(const EffectManager&) = delete;

	// 斬撃1つぶん(位置/面内回転/経過/寿命)
	struct SlashFX { Math::Vector3 pos; float rot; float age; float life; };
	std::vector<SlashFX> m_slashes;

	// 斬撃の見た目に使う共有の板ポリ(カメラを向く点ビルボード)
	std::unique_ptr<KdSquarePolygon> m_upSlashPoly;

	// 面内回転を散らすための種(発生ごとに増やす)
	unsigned int m_spawnCounter = 0;
};
