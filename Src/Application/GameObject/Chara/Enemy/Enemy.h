#pragma once

#include "../CharaBase.h"

//====================================================
//
// 敵キャラクター(石のゴーレム)
//  ・追従対象(Player)に近づき、間合いまで来たら止まる
//  ・プレイヤーの攻撃で関節と本体HPが削れ、膝が壊れるか本体HPが尽きると倒れる
//
//  ※ 攻撃手段は【今は無い】。敵がBlock.gltfの立方体だった頃の「予備動作→突進」は
//    2026-08-04に撤去した(見た目が25mのゴーレムに変わって突進が成立しなくなったため)。
//    🔴 これでプレイヤーの反撃(ジャスト回避カウンター)を発動させる手段が無くなっている。
//      ダウン攻撃などを入れるときに、そちらから NotifyCounter を呼び直すこと
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

	// 敵が使う破片(gib5個 + 全身破砕30個)をまとめて読み込ませる。
	// 🔴 シーン構築時に【1回だけ】呼ぶこと。
	//   敵はスポナーで後から湧くので、敵の生成を待つと35モデルの読み込みと凸包構築が
	//   【ゲーム中に】走る。実測でFPSが5まで落ちた(2026-08-05)。
	//   読み込み画面のうちに払っておけば、湧いた敵は登録済みIDを引くだけで済む
	static void PreloadDebrisAssets(DebrisSystem& _debris);

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

	// メッシュノードを1つ非表示にする(部位を消す／全身破砕で本体を消す)。1回だけ呼べばよい。
	// ノードの可視はアニメで書き戻されないので、毎フレームやり直す必要は無い
	// (ボーン潰し方式では毎フレーム潰し直す必要があった)
	void HideMeshNode(const char* _nodeName);

	// 破片を出す先(シーンに1つ)を探す。見つからなければnullptr。
	// 【なぜInit()で探さないか】Init()の時点ではシーンの構築が終わっていない。
	//   一度見つけたら m_wpDebrisSystem にキャッシュする
	std::shared_ptr<DebrisSystem> FindDebrisSystem();

	// gibと全身破砕の破片を先に読み込んでおく。Updateから毎フレーム呼んでよい(1回で済む)
	void PreloadDebrisModels();

	// 上を済ませたか
	bool m_debrisPreloaded = false;

	// 全身破砕の破片1つぶん。fragments.json(破砕ツールの出力)から読む
	struct FragmentDef
	{
		std::string	modelPath;			// Frag_XX.gltf
		std::string	bone;				// 支配ボーン。この骨の姿勢で破片を置く
		int			modelId = -1;		// DebrisSystem側のID
	};

	// 【なぜコードに表を持たないか】破片の数や骨の割り当ては破砕ツールの出力で決まる。
	//   コードに焼くと、ツールを流し直すたびに手で書き換えることになり、必ず食い違う
	std::vector<FragmentDef> m_fragments;

	// 破片の表(fragments.json)を読む。読めなければfalse
	// (その場合は破砕せず、従来どおり消えるだけ)
	static bool ReadFragmentTable(std::vector<FragmentDef>& _out);

	// 体の当たり半径(m)。追従を止める間合いの基準とデバッグ表示に使う。
	// 【2026/07/29】KdColliderへの登録をやめたので、用途はこの2つだけになった。
	// 【2026/08/04】突進を撤去したので、命中判定の用途は無くなった。
	// 【2026/07/30】立方体へ戻したので 1.8 → 0.6 に戻した。
	//   メカ(幅3.77 × 奥行3.00)を使う場合は、体を包む球としておよそ1.8mが適正
	float m_hitRadius = 0.6f;

	// 追従・攻撃の対象
	std::weak_ptr<KdGameObject> m_wpTarget;

	// --- 状態 ---
	// Chase(追う) → Windup(振りかぶる) → Strike(振り下ろす) → Recover(硬直) → Chase
	// Down は片道。膝を壊されるか本体HPが尽きると入り、二度と出てこない
	// (「片膝を斬られた時点で立ち上がれない」という仕様)
	//
	// ※ 2026-08-04に立方体時代の「予備動作→突進→硬直」を撤去したが、
	//   2026-08-05に【腕の振り下ろし】として作り直した。突進ではないので敵は動かない
	enum class State { Chase, Windup, Strike, Recover, Down };
	State m_state = State::Chase;

	// 今の状態の残り秒数(Windup/Strike/Recoverで使う)
	float m_stateTimer = 0.0f;

	// 次に攻撃できるまでの残り秒数。連続で殴り続けないための間
	float m_attackCooldown = 0.0f;

	// この振りで既に当てたか。判定球は数フレーム重なり続けるので、
	// これが無いと1回の振りで何度もノックバックする
	bool m_hitDoneThisSwing = false;

	// 右腕で振るか(振るたびに入れ替える)。判定球もアニメもこれで左右を切り替える。
	// 食い違うと「当たっていないのに食らう」という一番たちの悪い状態になる
	bool m_swingRight = false;

	// 前フレームの判定球。掃引(すり抜け防止)で「どこからどこへ動いたか」に使う。
	// 🔴 振り下ろしの最速時、手は1フレームで9.48m動く。判定球1.58m＋プレイヤー0.6mでは
	//   その場の位置だけ見ても間にいたプレイヤーを飛び越す(2026-08-05にアニメを実測)
	std::vector<std::pair<Math::Vector3, float>> m_prevSpheres;
	bool m_hasPrevSpheres = false;

	// 攻撃の状態へ入る
	void EnterWindup();
	void EnterStrike();
	void EnterRecover();

	// 攻撃の判定球(腕に沿って数個)をワールドで作る。
	// 【なぜ複数か】手だけに付けると腕の中ほどが素通りする
	void BuildAttackSpheres(std::vector<std::pair<Math::Vector3, float>>& _out) const;

	// 判定球がプレイヤーに当たっているか調べ、当たっていれば反撃/ノックバックへ回す。
	// 当てたらtrue(1回の振りで1度だけ)
	bool ResolveAttackHit(const std::shared_ptr<KdGameObject>& _target);

	// 攻撃の判定球をデバッグ表示する
	void DrawAttackDebug();

	// 直近のUpdateで実際に出した水平の速さ。SelectAnimationSpeedが足を滑らせない倍率に使う。
	// 状態から逆算すると「間合いで止まっているのに歩いている」ズレが出るので、結果を持たせる
	float m_currentMoveSpeed = 0.0f;

	// 倒れた状態へ入る。_isDead=trueなら「死んで倒れた」＝倒れ切ったあと全身破砕へ移る、
	// falseなら「膝を壊されて倒れた」＝生きていて、ダウンのまま戦い続ける
	void EnterDown(bool _isDead);

	// 倒れている間の処理(向き直りと、死亡時の消滅までの管理)
	void UpdateDown(float _dt);

	// 倒れるモーションを最後まで流し終えたか。
	// 【なぜ要るか】倒れている【最中】に向きを変えると体が滑るように回り、
	//   消滅も倒れ切る前に始まってしまう(2026-08-04に実機で両方とも確認された)
	bool IsFallFinished() const;

	// 倒れ切ってから砕けるまでの秒数(倒れ伏した姿を見せる間)。
	// 0にすると倒れ終わった瞬間に砕ける
	float GetShatterDelayTime() const;

	// 全身破砕。倒れ切ってから1回だけ呼ぶ。
	//   ①まだ体に付いている部位を落とす ②本体を消す ③本体の破片を出す ④自分は退場する
	// 【5-Aの仮実装】③で出しているのは本物の破片ではなく既存のgibモデルの使い回し。
	//   数だけ本番と揃えてあるので、生成コストは本番と同じ経路で測れる
	void Shatter();

	// 死んで倒れたのか(falseなら膝を壊されただけで、まだ生きている)
	bool m_isDead = false;

	// 倒れ切ってから砕けるまでの残り秒数
	float m_downTimer = 0.0f;

	// 既に砕けたか(二重に破片を出さないため)
	bool m_shattered = false;

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


	// 移動速度(DebugParams)。キーと既定値を1箇所に集約する
	// (別々に書くと既定値が食い違ったとき、先に呼ばれたほうが黙って勝つ)
	float GetMoveSpeed() const;
};
