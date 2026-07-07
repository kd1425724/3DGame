#pragma once

//====================================================
//
// 魔法(エフェクトを伴う攻撃など)の基底クラス
//  ・Effekseerエフェクトを「見た目」として従える共通処理をまとめる
//  ・エフェクトの位置を毎フレーム適用し直す(遅延生成ノードが原点に出るのを防ぐ)
//  ・寿命(m_lifeTime秒)が来たらエフェクトを停止して消滅する
//  ・当たり判定など個別の挙動は派生クラス(LaserShot等)がPostUpdate等で実装する
//
//====================================================
class MagicBase : public KdGameObject
{
public:

	MagicBase()				{}
	~MagicBase() override	{}

	// エフェクト位置の再適用と寿命管理(派生でoverrideする場合はMagicBase::Update()を呼ぶこと)
	void Update() override;

protected:

	// エフェクトを再生して従える(ワールド行列を指定。以降その行列を毎フレーム適用する)
	void PlayEffect(const std::string& _effectPath, const Math::Matrix& _worldMatrix);

	// エフェクトを停止して自身を消滅状態にする
	// ※ 追加でエフェクト(魔法陣など)を持つ派生はoverrideして自前分も止め、MagicBase::StopAndExpire()を呼ぶ
	virtual void StopAndExpire();

	// 従えているエフェクト(見た目)への弱参照
	std::weak_ptr<KdEffekseerObject> m_wpEffect;

	// エフェクト用ワールド行列(毎フレーム適用し直す)
	Math::Matrix m_effectMatrix = Math::Matrix::Identity;

	// 生存時間(秒)と経過時間
	float m_lifeTime = 5.0f;
	float m_elapsed = 0.0f;
};
