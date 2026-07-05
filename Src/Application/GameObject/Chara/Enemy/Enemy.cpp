#include "Enemy.h"

#include "../../../main.h"
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Scene/SceneManager.h"
#include "../Player/Player.h"

void Enemy::Init()
{
	SetAsset("Asset/Models/Test/Block/Block.gltf");

	// 他のオブジェクトと見分けが付くように赤色にする
	m_color = kRedColor;

	// Playerと同じ比率で縮小
	SetScale(Math::Vector3(0.5f, 0.5f, 0.5f));
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
		for (auto& obj : SceneManager::Instance().GetObjList())
		{
			std::shared_ptr<Player> spPlayer = std::dynamic_pointer_cast<Player>(obj);
			if (spPlayer)
			{
				m_wpTarget = spPlayer;
				break;
			}
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

		pos += toTarget * m_moveSpeed * Application::Instance().GetDeltaTime();
		SetPos(pos);

		// 対象の方向を向く
		Math::Vector3 rot = GetRot();
		rot.y = MathAPI::RotateToDirection(rot.y, toTarget, m_turnSpeedDeg * Application::Instance().GetDeltaTime());
		SetRot(rot);
	}
}

void Enemy::PostUpdate()
{
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
