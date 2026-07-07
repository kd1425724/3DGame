#pragma once

//====================================================
//
// レーザーの発射体(当たり判定つき)
//  ・BlueLaserエフェクトを「見た目」として従える
//  ・自身はKdGameObjectとしてシーンに登録され、PostUpdateで攻撃判定を行う
//  ・寿命(m_lifeTime秒)が来たらエフェクトを停止して消滅する
//
// 使い方(Player等から)：
//   auto shot = std::make_shared<LaserShot>();
//   shot->Fire(firePos, front);                 // エフェクト再生 + 判定準備
//   SceneManager::Instance().AddObject(shot);   // シーンに追加(以降Update/PostUpdateが回る)
//
//====================================================
class LaserShot : public KdGameObject
{
public:

	LaserShot()				{}
	~LaserShot() override	{}

	// 発射する(発射位置と正面方向を指定。エフェクト再生と判定の初期化を行う)
	void Fire(const Math::Vector3& _pos, const Math::Vector3& _dir);

	void Update()		override;
	void PostUpdate()	override;
	void DrawDebug()	override;

private:

	// エフェクトを停止して自身を消滅状態にする
	void StopAndExpire();

	// 従えているエフェクト(見た目)への弱参照
	std::weak_ptr<KdEffekseerObject> m_wpEffect;

	// 発射時に組んだエフェクト用ワールド行列(毎フレーム適用し直す)
	Math::Matrix m_effectMatrix = Math::Matrix::Identity;

	// 正面方向(攻撃判定の中心を前方にずらすのに使う)
	Math::Vector3 m_dir = Math::Vector3::Backward;

	// 生存時間(秒)と経過時間
	float m_lifeTime = 5.0f;
	float m_elapsed = 0.0f;

	// 攻撃判定の球：発射位置から前方m_hitOffsetの位置に半径m_hitRadiusの球を置く
	float m_hitRadius = 1.5f;
	float m_hitOffset = 2.0f;
};
