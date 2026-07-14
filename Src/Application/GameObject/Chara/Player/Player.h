#pragma once

#include "../CharaBase.h"

class CameraBase;
class WireAction;
class KdSquarePolygon;

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

	// ※ コンストラクタ/デストラクタは.cppで定義する
	//    (unique_ptr<WireAction>を前方宣言で持つため、実体化はWireAction.hをincludeした.cpp側で行う)
	Player();
	~Player()	override;

	void Init()			override;
	void Update()		override;
	void PostUpdate()	override;
	void DrawUnLit()	override;   // ワイヤーの見た目(陰影なしパス)。中身は DrawWire() に委譲
	void DrawDebug()	override;

	// 種別タグ：シーン内からPlayerを探すときの判定に使う(dynamic_pointer_castの代わり)
	ObjectTag GetObjectTag() override { return ObjectTag::Player; }

	// 移動方向の基準にするカメラを設定する(未設定ならワールド座標軸のまま移動する)
	void SetCameraReference(const std::shared_ptr<CameraBase>& camera) { m_wpCamera = camera; }

	// リスポーン(落下リセット/Rキー)の復帰先を設定する
	void SetSpawnPos(const Math::Vector3& _pos) { m_spawnPos = _pos; }

private:

	// --- Update()の中身を仕事ごとに分けたもの(挙動は分割前と同じ。Updateは流れだけ) ---
	// ワイヤーの発射/解除の入力を処理する
	void UpdateWireInput();
	// ワイヤー接続中のスイング物理(重力→移動→距離拘束→当たり解決→SetPos)
	void UpdateWireSwing(float dt);
	// 通常移動(接地=入力に即セット/空中=エアアクセル。カメラの水平向き基準)
	void UpdateMove(float dt);
	// SPACEでジャンプ。コヨーテタイム(接地を離れた直後の猶予)＋先行入力(着地寸前の入力先読み)つき
	void UpdateJump(float dt);
	// Eキーで正面にレーザーを発射する
	void UpdateLaser();
	// 落下攻撃：空中で攻撃入力→急降下(ロックオン中は対象へ突撃するホーミング/未ロックは真下)
	void UpdateDive(float dt);
	// 落下攻撃の着弾：周囲の敵を範囲撃破し、カメラを揺らす
	void DiveImpact();
	// ロックオンの切替(PostUpdateから呼ぶ。"LockOn"押下で最寄りの敵をロックオン/離すと解除)
	void UpdateLockOn();

	// 開始位置へ復帰する(速度リセット・接地解除・ワイヤー解除)。落下時とRキーで呼ぶ
	void Respawn();

	// 【#1 自分で実装】ワイヤーの見た目を描く。DrawUnLitから呼ぶ土台のみ用意(中身はコメント)
	//  ・物理はWireAction(距離拘束)、見た目はここ、と役割を分ける
	//  ・繋がっている間だけ、手元→アンカーに沿ってワイヤーを描く
	void DrawWire();

	// ※ 移動速度はDebugParams("プレイヤー/移動速度")で調整する

	// 移動方向の基準にするカメラ
	std::weak_ptr<CameraBase> m_wpCamera;

	// リスポーンの復帰先(開始位置)
	Math::Vector3 m_spawnPos = {};

	// ジャンプの操作補助タイマー
	float m_coyoteTimer = 999.0f;     // 接地を離れてからの経過時間(小さいうちは空中でも跳べる)
	float m_jumpBufferTimer = 0.0f;   // ジャンプ入力を先読みして保持する残り時間

	// 落下攻撃中フラグ(急降下→着弾でDiveImpact)
	bool m_isDiving = false;

	// ロックオン中の対象(UpdateLockOnで設定。落下攻撃の突撃先に使う)
	std::weak_ptr<KdGameObject> m_wpLockOnTarget;
	// 落下攻撃で突撃中の対象(ロックオンからコピー。ホーミングの狙い先)
	std::weak_ptr<KdGameObject> m_wpDiveTarget;

	//ワイヤー
	std::unique_ptr<WireAction> m_upWire;

	// ワイヤーの見た目(板ポリを線に沿わせカメラへ向ける軸固定ビルボード)
	std::unique_ptr<KdSquarePolygon> m_upWirePoly;

	// ※ 移動用の速度は基底CharaBaseの m_velocity(3D) を共通で使う
};
