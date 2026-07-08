#include "Enemy.h"

#include "../../../main.h"
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"

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

	Math::Vector3 pos = GetPos();
	Math::Vector3 targetPos = spTarget->GetPos();

	// 対象へ水平方向にゆっくり近づく
	Math::Vector3 toTarget = targetPos - pos;
	toTarget.y = 0.0f;

	if (toTarget.LengthSquared() > 0.0f)
	{
		toTarget.Normalize();

		float moveSpeed = DebugParams::Instance().Float("敵/移動速度", 1.5f, 0.0f, 20.0f);
		pos += toTarget * moveSpeed * Application::Instance().GetDeltaTime();
		SetPos(pos);

		// 対象の方向を向く
		float turnSpeedDeg = DebugParams::Instance().Float("敵/旋回速度", 180.0f, 0.0f, 720.0f);
		Math::Vector3 rot = GetRot();
		rot.y = MathAPI::RotateToDirection(rot.y, toTarget, turnSpeedDeg * Application::Instance().GetDeltaTime());
		SetRot(rot);
	}
}

void Enemy::PostUpdate()
{
	// デバッグ表示：接触判定(m_hitRadius)を可視化
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
		m_pDebugWire->AddDebugSphere(GetPos(), m_hitRadius, kRedColor);
	}

	std::shared_ptr<KdGameObject> spTarget = m_wpTarget.lock();

	// 対象に接触したら消滅する
	if (spTarget && Math::Vector3::Distance(GetPos(), spTarget->GetPos()) < m_hitRadius)
	{
		m_isExpired = true;
		return;
	}

	// 地面(KdCollider::TypeGround)に立つ
	GroundCheck();
}
