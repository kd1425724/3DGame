#include "Enemy.h"

#include "../../../main.h"
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugDraw/DebugDraw.h"
#include "../Player/Player.h"   // 突進命中時にPlayerの無敵判定/反撃通知/ノックバックを呼ぶため

DebugDraw::Category Enemy::GetDebugCategory() const
{
	return DebugDraw::Category::Enemy;
}

void Enemy::DrawDebug()
{
	// KdGameObject::DrawDebug は s_showColliderDebug しか見ないので、
	// そのまま呼ぶと登録済みコライダー(緑の球)がカテゴリに関係なく出てしまう。
	// 「敵」がOFFのときは基底の間だけフラグを落とす
	DebugDraw::ScopedGate gate(DebugDraw::Category::Enemy);
	KdGameObject::DrawDebug();
}

void Enemy::Preload()
{
	// モデルとテクスチャを先に読み込んでキャッシュへ載せる。
	//
	// 【なぜ必要か】KdAssetsはパス単位のキャッシュで、最初に要求した時に
	//   glTFの解析・GPUバッファ生成・テクスチャ読み込み(＋ミップ生成)をまとめて行う。
	//   そのため【最初の1体が出現した瞬間に画面がかくつく】。
	//   メカ(W9231)でも同じ症状が出ていた(2026/07/30)。実機で「出た瞬間だけ」と
	//   確認できたので、描画コストではなく読み込みが原因と判明した(2026/07/31)。
	//
	//   シーン開始時に一度読んでおけば、以降の出現は既にキャッシュにあるので無料になる。
	//   ※ モデルを読むとマテリアル経由でテクスチャも一緒に読まれるので、これ1回で足りる
	KdAssets::Instance().m_modeldatas.GetData(kAssetPath);
}

void Enemy::Init()
{
	// 【2026/07/31】敵の見た目を石のゴーレムにした。
	// 画像生成 → Meshyで3D化＋テクスチャ → Mixamoで自動リグ、という経路で作った。
	// 制作手順は Desktop\Cloude\Project\3DGame\Doc\Golem_Pipeline.md
	// ライセンスは CC BY 4.0(Meshyのクレジット必須) → THIRD_PARTY_LICENSES.txt
	SetAsset(kAssetPath);

	// ※ m_modelOriginIsFeet は既定の false のまま。
	//   このモデルの原点は【足元ではなく体の中心】(glTFの頂点Y範囲が -0.951〜+0.948)。
	//   プレイヤーやメカ(原点＝足元)とは違うので、ここを true にすると半身ぶん沈む

	// モデルの実寸は高さ1.899m。街の実測から導いた目標は12〜18mで、
	// 「通りを移動できる幅15mが上限、その比率だと高さ17〜18mが限界」。
	// 【2026/07/31】実機で15mを見て「もっと大きくてよい」となったので上限側の18mへ。
	// 実機で回して決められるようDebugParamsに出してある(出現し直すと反映される)
	float targetHeight = DebugParams::Instance().Float(U8("敵/身長"), 18.0f, 1.0f, 40.0f);
	float modelHeight = 1.899f;

	m_bodyHeight = modelHeight;
	SetScale(Math::Vector3::One * (targetHeight / modelHeight));

	// 体の当たり半径。モデルは幅1.221m(腕を広げた全幅)なので、
	// 胴に寄せて身長の1/4程度を初期値にする。実機で見て詰める値
	m_hitRadius = targetHeight * 0.25f;

	// 正面は +Z。実機で見て確定させた(2026/07/31)。
	// 🔴 ここは【実機で見て決める】以外に方法が無い。データや軸変換から推論すると必ず逆になる。
	//   同じMixamoリグのGogglesCharaが -Z(true) だったので true から始めたが、実機では逆だった。
	//   「同じ経路で作ったモデルなら同じ向き」も成り立たない、ということ
	m_modelForwardIsMinusZ = false;

	// --- 戦闘メカ(W9231)を使う場合の設定。戻すときはここを有効にして上を消す ---
	// 部位破壊の題材として選んだモデル。骨が Arm_L / Minigun_L / Camera / Top_Leg_L … と
	// 最初から機械の部位で分かれており、部位ごとの当たり判定を骨に追従させやすい。
	// ライセンスは CC BY 4.0(作者クレジット必須) → THIRD_PARTY_LICENSES.txt
	// あわせて Enemy.h の m_hitRadius を 0.6 → 1.8 に戻すこと(幅3.77×奥行3.00の実測から)。
	//SetAsset("Asset/Models/Character/W9231Mech/W9231Mech.gltf");
	//
	// 実寸モデルなので等倍。原点は足元にある(実測 足元Z=0.018)
	//SetScale(Math::Vector3::One);
	//m_modelOriginIsFeet = true;
	//
	// 高さの実測値。スキン変形を計算して求めた値(幅3.764 × 高さ4.368 × 奥行3.000)。
	// 【罠】このモデルの生の頂点座標は±19000という巨大な値で、スキン変形で初めて
	//   正しい位置に戻る。頂点座標をそのまま測ると桁が違う値が出るので必ず変形後を見ること。
	// GetBodyHalfHeight()経由で接地・天井・壁の判定が足元と頭の位置を出すのに使う
	//m_bodyHeight = 4.37f;
	//
	// 正面は +Z。実機で見て確定させた(2026/07/29)。
	// 【罠】glTFのデータ上ではセンサー(Camera_08)が -Z 側にあるので -Z が正面に見える。
	//   しかしローダーは頂点の z を反転する(KdGLTFLoader.cpp:501 の `* -1`)ため、
	//   glTFの -Z はエンジンでは +Z になる。
	//   データから推論すると必ず逆になるので、この値は実機で見て決めること。
	//m_modelForwardIsMinusZ = false;

	// ※ 当たり判定(KdCollider)は登録しない。立方体へ戻す際も復活させていない。
	//   理由は下記①のとおり「登録しても誰も問い合わせていなかった」ため、
	//   戻すと動かないコードが復活するだけになる。
	//   ①以前登録していた "EnemyDamage"(TypeDamage)は Src/Application 全体で
	//     誰も問い合わせていなかった(実際の命中判定はPlayer側の距離チェック)
	//   ②部位破壊の判定は【骨のワールド座標から毎フレーム球を作る自前計算】で行う。
	//     KdColliderに登録した形状はモデルのローカル座標に固定で、
	//     RegisterCollisionShapeがemplace(同名で上書きされない)なので、
	//     アニメーションで動く骨には追従できない
	//   ③モデルを RegisterCollisionShape に渡してはいけない。
	//     このモデルはCOLノードを持たないので、渡すと見た目の70,832三角が
	//     そのまま当たり判定メッシュになる(KdModel.cpp:107-110のフォールバック)
}

