#include "Enemy.h"

#include "../../../main.h"
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../Player/Player.h"   // 突進命中時にPlayerの無敵判定/反撃通知/ノックバックを呼ぶため

void Enemy::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 他のオブジェクトと見分けが付くように赤色にする
	m_color = kRedColor;

	// Playerと同じ比率で縮小
	SetScale(Math::Vector3(0.5f, 0.5f, 0.5f));

	// 攻撃を受ける側の当たり判定(球)を登録する
	// ※ 半径はローカル値。Intersects時にworld行列(このEnemyはscale0.5)で縮むため、
	//    world半径≒m_hitRadiusになるようローカル半径をscaleで割って指定する
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape(
		"EnemyDamage",
		Math::Vector3::Zero,
		m_hitRadius / 0.5f,
		KdCollider::TypeDamage
	);
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
	Math::Vector3 toTarget = targetPos - pos;
	toTarget.y = 0.0f;
	float distXZ = toTarget.Length();
	Math::Vector3 dirToTarget = (distXZ > 0.0001f) ? (toTarget / distXZ) : Math::Vector3::Backward;

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
		if (m_stateTimer <= 0.0f) { EnterRecover(); }
		break;
	}
	case State::Recover:
	{
		// 突進の後隙。硬直が明けたら追従に戻る
		m_stateTimer -= dt;
		if (m_stateTimer <= 0.0f) { m_state = State::Chase; }
		break;
	}
	}

	// 見た目：状態で色を変えて攻撃を予告する(黄=予備動作 / 明るい赤=突進 / 通常=赤)
	switch (m_state)
	{
	case State::Windup: m_color = Math::Color(1.0f, 0.9f, 0.2f, 1.0f); break;
	case State::Strike: m_color = Math::Color(1.0f, 0.35f, 0.2f, 1.0f); break;
	default:            m_color = kRedColor; break;
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
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
		m_pDebugWire->AddDebugSphere(GetPos(), m_hitRadius, kRedColor);
	}

	// ※ 以前は「対象に接触したら敵が消滅」する仮処理だったが、攻撃(突進)に一本化したため撤去。
	//    敵はプレイヤーの攻撃(OnHit)か反撃でのみ消滅する

	// 地面(KdCollider::TypeGround)に立つ
	GroundCheck();
}
