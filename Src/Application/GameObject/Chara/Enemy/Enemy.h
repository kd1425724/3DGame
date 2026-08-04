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
// weak_ptrで持つだけなので前方宣言でよい(実体のincludeは.cpp側)
class DebrisSystem;

class Enemy : public CharaBase
{
public:

	Enemy()				{}
	~Enemy()	override	{}

	// 敵のモデルのパス。Init と Preload が同じものを指すよう1箇所にまとめてある
	static constexpr const char* kAssetPath = "Asset/Models/Character/StoneGolem/StoneGolem.gltf";

	// このモデルが持つアニメの名前。glTFのanimationsは【この1本だけ】(2026/08/02にファイルを直接読んで確認)。
	// 攻撃・被弾はまだMixamoから取っていない
	static constexpr const char* kWalkAnimName = "walk";

	// 倒れるモーションの名前。
	// 【まだモデルに入っていない】(2026-08-04時点)。Mixamoの Sword And Shield Death を
	//   序盤カット＋ルートモーション除去して取り込む予定。
	//   見つからない間、CharaBase::UpdateAnimation は切り替えず前のアニメを流し続けるので、
	//   取り込むまでは倒れても歩行のままになる(止まりはしない)。
	// 【死亡モーションを別に用意しない理由】倒れた先が終端(起き上がらない)なので、
	//   膝を壊されたダウンも死亡もこれ1本で足りる。
	//   ループOFFで流すと最終フレームで止まるため、ダウン中の待機アニメも兼ねる
	static constexpr const char* kFallAnimName = "fall";

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

	// 「倒れる」だけはループさせない(最終フレームのポーズで留めてダウン中の待機に使う)
	bool SelectAnimationLoop() const override;

	// 切り替えを混ぜる秒数。歩き→倒れるが1フレームで飛ぶのを防ぐ
	float SelectAnimationBlendTime() const override;

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

		// 壊れたときに落ちてくる部位のモデル。
		// 【骨と切り出し範囲を必ず揃えること】gibは「この骨と配下」を関節の平面で
		//   切って作ってある。骨を変えるなら作り直しも要る
		// 【必ず骨のローカル空間に焼き込むこと】頂点は inverse(骨の静止ワールド行列) を
		//   掛けた状態で保存する。平行移動だけだと、骨の静止姿勢に回転がある関節
		//   (肘・膝は約175度)で部位が裏返って出る
		const char* gibModel;

		// 壊れたときに非表示にする【メッシュノード名】(骨名ではない)。
		// StoneGolem.gltf は本体＋部位5つの6メッシュノードに分けてあり、
		// このノードを描かないことで部位が消える。
		//
		// 【なぜボーン潰しをやめたか】CollapseBoneは「その骨にウェイトを持つ頂点を
		//   1点へ集める」操作なので、体に残る切り口がウェイト境界のギザギザな穴
		//   (複数の岩の殻にまたがった開いた弧)になり、断面を塞ぐ手立てが無かった。
		//   ノード単位で消せば切り口は切断平面＝閉じた輪になり、蓋をモデル側に
		//   作り込める(業界標準のやり方)。gibも同じ平面で切ってあるので断面が一致する。
		//
		// 🔴 gibとノードの切り分けは必ず同じ平面で作ること。ずれると
		//   「落ちた部位」と「体の切り口」が食い違う
		const char* partNode;

		// この関節を壊されたら倒れるか(＝膝)。
		// 【なぜ表に持たせるか】添字を直接見て「3と4は膝」と書くと、表を並べ替えたときに
		//   黙って別の関節が対象になる。どの関節が何を引き起こすかは表側の情報にしておく
		bool causesDown;
	};

	// 関節の数。表の実体はEnemy.cppにある
	static constexpr int kJointCount = 5;

	// このモデル(StoneGolem)の関節表。
	// 骨名は推測せずglTFのskins[0].jointsを直接読んで確認した実在の名前(2026/08/02)
	static const JointDef kJointDefs[kJointCount];

	//----------------------------------------
	// 関節を外から狙うための口(Playerのロックオン・突撃が使う)
	//----------------------------------------

	// _index番目の関節の球をワールド座標で返す。
	// 添字が範囲外・骨が無い・既に壊れている場合はfalse(壊れた関節は狙えない)
	bool GetJointSphereAt(int _index, Math::Vector3& _outCenter, float& _outRadius) const;

	// _index番目がまだ壊れていないか。ロックオンの切り替えで壊れた関節を飛ばすのに使う
	bool IsJointAlive(int _index) const;

	// _index番目の関節にダメージを与える。関節HPと本体HPの両方を削り、
	// 関節HPが0になったらその関節の骨を潰す(＝先の部位が消える)
	void ApplyJointDamage(int _index, float _damage);

	// 表示用の関節名(「首」「左肘」など)。添字が範囲外なら空文字
	static const char* GetJointName(int _index);

	// プレイヤーの一撃の威力(DebugParams「プレイヤー/攻撃力」)。
	// 攻撃側と被弾側で別々に書くと既定値が食い違うので、ここ1箇所に集約する
	static float GetAttackPower();

	// 部位を指定しないダメージ。本体HPだけを削り、0で消滅する
	void ApplyBodyDamage(float _damage);