void Enemy::OnHit(KdGameObject* /*_other*/)
{
	// 攻撃に当たったら消滅する
	m_isExpired = true;
}

void Enemy::Update()
{
	// 追従対象が未設定なら、シーン内のPlayerを自動で探す
	// (レベルエディタ配置など、外部からSetTarget()を呼ばれない経路のため)
	// ※ Init()の時点ではSceneManagerのシングルトン初期化(=GameScene::Init())が
	//    完了していない場合があり、ここでSceneManager::Instance()を呼ぶと
	//    自己再入でフリーズするため、Update()まで遅延させている
	if (m_wpTarget.expired())
	{
		if (std::shared_ptr<KdGameObject> spPlayer = SceneManager::Instance().FindObjectWithTag(ObjectTag::Player))
		{
			m_wpTarget = spPlayer;
		}
	}

	std::shared_ptr<KdGameObject> spTarget = m_wpTarget.lock();
	if (!spTarget) { return; }

	const float dt = Application::Instance().GetDeltaTime();

	Math::Vector3 pos       = GetPos();
	Math::Vector3 targetPos = spTarget->GetPos();

	// 対象への水平方向と距離
	Math::Vector3 toTarget = MathAPI::FlattenY(targetPos - pos);
	float distXZ = toTarget.Length();
	Math::Vector3 dirToTarget = MathAPI::GetSafeNormal(toTarget, Math::Vector3::Backward);

	// 対象の方を向く小関数(突進中は向きを固定したいので状態側で使い分ける)
	auto faceTarget = [&]()
	{
		float turnSpeedDeg = DebugParams::Instance().Float(U8("敵/旋回速度"), 180.0f, 0.0f, 720.0f);
		Math::Vector3 rot = GetRot();
		rot.y = MathAPI::RotateToDirection(rot.y, dirToTarget, turnSpeedDeg * dt);
		SetRot(rot);
	};

	switch (m_state)
	{
	case State::Chase:
	{
		// 攻撃開始距離まで近づいたら予備動作へ(その場で予告)
		float atkStart = DebugParams::Instance().Float(U8("敵/攻撃開始距離"), 2.5f, 0.5f, 20.0f);
		if (distXZ <= atkStart)
		{
			m_state = State::Windup;
			m_stateTimer = DebugParams::Instance().Float(U8("敵/予備動作時間"), 0.45f, 0.05f, 2.0f);
			break;
		}

		// 追従移動(対象へゆっくり近づく)
		float moveSpeed = DebugParams::Instance().Float(U8("敵/移動速度"), 1.5f, 0.0f, 20.0f);
		pos += dirToTarget * moveSpeed * dt;
		SetPos(pos);
		faceTarget();
		break;
	}
	case State::Windup:
	{
		// その場で予告(向きだけ対象へ合わせ続ける=どこへ突っ込むか読める)
		faceTarget();
		m_stateTimer -= dt;
		if (m_stateTimer <= 0.0f)
		{
			// 突進開始。方向をここで固定する(以後は追尾しないので回避で避けられる)
			m_state = State::Strike;
			m_stateTimer = DebugParams::Instance().Float(U8("敵/突進時間"), 0.22f, 0.05f, 1.0f);
			m_lungeDir = dirToTarget;
		}
		break;
	}
	case State::Strike:
	{
		// 固定方向へ高速で突っ込む
		float lungeSpeed = DebugParams::Instance().Float(U8("敵/突進速度"), 14.0f, 1.0f, 40.0f);
		pos += m_lungeDir * lungeSpeed * dt;
		SetPos(pos);
		m_stateTimer -= dt;

		// 命中判定：対象に十分近ければ命中処理(反撃 or ノックバック)して硬直へ
		float hitDist = m_hitRadius + 0.5f;   // プレイヤー半径ぶん少し余裕を持たせる
		if (Math::Vector3::Distance(GetPos(), targetPos) <= hitDist)
		{
			ResolveStrikeHit(spTarget);
			EnterRecover();
			break;
		}
		// 当たらずに突進時間が切れたら空振りで硬直へ
		if (m_stateTimer <= 0.0f)
		{
			EnterRecover();
		}
		break;
	}
	case State::Recover:
	{
		// 突進の後隙。硬直が明けたら追従に戻る
		m_stateTimer -= dt;
		if (m_stateTimer <= 0.0f)
		{
			m_state = State::Chase;
		}
		break;
	}
	}

	// 見た目：状態で色を変えて攻撃を予告する(黄=予備動作 / 明るい赤=突進)。
	// 【2026/07/31】通常時は白(＝色味を掛けない)。
	//   ここは基本色テクスチャへの【乗算】なので、赤を入れるとゴーレム全体が赤く染まって
	//   苔も割れ目の光も台無しになる。立方体だった頃は見分けるために赤くしていたが、
	//   テクスチャを持つモデルでは白のままにすること。
	//   予備動作と突進の色は攻撃の予告として機能するので残す
	switch (m_state)
	{
	case State::Windup: m_color = Math::Color(1.0f, 0.9f, 0.2f, 1.0f); break;
	case State::Strike: m_color = Math::Color(1.0f, 0.35f, 0.2f, 1.0f); break;
	default:            m_color = kWhiteColor; break;
	}
}

