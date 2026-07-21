#pragma once

//====================================================
//
// 全キャラクター(Player/Enemyなど)の基底クラス
//  ・モデル表示(色味を変えて種類を見分けられるようにcolRateを持つ)
//  ・重力＋垂直速度を持ち、下方向レイで地面(KdCollider::TypeGround)に着地する
//  ・ジャンプ対応：上昇中や空中では地面に吸着せず、落下して着地したときだけ立たせる
//
//====================================================
class CharaBase : public KdGameObject
{
public:

	// ワイヤー(移動技)はスイングでこのキャラの速度・位置・当たり解決を直接動かすため、
	// 再利用できる部品として CharaBase& を操作する。protectedへアクセスするのでfriendにする
	friend class WireAction;

	// 壁走り(WallAction)も同様に、このキャラの速度・重力倍率・壁の接触情報を直接扱う部品
	friend class WallAction;

	CharaBase()				{}
	virtual ~CharaBase() override {}

	void SetAsset(const std::string& assetName) override;

	void DrawLit() override;

	// 影生成：光から見た深度マップに自分のモデルを描く(影を落とす側)。
	// BaseScene が BeginGenerateDepthMapFromLight〜End の間で呼ぶ。中身はDrawLitと同じモデル描画で、
	// このときシェーダは深度書き込みモードになっている(BeginGenerateDepthMapFromLightが切替済み)
	void GenerateDepthMapFromLight() override;

