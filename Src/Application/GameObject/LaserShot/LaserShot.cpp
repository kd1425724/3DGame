#include "LaserShot.h"

#include "../../main.h"
#include "../../Scene/SceneManager.h"
#include "../Chara/Enemy/Enemy.h"

void LaserShot::Fire(const Math::Vector3& _pos, const Math::Vector3& _dir)
{
	m_dir = _dir;
	m_dir.Normalize();

	// 当たり判定用に自身の座標を発射位置に置く(回転は判定(球)に不要なので平行移動のみ)
	SetPos(_pos);

	// エフェクトを発射位置・正面向きで再生する
	m_effectMatrix = Math::Matrix::CreateWorld(_pos, m_dir, Math::Vector3::Up);
	m_wpEffect = KdEffekseerManager::GetInstance().Play("Magic/BlueLaser/BlueLaser.efk", _pos);

	std::shared_ptr<KdEffekseerObject> spEffect = m_wpEffect.lock();
	if (spEffect)
	{
		spEffect->SetWorldMatrix(m_effectMatrix);
	}
}

void LaserShot::Update()
{
	// エフェクトの位置を毎フレーム適用し直す
	// (後半に遅れて生成されるノードにも位置が反映され、原点(0,0,0)に出るのを防ぐ)
	std::shared_ptr<KdEffekseerObject> spEffect = m_wpEffect.lock();
	if (spEffect)
	{
		spEffect->SetWorldMatrix(m_effectMatrix);
	}

	// 寿命を進め、時間が来たら停止して消滅する
	m_elapsed += Application::Instance().GetDeltaTime();
	if (m_elapsed >= m_lifeTime)
	{
		StopAndExpire();
	}
}

void LaserShot::PostUpdate()
{
	if (m_isExpired) { return; }

	// 攻撃判定：発射位置から前方m_hitOffsetの位置に半径m_hitRadiusの球を作り、敵に当てる
	Math::Vector3 attackCenter = GetPos() + m_dir * m_hitOffset;
	KdCollider::SphereInfo attackSphere(KdCollider::TypeDamage, attackCenter, m_hitRadius);

	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		std::shared_ptr<Enemy> spEnemy = std::dynamic_pointer_cast<Enemy>(obj);
		if (!spEnemy) { continue; }

		std::list<KdCollider::CollisionResult> results;
		if (spEnemy->Intersects(attackSphere, &results))
		{
			// 当たった相手に通知する(Enemy側で消滅などの処理を行う)
			spEnemy->OnHit(this);
		}
	}
}

void LaserShot::DrawDebug()
{
	// 攻撃判定の球を可視化する(当たり判定表示フラグが有効なときのみ)
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }

		Math::Vector3 attackCenter = GetPos() + m_dir * m_hitOffset;
		m_pDebugWire->AddDebugSphere(attackCenter, m_hitRadius, Math::Color(1.0f, 0.3f, 0.3f, 1.0f));
	}

	KdGameObject::DrawDebug();
}

void LaserShot::StopAndExpire()
{
	std::shared_ptr<KdEffekseerObject> spEffect = m_wpEffect.lock();
	if (spEffect)
	{
		spEffect->StopEffect();
	}

	m_isExpired = true;
}
