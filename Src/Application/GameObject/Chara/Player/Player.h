#pragma once

#include "../CharaBase.h"

class CameraBase;
class WireAction;
class Targeting;

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
	// ワイヤーの発射/解除の入力を処理する(スイング物理はWireAction::UpdateSwingに委譲)
	void UpdateWireInput();
	// 通常移動(接地=入力に即セット/空中=エアアクセル。カメラの水平向き基準)
	void UpdateMove(float dt);
	// SPACEでジャンプ。コヨーテタイム(接地を離れた直後の猶予)＋先行入力(着地寸前の入力先読み)つき
	void UpdateJump(float dt);
	// 回避ダッシュ：Shiftで移動入力方向(なければカメラ前方)へ短距離クイックムーブ＋無敵＋クールダウン
	void UpdateDodge(float dt);
	// 加速/空中ステップ：右クリック長押しで加速し続ける／単押しで空中ステップ。
	// 方向は移動入力(カメラ基準)、無入力なら進行方向。Jump併用で上向き成分が混ざる
	void UpdateAccel(float dt);
	// 加速/空中ステップの方向を求める(移動入力→無入力なら進行方向→Jumpで上向きを加算)
	Math::Vector3 GetAccelDir() const;
	// 左クリックが「攻撃」か「ワイヤー」かを判定する。
	// 連続攻撃の受付中、またはE(Focus)を押していてターゲットがいる時だけ攻撃
	bool IsAttackInput() const;
	// 突撃(落下攻撃)を開始する。左クリックを押した瞬間にUpdateWireInputから呼ばれる
	void StartDive();

	// 反撃(ジャスト回避カウンター)：敵の突進を回避の無敵で受けるとスロー猶予窓を開き、
	// その窓の間に攻撃(左クリック)を押すと今の突撃(ダイブ)へ移行する。回避の早期returnより前で呼ぶ
	void UpdateCounter();
	// 空中スロー(エアフォーカス)：空中でE(Focus)長押し中は時間をスローにして狙う。
	// フォーカスゲージで制限する
	void UpdateAirFocus();
	// 落下攻撃：対象へワイヤーで引き寄せ突撃(未ロックは真下ダイブ)。
	// 斬ったら受付窓中にもう一度押して周りの敵へ続けて突撃する連続攻撃
	void UpdateDive(float dt);
	// 範囲内で最も近い生きている敵を返す(連続攻撃の次の突撃先選び)。いなければnull
	std::shared_ptr<KdGameObject> FindNearestEnemy(const Math::Vector3& center, float range) const;

	// 開始位置へ復帰する(速度リセット・接地解除・ワイヤー解除)。落下時とRキーで呼ぶ
	void Respawn();

	// ワイヤーの見た目を描く。端点(手元→アンカー/対象)を決めてWireAction::Drawに委譲する。
	// スイング中はアンカーへ、突撃(グラップル)中は対象へ、手元から線を引く
	void DrawWire();

	// DebugWatchにプレイヤーの状態(速度/接地/各クールタイム/ゲージ/窓など)を毎フレーム出す。
	// DebugWatchウィンドウ(DebugManagerのメニューで表示ON)に一覧表示される
	void WatchDebug() const;

	// 当たり判定表示(DebugFlags「当たり判定/AABB表示」)がONのとき、索敵範囲・攻撃範囲・
	// 現在のターゲット線などを m_pDebugWire に積んで可視化する(DrawDebugから呼ぶ)
	void DrawDebugRanges();

	// ※ 移動速度はDebugParams("プレイヤー/移動速度")で調整する

	// 移動方向の基準にするカメラ
	std::weak_ptr<CameraBase> m_wpCamera;

	// リスポーンの復帰先(開始位置)
	Math::Vector3 m_spawnPos = {};

	// 回避ダッシュの状態(実行中フラグ/残り時間/クールダウン残り/ダッシュ方向/無敵残り)
	bool m_isDodging = false;
	float m_dodgeTimer = 0.0f;
	float m_dodgeCooldownTimer = 0.0f;
	Math::Vector3 m_dodgeDir = {};
	float m_invincibleTimer = 0.0f;   // ※ ダメージ実装後に使う想定(今は参照側が無いので無敵は実質未使用)

	// 左クリックを押した時、それが「ワイヤーとして」だったか。
	// ワイヤーで押した時だけ、離したらワイヤーを外す(攻撃で押した時は離しても無視する)
	bool m_anchorPressWasWire = false;

	// 右クリックを押している時間。単押し(空中ステップ)と長押し(加速)の区別に使う
	float m_accelHoldTime = 0.0f;

	// 反撃(ジャスト回避カウンター)の状態
	bool m_counterPending = false;       // 敵の突進を無敵で受けた=次のUpdateCounterでスロー窓を開く
	float m_counterWindowTimer = 0.0f;   // スロー猶予窓の残り(実時間で減らす)。この間に攻撃で突撃へ移行できる
	// 被弾ノックバックの硬直(この間は移動入力を無視して勢いを崩される。HPは無い)
	float m_staggerTimer = 0.0f;

public:
	// 無敵中か(回避のiフレーム)。ダメージ処理を入れたらここを見て被弾を無視する
	bool IsInvincible() const { return m_invincibleTimer > 0.0f; }

	// 敵の突進を無敵(回避)で受けた時に敵から呼ばれ、反撃(スロー猶予窓)の開始を予約する
	void NotifyCounter();
	// 敵の突進を無防備で受けた時に敵から呼ばれ、外向きノックバック＋短い硬直を与える(HPは無い)
	void ApplyKnockback(const Math::Vector3& _dir, float _power);
private:

	// ジャンプの操作補助タイマー
	float m_coyoteTimer = 999.0f;     // 接地を離れてからの経過時間(小さいうちは空中でも跳べる)
	float m_jumpBufferTimer = 0.0f;   // ジャンプ入力を先読みして保持する残り時間

	// 落下攻撃(突撃)中フラグ(対象へ引き寄せ→斬ったら周りの敵へ続けて突撃)
	bool m_isDiving = false;
	// チェイン数：一撃ごとに増える(手応えを段階的に強く/将来のコンボUI用)。接地・待機でリセット
	int m_diveChainCount = 0;
	// 斬った後、次の突撃入力を受け付ける残り時間(この間に長押し→離しで次の敵へ続けて突撃)
	float m_comboWindowTimer = 0.0f;
	// 空中スローのフォーカスゲージ(秒)。スロー中は実時間で減り、地上/未使用で回復
	float m_focusGauge = 1.5f;

	// 落下攻撃で突撃中の対象(Targetingの選択からコピー。ホーミングの狙い先)
	std::weak_ptr<KdGameObject> m_wpDiveTarget;

	//ワイヤー(物理＋見た目を内包)
	std::unique_ptr<WireAction> m_upWire;

	// 照準(画面中心の敵を自動ロックオン＋マーカー描画を内包する部品)
	std::unique_ptr<Targeting> m_upTargeting;

	// ※ 移動用の速度は基底CharaBaseの m_velocity(3D) を共通で使う
};