	// 水平方向(xz)の速さを返す。HUDの速度メーターなど表示用
	float GetHorizontalSpeed() const
	{
		return sqrtf(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
	}

protected:

	// 重力を適用して垂直移動し、落下中に地面(TypeGround)へ着地したらその高さに立たせる
	// ※ PostUpdate等の「world状態の解決」フェーズで毎フレーム呼ぶ想定
	void GroundCheck();

	// 移動後の位置posを受け取り、地面(TypeGround)に潜っていたら立つ高さへ押し上げて
	// 縦の落下を止める。接地状態(m_isGrounded)も更新する。
	// ※ 重力・移動の積分は呼び出し側で済ませておくこと(この関数は地面との"解決"だけ行う)
	// ※ ワイヤー中など、GroundCheckを通さず自前で移動するときのすり抜け防止にも使う
	//
	// _allowLanding = false にすると「着地」しなくなる(2026/07/19 追加)。
	//   地面へのめり込みだけ直して落下は止めるが、接地フラグを立てず、着地の手応え
	//   (CameraShake用のm_landingImpact)も記録しない。
	//   ワイヤーで地面スレスレを飛ぶ用。着地扱いにするとそのたびに止まって勢いが死ぬため。
	void ResolveGround(Math::Vector3& pos, bool _allowLanding = true);

	// 上昇中(ジャンプ/ワイヤー/ダイブ)に頭上の天井(TypeBump)へ潜り込むのを止める。
	// 移動前の頭の高さ fromPos から現在の頭の高さまで上向きレイで掃引し、天井があれば
	// 頭がその直下に収まる高さへクランプして上向き速度を0にする(次フレームから重力で落ちる)。
	// ※ ResolveGroundの上下反転(下から上)。始点を「頭」にするのは、体の中心より下から飛ばすと
	//    立っている床の天面を裏から拾って誤検知するため。射程が移動量ぶん伸びるので高速上昇の
	//    トンネリングも拾える。縦専用=水平の壁はResolveBump/Sweepが担当。落下中(velocity.y<=0)は何もしない
	void ResolveCeiling(const Math::Vector3& fromPos, Math::Vector3& pos);

	// 体を球で近似し、Block等(TypeBump)にめり込んでいたら水平方向へ押し出す(壁として働く)
	// ※ 縦の乗り上げ/着地はResolveGroundが担当。ワイヤー中もすり抜け防止に使う
	void ResolveBump(Math::Vector3& pos);

	// 高速移動で壁(TypeBump)を1フレームで飛び越える(トンネリング)のを防ぐ連続当たり判定。
	// フレーム開始位置fromPosから移動後posへ体の高さで水平レイを飛ばし、途中に壁があれば
	// その手前(半径ぶん外側)で止め、壁へ向かう水平速度を消す。ResolveBumpの前に呼ぶ安全弁。
	// ※ ワイヤーのスイング/フリングなど水平速度が大きいときに効く(低速時はResolveBumpで足りる)
	void ResolveBumpSweep(const Math::Vector3& fromPos, Math::Vector3& pos);

	// 接地中ならジャンプする(垂直速度に初速を与える。初速はDebugParams"キャラ/ジャンプ力")
	void Jump();

	// 接地判定を無視して実際にジャンプ初速を与える。
	// 接地の可否は呼び出し側で判断する用(コヨーテタイム/先行入力など)
	void DoJump();

	// 接地しているか
	bool IsGrounded() const { return m_isGrounded; }

	// ※ IsWallBetween(from→toの間に壁があるか) は 2026/07/19 に CollisionGrid へ移動した。
	//    キャラに依存しない世界クエリで、照準(Targeting)の遮蔽判定からも使うため。
	//    → CollisionGrid::IsWallBetween(from, to, margin)

	// 直近フレームの着地/壁ヒットの衝撃の大きさを取り出す(読むと0に戻すConsume方式)。
	// カメラの手応え(CameraShake)の発火に使う。ResolveGround/ResolveBumpSweepが記録する。
	// ※ 記録は全キャラ共通だが、実際にカメラを揺らすのはPlayerだけ(Enemyは呼ばない)
	float ConsumeLandingImpact()
	{
		float v = m_landingImpact;
		m_landingImpact = 0.0f;
		return v;
	}
	float ConsumeWallImpact()
	{
		float v = m_wallImpact;
		m_wallImpact = 0.0f;
		return v;
	}

	// アニメーションを再生する。SelectAnimation()が返した名前のアニメへ切り替え、時間を進める。
	// ※ アニメは「接地・速度が確定したあとの結果」なので、PostUpdateの最後で呼ぶ
	//    (CLAUDE.mdの「Updateは意思決定・PostUpdateはworld状態の解決」に合わせた位置)
	void UpdateAnimation();

	// いま再生すべきアニメ名を返す。空文字なら再生しない(アニメを持たないキャラ用)。
	// 状態の判断は派生クラスの責務で、CharaBaseは「再生する仕組み」だけを持つ
	virtual std::string SelectAnimation() const { return ""; }

	// 再生速度の倍率(1.0=等速)。走りを実際の速さに合わせて速める用。
	// 同じ走りモーションを歩きでもダッシュでも等速で流すと、足が地面を滑って見える
	virtual float SelectAnimationSpeed() const { return 1.0f; }

	// 再生中のアニメ名(デバッグ表示から読む)
	const std::string& GetCurrentAnimName() const { return m_currentAnimName; }

	// SelectFacingDir()が返した向きへ、体をなめらかに回す(Y軸のみ)。
	// ※ 向きは「速度が確定したあとの結果」なのでアニメと同じくPostUpdateの最後で呼ぶ。
	//    呼ぶかどうかは派生クラスが決める(既定では誰も呼ばないのでEnemy等の挙動は不変)
	void UpdateFacing(float _deltaTime);

	// 向くべき方向(世界座標)。長さが0に近ければ「向きを変えない」の意味になる。
	// 既定は水平方向の速度＝進んでいる方向を向く。
	// 判断は派生クラスの責務で、CharaBaseは「回す仕組み」だけを持つ(SelectAnimationと同じ分け方)
	virtual Math::Vector3 SelectFacingDir() const { return Math::Vector3(m_velocity.x, 0.0f, m_velocity.z); }

	// 1秒あたり何度まで回れるか。小さいほどぬるっと向き直る
	virtual float SelectTurnSpeed() const { return 720.0f; }

	// 体の半分の高さ(pos=体の中心 から足元までの距離)。
	// 当たり判定(接地/天井/壁)が「足元」「頭」を求めるのに使う共通の基準。
	// ※ 以前は GetScale().y*0.5 を直接使っており「モデル＝1辺1mの立方体」前提だった。
	//    実寸のキャラモデルに差し替えたとき当たりと見た目がずれたのでここへ集約した
	float GetBodyHalfHeight() const { return m_bodyHeight * GetScale().y * 0.5f; }

	// モデル描画用の行列。原点が足元にあるモデルは半身ぶん下げて pos(体の中心)に合わせる。
	// ※ 影(GenerateDepthMapFromLight)も必ずこれで描くこと。片方だけだと影がずれる
	Math::Matrix GetDrawMatrix() const;

	// 表示用モデルワーク
	KdModelWork m_modelWork;

	// 体の高さ(実寸m)。既定1.0は「1辺1mの立方体」だった頃と同じ挙動(Enemyなどは据え置き)
	float m_bodyHeight = 1.0f;

	// モデルの原点が足元にあるか。trueなら描画時に半身ぶん下げる。
	// 既定false=原点が中心のモデル(Block等)。実寸のキャラモデルだけtrueにする
	bool m_modelOriginIsFeet = false;

	// モデルの正面が -Z を向いているか。UpdateFacingが向きを計算するときの符号に効く。
	// ※ どちらになるかはBlenderのエクスポート設定次第で、モデルごとに違う。
	//    実測: Scifi_girl = -Z(true) / GogglesChara = +Z(false)
	//    間違えると「進行方向のちょうど逆」を向くので、モデル差し替え時は必ず確認すること
	bool m_modelForwardIsMinusZ = true;

	// アニメーション再生機と、いま流しているアニメ名。
	// 名前を覚えておくのは、同じアニメのときにSetAnimationを呼び直さないため
	// (呼ぶたび再生時間が0に戻るので、毎フレーム呼ぶと先頭で固まって見える)
	KdAnimator m_animator;
	std::string m_currentAnimName;

	// 描画時の色味(サブクラスで変更して他のキャラクターと見分けられるようにする)
	Math::Color m_color = kWhiteColor;

	// 速度(3D)。重力・ジャンプ・移動・ワイヤーが共通で使う
	// ・y … 重力/ジャンプ(GroundCheckが積分し、着地で0に戻す)
	// ・x,z … 水平移動。各キャラのUpdateが設定する(Enemyは使わず0のまま)
	// GroundCheckがこの速度でまとめて位置を進めるので、スイング後の勢いも自然に残る
	Math::Vector3 m_velocity = {};

	// 接地しているか(GroundCheckで毎フレーム更新)
	bool m_isGrounded = false;

	// 手応え用：直近フレームに発生した着地/壁ヒットの衝撃(速度の大きさ)。
	// ResolveGround(着地遷移時)/ResolveBumpSweep(壁で止めた時)が記録し、Consume〜で読み取る
	float m_landingImpact = 0.0f;
	float m_wallImpact = 0.0f;

	// 壁に押し付き続けているか(壁の手応えを"当たった瞬間"だけにするエッジ検出用)。
	// これが無いと連続攻撃で壁に突っ込み続けたとき毎フレームtraumaが入りシェイクが終わらない
	bool m_wasHittingWall = false;

	// 直近のResolveBumpで壁(TypeBump)に触れていたか＆その壁の法線(水平・正規化済み)。
	// 壁走りはこれを見るだけで判定でき、専用の当たり判定を撃たなくて済む
	// (ResolveBumpは押し出しのために既に法線を計算しているので、記録するだけならタダ)
	bool m_isTouchingWall = false;
	Math::Vector3 m_wallNormal = {};

	// 重力の倍率(1.0=通常)。壁走り中に0にして落下を止めるために使う。
	// GroundCheckが重力を積分するときに掛かる
	float m_gravityScale = 1.0f;

	// ※ 重力加速度・ジャンプ初速はDebugParams("キャラ/重力"・"キャラ/ジャンプ力")で調整する
};
