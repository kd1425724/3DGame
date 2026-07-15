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
	// 落下攻撃：空中で攻撃入力→対象へワイヤーで引き寄せ突撃(未ロックは真下ダイブ)。
	// 到達/着地で連続攻撃(フルリー)へ移行する
	void UpdateDive(float dt);
	// 敵の場所で一旦止まり、周りの敵への連続攻撃(フルリー)を開始する
	void StartFlurry(const Math::Vector3& atPos);
	// 照準：カメラの向き(画面中心)に一番近い敵を毎フレーム自動ターゲットにする(カメラは回さない)。
	// PostUpdateから呼ぶ。選ばれた敵はマーカー表示され、落下攻撃の突撃先になる
	void UpdateTargeting();
	// 自動ターゲット中の敵にマーカー(カメラを向く板ポリ)を描く
	void DrawTargetMarker();

	// 開始位置へ復帰する(速度リセット・接地解除・ワイヤー解除)。落下時とRキーで呼ぶ
	void Respawn();

	// 【#1 自分で実装】ワイヤーの見た目を描く。DrawUnLitから呼ぶ土台のみ用意(中身はコメント)
	//  ・物理はWireAction(距離拘束)、見た目はここ、と役割を分ける
	//  ・スイング中はアンカーへ、突撃(グラップル)中は対象へ、手元から線を引く
	void DrawWire();
	// 手元(from)→終点(to)に沿ってワイヤーの板ポリを1本描く(スイング/突撃の共通処理)
	void DrawWireSegment(const Math::Vector3& from, const Math::Vector3& to);

	// ※ 移動速度はDebugParams("プレイヤー/移動速度")で調整する

	// 移動方向の基準にするカメラ
	std::weak_ptr<CameraBase> m_wpCamera;

	// リスポーンの復帰先(開始位置)
	Math::Vector3 m_spawnPos = {};

	// ジャンプの操作補助タイマー
	float m_coyoteTimer = 999.0f;     // 接地を離れてからの経過時間(小さいうちは空中でも跳べる)
	float m_jumpBufferTimer = 0.0f;   // ジャンプ入力を先読みして保持する残り時間

	// 落下攻撃(突撃)中フラグ(対象へ引き寄せ→到達でフルリーへ)
	bool m_isDiving = false;
	// 連続攻撃(フルリー)中フラグ：到達点で止まり、周りの敵を自動ロックオンして連続で斬る
	bool m_isFlurry = false;
	// フルリーの残り時間／次の一撃までの間隔タイマー／止まって攻撃する位置
	float m_flurryTimer = 0.0f;
	float m_flurryHitTimer = 0.0f;
	Math::Vector3 m_flurryPos = {};
	// チェイン数：一撃ごとに増える(手応えを段階的に強く/将来のコンボUI用)。接地・待機でリセット
	int m_diveChainCount = 0;
	// 突撃入力の先行入力バッファ(連打でグラップルが繋がりやすいように少しの間押下を覚える)
	float m_diveBufferTimer = 0.0f;

	// ロックオン中の対象(UpdateLockOnで設定。落下攻撃の突撃先に使う)
	std::weak_ptr<KdGameObject> m_wpLockOnTarget;
	// 落下攻撃で突撃中の対象(ロックオンからコピー。ホーミングの狙い先)
	std::weak_ptr<KdGameObject> m_wpDiveTarget;

	//ワイヤー
	std::unique_ptr<WireAction> m_upWire;

	// ワイヤーの見た目(板ポリを線に沿わせカメラへ向ける軸固定ビルボード)
	std::unique_ptr<KdSquarePolygon> m_upWirePoly;

	// 自動ターゲットのマーカー(カメラを向く板ポリ)
	std::unique_ptr<KdSquarePolygon> m_upMarkerPoly;
	// マーカーの回転/脈動アニメ用の経過時間
	float m_markerTime = 0.0f;

	// ※ 移動用の速度は基底CharaBaseの m_velocity(3D) を共通で使う
};