private:

	// ※ 移動速度・旋回速度・攻撃系の数値はDebugParams("敵/…")で調整する

	// 本体HP。0で消滅する。関節を壊しても本体HPは別に残る(モンハン方式)
	// ※ 初期値はInit()でDebugParamsから入れる。ここは「未初期化で0にならない」ための保険
	float m_hp = 1.0f;

	// 関節ごとのHP。0になったらその関節の骨を潰す。添字はkJointDefsと同じ
	float m_jointHp[kJointCount] = {};

	// その関節のgibを既に出したか。
	// 【なぜ要るか】UpdateBrokenJointsは壊れた関節を毎フレーム潰し直すので、
	//   ここで区別しないと部位が毎フレーム降ってくる
	bool m_gibSpawned[kJointCount] = {};

	// gibモデルのDebrisSystem側のID。-1=未登録(初めて壊れたときに登録する)
	int m_gibModelIds[kJointCount] = { -1, -1, -1, -1, -1 };

	// 破片を出す先。シーンに1つあるので初回に探してキャッシュする
	std::weak_ptr<DebrisSystem> m_wpDebrisSystem;

	// _index番目の関節の部位を、その関節の姿勢で落とす。壊れた瞬間に1回だけ呼ぶ
	void SpawnGib(int _index);

	// メッシュノードを1つ非表示にする(＝その部位を消す)。壊れた瞬間に1回だけ呼ぶ。
	// ノードの可視はアニメで書き戻されないので、毎フレームやり直す必要は無い
	// (ボーン潰し方式では毎フレーム潰し直す必要があった)
	void HidePartNode(const char* _nodeName);

	// 体の当たり半径(m)。突進の命中判定とデバッグ表示に使う。
	// 【2026/07/29】KdColliderへの登録をやめたので、用途はこの2つだけになった。
	// 【2026/07/30】立方体へ戻したので 1.8 → 0.6 に戻した。
	//   メカ(幅3.77 × 奥行3.00)を使う場合は、体を包む球としておよそ1.8mが適正
	float m_hitRadius = 0.6f;

	// 追従・攻撃の対象
	std::weak_ptr<KdGameObject> m_wpTarget;

	// --- 攻撃の状態機械：追従 → 予備動作(予告) → 突進 → 硬直 → 追従 ---
	// Down は片道。膝を壊されるか本体HPが尽きると入り、二度と出てこない
	// (「片膝を斬られた時点で立ち上がれない」という仕様)
	enum class State { Chase, Windup, Strike, Recover, Down };
	State m_state = State::Chase;
	float m_stateTimer = 0.0f;          // 現在状態の残り時間(Windup/Strike/Recoverで使用)
	Math::Vector3 m_lungeDir = {};      // 突進方向。Windup終了時に固定する(以後は追尾しない=回避で避けられる)

	// 倒れた状態へ入る。_isDead=trueなら「死んで倒れた」＝そのまま消える(将来は全身破砕へ繋ぐ)、
	// falseなら「膝を壊されて倒れた」＝生きていて、ダウンのまま戦い続ける
	void EnterDown(bool _isDead);

	// 倒れている間の処理(向き直りと、死亡時の消滅までの管理)
	void UpdateDown(float _dt);

	// 死んで倒れてから消えるまでの秒数。
	// 【仮】全身破砕を入れたら「倒れ切った瞬間に砕く」へ差し替わる。
	//   それまで死体が残り続けないようにするためのつなぎ
	float GetDownDisappearTime() const;

	// 死んで倒れたのか(falseなら膝を壊されただけで、まだ生きている)
	bool m_isDead = false;

	// 死んで倒れてから消えるまでの残り秒数
	float m_downTimer = 0.0f;

	// 【確認用】DebugFlags「敵/動きを止める」。AIと移動を止め、アニメも姿勢を凍らせる。
	// 関節の球や部位破壊を見比べるとき、敵が歩き回っていると確認しづらいため
	bool IsFrozenForDebug() const;

	// 関節の球(中心と半径)をワールド座標で取り出す。半径もワールド(スケール済み)で返す。
	// 骨が見つからなければfalse(モデルを差し替えて骨名が変わっても落ちない)
	bool GetJointSphere(const JointDef& _joint, Math::Vector3& _outCenter, float& _outRadius) const;

	// 関節の球をデバッグ表示する。
	// 半径が妥当かは目で見ないと決められないので、命中判定を書く【前】に用意する
	void DrawJointDebug();

	// 壊れた関節の骨を毎フレーム潰し直す。
	// アニメが毎フレーム骨を書き戻すので、1回潰すだけでは次のフレームで元に戻る
	void UpdateBrokenJoints();

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
