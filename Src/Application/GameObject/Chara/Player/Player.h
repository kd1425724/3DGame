#pragma once

#include "../CharaBase.h"

class CameraBase;
class WireAction;
class WallAction;
class Targeting;
class Enemy;

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
	void DrawSprite()	override;   // ロックオンのマーカー(2Dパス)。関節は体の内側なので3Dだと埋まる
	void DrawDebug()	override;

	// 状態に応じた再生アニメ名を返す(CharaBase::UpdateAnimationが毎フレーム呼ぶ)
	std::string SelectAnimation() const override;

	// 走りだけ、実際の速さに合わせて再生速度を変える
	float SelectAnimationSpeed() const override;

	// 向き直る速さ(度/秒)。実機で詰めたいのでDebugParamsから読む
	float SelectTurnSpeed() const override;

	// アニメごとの再生倍率をDebugParamsから読む(1.0=等速)。キーだけ差し替えて使う
	float GetAnimSpeedScale(const char* _key) const;

	// 種別タグ：シーン内からPlayerを探すときの判定に使う(dynamic_pointer_castの代わり)
	ObjectTag GetObjectTag() override { return ObjectTag::Player; }

	// 移動方向の基準にするカメラを設定する(未設定ならワールド座標軸のまま移動する)
	void SetCameraReference(const std::shared_ptr<CameraBase>& camera) { m_wpCamera = camera; }

	// リスポーン(落下リセット/Rキー)の復帰先を設定する
	void SetSpawnPos(const Math::Vector3& _pos) { m_spawnPos = _pos; }

