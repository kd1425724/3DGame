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

	// 敵のモデルのパス。Init と Preload が同じものを指すよう1箇所にまとめてある
	static constexpr const char* kAssetPath = "Asset/Models/Character/StoneGolem/StoneGolem.gltf";

	// このモデルが持つアニメの名前。glTFのanimationsは【この1本だけ】(2026/08/02にファイルを直接読んで確認)。
	// 攻撃・被弾・死亡はまだMixamoから取っていない
	static constexpr const char* kWalkAnimName = "walk";

	// モデルとテクスチャを先読みしてキャッシュへ載せる。シーンのInitから1回呼ぶ。
	// 【なぜ要るか】最初の1体が出現した瞬間に読み込みが走って画面がかくつくため。
	//   実機で「出た瞬間だけ」と確認できたので、描画コストではなく読み込みが原因
	static void Preload();

	void Init()			override;
	void Update()		override;
	void PostUpdate()	override;

	// 再生するアニメ名(CharaBase::UpdateAnimationが毎フレーム呼ぶ)
	std::string SelectAnimation() const override;

	// 再生速度の倍率。足が地面を滑らないよう、実際の移動速度から計算する
	float SelectAnimationSpeed() const override;

	// 当たり判定デバッグ表示は「敵」カテゴリに出す(接地/壁判定はCharaBaseが描くので、
	// ここで種類を教えないとプレイヤーと一緒くたに出てしまう)
	DebugDraw::Category GetDebugCategory() const override;

	// 登録済みコライダー(KdColliderが緑の球で描く)を「敵」カテゴリのときだけ出す
	void DrawDebug() override;

	// 種別タグ：シーン内からEnemyを探すときの判定に使う(dynamic_pointer_castの代わり)
	ObjectTag GetObjectTag() override { return ObjectTag::Enemy; }

	// 攻撃(レーザー等)に当たったときに呼ばれる：消滅する
	void OnHit(KdGameObject* _other) override;

	// 追従・接触判定の対象を設定する
	void SetTarget(const std::shared_ptr<KdGameObject>& target) { m_wpTarget = target; }

	//----------------------------------------
	// 狙える関節(部位破壊とワイヤーの掛け先を兼ねる)
	//----------------------------------------

	// 狙える場所は【関節だけ】で、1関節あたり球ひとつ。首・左右の肘・左右の膝の5つ。
	//
	// 【なぜ部位全体ではなく関節か】ワイヤーは関節の引っ掛かりにしか打てない仕様なので、
	//   ワイヤーの掛け先と部位破壊の的が同じものになる。表をひとつにすれば両方が同じ点を指す。
	//   腕や脚を丸ごと包むより判定も表現もはるかに単純で、狙う楽しさも関節に集まる。
	//
	// 【潰す骨を別に持たない理由】関節の骨をそのまま潰せば配下が道連れで消える。
	//   肘(ForeArm)を潰せば前腕と手、膝(Leg)なら脛と足、首(Neck)なら頭が消える＝関節の骨で足りる
	struct JointDef
	{
		const char* name;           // 表示用の関節名
		const char* bone;           // 判定の中心になる骨。破壊時に潰す骨も兼ねる
		const char* radiusKey;      // 半径のDebugParamsキー(実機で見ながら詰めるため外に出す)
		float       defaultRadius;  // 半径の既定値【モデル座標系】。ワールドで使う側がスケールを掛ける
		float       damageScale;    // ダメージ倍率。弱点ほど大きい
		float       maxHp;          // 関節のHP。本体HPとは別勘定(モンハン方式)
	};

	// 関節の数。表の実体はEnemy.cppにある
	static constexpr int kJointCount = 5;

	// このモデル(StoneGolem)の関節表。
	// 骨名は推測せずglTFのskins[0].jointsを直接読んで確認した実在の名前(2026/08/02)
	static const JointDef kJointDefs[kJointCount];

private:

	// ※ 移動速度・旋回速度・攻撃系の数値はDebugParams("敵/…")で調整する

	// 体の当たり半径(m)。突進の命中判定とデバッグ表示に使う。
	// 【2026/07/29】KdColliderへの登録をやめたので、用途はこの2つだけになった。
	// 【2026/07/30】立方体へ戻したので 1.8 → 0.6 に戻した。
	//   メカ(幅3.77 × 奥行3.00)を使う場合は、体を包む球としておよそ1.8mが適正
	float m_hitRadius = 0.6f;

	// 追従・攻撃の対象
	std::weak_ptr<KdGameObject> m_wpTarget;

	// --- 攻撃の状態機械：追従 → 予備動作(予告) → 突進 → 硬直 → 追従 ---
	enum class State { Chase, Windup, Strike, Recover };
	State m_state = State::Chase;
	float m_stateTimer = 0.0f;          // 現在状態の残り時間(Windup/Strike/Recoverで使用)
	Math::Vector3 m_lungeDir = {};      // 突進方向。Windup終了時に固定する(以後は追尾しない=回避で避けられる)

	// 【確認用】DebugFlags「敵/動きを止める」。AIと移動を止め、アニメも姿勢を凍らせる。
	// 関節の球や部位破壊を見比べるとき、敵が歩き回っていると確認しづらいため
	bool IsFrozenForDebug() const;

	// 関節の球(中心と半径)をワールド座標で取り出す。半径もワールド(スケール済み)で返す。
	// 骨が見つからなければfalse(モデルを差し替えて骨名が変わっても落ちない)
	bool GetJointSphere(const JointDef& _joint, Math::Vector3& _outCenter, float& _outRadius) const;

	// 関節の球をデバッグ表示する。
	// 半径が妥当かは目で見ないと決められないので、命中判定を書く【前】に用意する
	void DrawJointDebug();

	// 突進が命中した瞬間の処理(無敵なら反撃成立で自滅／無防備ならノックバック)
	void ResolveStrikeHit(const std::shared_ptr<KdGameObject>& _target);
	// 硬直状態へ移行する(突進の後隙)
	void EnterRecover();

	// 移動速度・突進速度(DebugParams)。Update(実際に動かす)と
	// SelectAnimationSpeed(足を滑らせない再生倍率)の両方から読むので、キーと既定値を
	// ここに集約する。別々に書くと既定値が食い違ったとき、先に呼ばれたほうが黙って勝つ
	float GetMoveSpeed() const;
	float GetLungeSpeed() const;
};