void Enemy::ResolveStrikeHit(const std::shared_ptr<KdGameObject>& _target)
{
	Player* pPlayer = dynamic_cast<Player*>(_target.get());
	if (!pPlayer) { return; }

	if (pPlayer->IsInvincible())
	{
		// ジャスト回避成立 → Player側に反撃(スロー窓)を通知する。
		// 自分は消滅させず空振り扱い(呼び出し側でEnterRecoverする)。プレイヤーはこの後の突撃で狩れる
		pPlayer->NotifyCounter();
	}
	else
	{
		// 無防備で被弾 → プレイヤーを外向きにノックバック(HPは無い=勢いを崩すだけ)
		Math::Vector3 knockDir = _target->GetPos() - GetPos();
		float power = DebugParams::Instance().Float(U8("敵/ノックバック力"), 8.0f, 0.0f, 40.0f);
		pPlayer->ApplyKnockback(knockDir, power);
	}
}

void Enemy::EnterRecover()
{
	m_state = State::Recover;
	m_stateTimer = DebugParams::Instance().Float(U8("敵/硬直時間"), 0.7f, 0.0f, 3.0f);
}

void Enemy::PostUpdate()
{
	// デバッグ表示：接触判定(m_hitRadius)を可視化
	if (KdGameObject::s_showColliderDebug && DebugDraw::IsOn(DebugDraw::Category::Enemy))
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugSphere(GetPos(), m_hitRadius, kRedColor);
	}

	// ※ 以前は「対象に接触したら敵が消滅」する仮処理だったが、攻撃(突進)に一本化したため撤去。
	//    敵はプレイヤーの攻撃(OnHit)か反撃でのみ消滅する

	// 地面(KdCollider::TypeGround)に立つ
	GroundCheck();
}