private:

	// --- Update()の中身を仕事ごとに分けたもの(挙動は分割前と同じ。Updateは流れだけ) ---
	// ワイヤーの発射/解除の入力を処理する(スイング物理はWireAction::UpdateSwingAllに委譲)
	void UpdateWireInput();
	// 攻撃(右クリック)の入力を処理する。3段階：
	//   1回目=的へアンカー射出 / 2回目=突撃 / 3回目=突撃中に斬る
	void UpdateAttackInput();
	// 的へアンカーを射出する(1回目)。地形ワイヤーは畳んでから撃つ
	void ShootJointWire();
	// 敵の関節へワイヤーが出ているか(飛行中も含む)。段階の判定に使う
	bool IsAnyJointWireActive() const;
	// 突撃中に斬る(3回目)。間合いの内なら即斬り、まだ遠ければ先行入力として覚える
	void PerformDiveSlash();
	// 実際に斬る処理。_isCritical はダメージ倍率と手応え(カメラ揺れ)を変える
	void ExecuteSlash(const Math::Vector3& _aim, bool _isCritical);
	// クリティカルになる間合い(m)。判定とデバッグ表示の両方が読む。
	// ※ 「斬撃範囲」は 2026/08/02 に廃止した。空振りを先行入力に変えた時点で、
	//   その内で押しても外で押しても結果は同じ通常ヒットになり、意味を持たなくなったため
	float GetCriticalRange() const;
	// 通常移動(接地=入力に即セット/空中=エアアクセル。カメラの水平向き基準)
	void UpdateMove(float dt);
	// SPACEでジャンプ。コヨーテタイム(接地を離れた直後の猶予)＋先行入力(着地寸前の入力先読み)つき
	void UpdateJump(float dt);
	// ステップ(回避)：移動入力方向(なければカメラ前方)へ短距離クイックムーブ＋無敵＋クールダウン。
	// ※ 2026/07/20にShiftから右クリック(Accel)の押下へ移した。地上ダッシュの初動がそのまま回避になる
	//   (原神と同じ形)。無敵は反撃(ジャスト回避カウンター)の入口なので、ここが唯一の発生源
	void UpdateDodge(float dt);
	// ステップの最大ストック数(DebugParams「回避/ストック数」)。
	// UpdateDodgeとRespawnの両方で要るので、キー文字列を1箇所にまとめるための小さな包み
	int GetMaxDodgeCharges() const;

	// ステップの速さ(DebugParams「回避/速度」)。
	// UpdateDodge(実行)とClampSpeed(その間だけ上限を緩める)の両方が読む
	float GetDodgeSpeed() const;
	// 加速/ステップ/ダッシュ(右クリック)。接地と空中で意味が変わる：
	//   空中 … 長押しで加速し続ける／短く離して空中ステップ
	//   地上 … 押した瞬間にステップ(回避・UpdateDodgeが担当)／押し続けているあいだダッシュ
	// 地上を「押した瞬間」にしているのは、離すまで待つと単押し判定の時間だけ反応が遅れるため
	void UpdateAccel(float dt);
	// 加速/空中ステップの方向を求める(移動入力→無入力なら進行方向→Jumpで上向きを加算)
	Math::Vector3 GetAccelDir() const;
	// 移動入力の向き(カメラの水平向き基準・水平・正規化済み)。無入力ならゼロベクトル。
	// 通常移動と、壁走り中の「壁を向いてよじ登る」判定で共有する
	Math::Vector3 GetWishDir() const;
	// 速度の上限を掛ける(毎フレームPostUpdateから呼ぶ)。
	// ワイヤーの巻き取り・重力・加速・離脱ブーストがどれも速度を足すだけで減らす仕組みが
	// 無いため、スイングを繋ぐほど際限なく速くなる。それをここで一括して抑える
	void ClampSpeed();
	// 加速中の噴射エフェクトを出す(時間あたりの個数で制御するのでフレームレートに依らない)
	void SpawnBoostFx(const Math::Vector3& _dir, float _dt);
	// フックが着弾した瞬間の火花を出す。刺さった手応えを見せるためのもの
	void SpawnWireImpactFx(const WireAction& _wire);
	// 噴射エフェクトの発生位置(体の中心あたりから加速方向の少し後ろ)
	Math::Vector3 GetBoostSpawnPos(const Math::Vector3& _dir) const;
	// 左クリックが「攻撃」か「ワイヤー」かを判定する。
	// 連続攻撃の受付中、またはE(Focus)を押していてターゲットがいる時だけ攻撃
	bool IsAttackInput() const;
	// 突撃(落下攻撃)を開始する。左クリックを押した瞬間にUpdateWireInputから呼ばれる
	void StartDive();

	// --- ロックオンと関節の選択 ---
	// E(Focus)のON/OFFを切り替える。Targetingの更新より【前】に呼ぶ
	// (このフレームのロック状態で対象を選ばせるため)
	void UpdateLockOnToggle();
	// 対象が確定した後の処理：対象が居なければロックを解き、ホイールで関節を切り替える。
	// Targetingの更新より【後】に呼ぶ(前だと掛けた瞬間に解けてしまう)
	void UpdateLockOnSelection();
	// ロック中の対象が敵なら、その敵を返す。ロックしていない/敵でないならnullptr。
	// ※ 生ポインタで返すのは寿命を持ち出さないため。使うのはそのフレーム内だけ
	Enemy* GetLockedEnemy() const;
	// 今狙っている関節の球の中心(ワールド)。関節が取れなければfalse
	bool GetLockedJointPos(Math::Vector3& _outPos) const;
	// ホイールの入力ぶん、狙う関節を切り替える。壊れた関節は飛ばす
	void CycleLockedJoint(int _step);

	// 反撃(ジャスト回避カウンター)：敵の突進を回避の無敵で受けるとスロー猶予窓を開き、
	// その窓の間に攻撃(左クリック)を押すと今の突撃(ダイブ)へ移行する。回避の早期returnより前で呼ぶ
	void UpdateCounter();
	// 空中スロー(エアフォーカス)：空中でE(Focus)長押し中は時間をスローにして狙う。
	// フォーカスゲージで制限する
	void UpdateAirFocus();
	// 突撃：対象へワイヤーで引き寄せる。斬るのは右クリックの3回目(PerformDiveSlash)で、
	// 斬ったら受付窓中にもう一度押して周りの敵へ続けて突撃する連続攻撃になる。
	// ※ 未ロック時の「真下ダイブ(落下攻撃)」は 2026/08/02 に撤去(到達できないため)
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

	Math::Vector2 SelectTilt()const override;

	// 左腕をワイヤーのアンカーへ向ける(ワイヤー接続中のみ)
	bool SelectArmAimTarget(Math::Vector3& _outTarget)const override;


	// ※ 移動速度はDebugParams("プレイヤー/移動速度")で調整する

	// 移動方向の基準にするカメラ
	std::weak_ptr<CameraBase> m_wpCamera;

	// リスポーンの復帰先(開始位置)
	Math::Vector3 m_spawnPos = {};

	// ステップ(回避)の状態(実行中フラグ/残り時間/ステップ方向/無敵残り)
	bool m_isDodging = false;
	float m_dodgeTimer = 0.0f;
	Math::Vector3 m_dodgeDir = {};

	// ステップのストック(2026/07/20)。単純なクールダウンから変更した。
	// 原神と同じく「2回までは続けてすぐ出せて、使い切ると少し待ってまた出せる」形にするため。
	// 1回のクールダウンだと連打した時に出る/出ないが交互になって挙動が読めなかった
	int m_dodgeCharges = 2;            // 残りストック(0になるまで続けて出せる)
	float m_dodgeRechargeTimer = 0.0f; // 最後にステップしてからの残り待ち時間(0になると2回分まとめて戻る)

	// 次のステップの先行入力の残り猶予。ステップ中に押した分を覚えておき、
	// 今のステップが明けた瞬間に間を空けず次へ繋ぐ(ジャンプの先行入力と同じ考え方)。
	// ※ 猶予がステップ時間より長いため、ステップ開始時に必ず0へ消費すること。
	//   消さないと1回押しただけで2回目が勝手に出る
	float m_dodgeBufferTimer = 0.0f;
	float m_invincibleTimer = 0.0f;   // ※ ダメージ実装後に使う想定(今は参照側が無いので無敵は実質未使用)

	// 左クリックを押した時、それが「ワイヤーとして」だったか。
	// ワイヤーで押した時だけ、離したらワイヤーを外す(攻撃で押した時は離しても無視する)
	bool m_anchorPressWasWire = false;

	// Spaceを押している時間。単押し(空中ステップ)と長押し(ブースト)の区別に使う
	float m_accelHoldTime = 0.0f;

	// ※ 地上ダッシュ(m_isSprinting)は 2026/08/02 の入力のAoT2化で廃止した。
	//   歩きを無くして最初からダッシュにしたので、切り替える対象そのものが無くなった

	// Spaceを押した瞬間、接地していたか。
	// 【なぜ要るか】接地でSpaceを押すとジャンプが出る。その押下をそのまま空中の
	// ブースト/ステップにも使うと「ジャンプすると必ず加速する」ことになるので、
	// この押下は離すまで空中側で無視する
	bool m_accelPressWasGround = false;

	// 噴射エフェクトの発生間隔を測るタイマー(時間あたりの個数を一定に保つ)
	float m_boostFxTimer = 0.0f;

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

	// 着地モーションの残り時間と、前フレームの接地状態(着地した瞬間を捉えるため)。
	// 接地しただけで判定すると着地モーションが1フレームで終わって見えないので、
	// 着地の瞬間に時間をセットして、その間だけ着地モーションを流す
	float m_landingAnimTimer = 0.0f;
	bool m_wasGroundedForAnim = false;
	// チェイン数：一撃ごとに増える(手応えを段階的に強く/将来のコンボUI用)。接地・待機でリセット
	int m_diveChainCount = 0;
	// 斬った後、次の突撃入力を受け付ける残り時間(この間に長押し→離しで次の敵へ続けて突撃)
	float m_comboWindowTimer = 0.0f;
	// 空中スローのフォーカスゲージ(秒)。スロー中は実時間で減り、地上/未使用で回復
	float m_focusGauge = 1.5f;

	// 突撃(連続攻撃)を始めた時の速さ。チェインの間ずっと保持する。
	// 突撃の速度上限をこれ以上には下げない＝勢いを付けて入った攻撃は速いまま繋がる
	// (引き寄せの上限45で一律に頭打ちすると、速く突っ込んでも毎回45に落とされる)
	float m_diveEntrySpeed = 0.0f;

	// 落下攻撃で突撃中の対象(Targetingの選択からコピー。ホーミングの狙い先)
	std::weak_ptr<KdGameObject> m_wpDiveTarget;

	// ロックオン中か。E(Focus)を押すたびに切り替わる(2026/08/02に押しっぱなしから変更)。
	// 【なぜトグルか】関節を狙い分けるにはロックを保ったままホイールを回す必要があり、
	//   押しっぱなしだと「Eを押しながらホイールを回しながら移動する」ことになるため
	bool m_isLockedOn = false;

	// 今狙っている関節の添字(Enemy::kJointDefsの添字)。ロックし直すと0(首)に戻る
	int m_lockedJointIndex = 0;

	// 斬撃の先行入力。間合いの外で攻撃を押した時に立ち、届いた瞬間に【通常】で出る。
	// 【なぜ通常止まりか】早押しでもクリティカルが出ると、間合いを見る意味が消えるため
	bool m_slashBuffered = false;

	// ワイヤー(物理＋見た目を内包)。立体機動装置に合わせて腰の左右から2本。
	// ※ 添字0=左 / 1=右。2本同時に撃つ(DebugFlags「ワイヤー/2本掛け」でOFFにすると0番のみ)
	static constexpr int kWireCount = 2;
	std::array<std::unique_ptr<WireAction>, kWireCount> m_upWires;

	// 1本でも繋がっているか
	bool IsAnyWireAttached() const;

	// 繋がっているワイヤーのうち最初の1本。無ければnullptr。
	// 体の傾き・見た目の線・突撃の描画など「代表1本あればよい」場所が使う
	WireAction* GetAttachedWire() const;

	// 全ワイヤーを外す(リスポーンや状態リセット用)。
	//  _animate ... trueならフックが手元へ帰る見た目を出す。
	//    既定falseにしているのは、自動リリース(壁の遮蔽)やリセットで演出を出すと
	//    フックが壁を貫通して戻るなど不自然になるため。→ WireAction::Release
	void ReleaseAllWires(bool _animate = false);

	// ワイヤーの射出口(腰の左右)のワールド位置。_index 0=左 / 1=右。
	// 立体機動装置は腰に付いているので、見た目の線はここから出す
	Math::Vector3 GetWireMuzzlePos(int _index) const;

	// フック_index(0=左/1=右)が取り付ける面を、自分の側へ扇状に探して方向を返す。
	// 「レティクル方向へ1本」では真横の壁に届かないため(街の実測に基づく。実装のコメント参照)。
	// 見つからなければfalse(そのフックは撃たない)
	bool FindAnchorDir(int _index, const Math::Vector3& _from, const Math::Vector3& _aimDir,
		float _maxLen, Math::Vector3& _outDir);

	// ワイヤーの拘束範囲・アンカー・探索レイをデバッグ表示する(DrawDebugから呼ぶ)
	void DrawDebugWire();

	// 直近の探索レイ1本ぶん。撃った瞬間にしか存在しないので、
	// 「なぜそこに刺さったのか」を後から見られるよう結果を残しておく
	struct WireProbe
	{
		Math::Vector3 from = {};
		Math::Vector3 dir = {};
		float length = 0.0f;   // 当たったらその距離、外れたら探索距離いっぱい
		bool hit = false;
		bool used = false;     // 実際に採用された1本か
	};
	std::vector<WireProbe> m_wireProbes;

	// 壁走り／壁ジャンプ(自動発動。走行中は通常移動とジャンプを止める)
	std::unique_ptr<WallAction> m_upWall;

	// 照準(画面中心の敵を自動ロックオン＋マーカー描画を内包する部品)
	std::unique_ptr<Targeting> m_upTargeting;

	// ※ 移動用の速度は基底CharaBaseの m_velocity(3D) を共通で使う
};
