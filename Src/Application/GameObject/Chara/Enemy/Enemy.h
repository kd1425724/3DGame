#pragma once

#include "../CharaBase.h"

//====================================================
//
// テスト用敵キャラクター
//  ・見た目はBlock.gltf、他と見分けが付くように赤色で表示
//  ・追従対象(Player)に近づき、接近したら「予備動作(予告)→突進」で攻撃する
//  ・突進を無敵(回避)で受けられると反撃で消滅、無防備だとプレイヤーをノックバック
//  ・プレイヤーの攻撃(OnHit)を受けても消滅する
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

	// ※ 移動速度・旋回速度・攻撃系の数値はDebugParams("敵/…")で調整する

	// 攻撃を受ける当たり判定(TypeDamage球)の半径。突進の命中判定にも使う(コライダー登録に使うためメンバ)
	float m_hitRadius = 0.6f;

	// 追従・攻撃の対象
	std::weak_ptr<KdGameObject> m_wpTarget;

	// --- 攻撃の状態機械：追従 → 予備動作(予告) → 突進 → 硬直 → 追従 ---
	enum class State { Chase, Windup, Strike, Recover };
	State m_state = State::Chase;
	float m_stateTimer = 0.0f;          // 現在状態の残り時間(Windup/Strike/Recoverで使用)
	Math::Vector3 m_lungeDir = {};      // 突進方向。Windup終了時に固定する(以後は追尾しない=回避で避けられる)

	// 突進が命中した瞬間の処理(無敵なら反撃成立で自滅／無防備ならノックバック)
	void ResolveStrikeHit(const std::shared_ptr<KdGameObject>& _target);
	// 硬直状態へ移行する(突進の後隙)
	void EnterRecover();
};
