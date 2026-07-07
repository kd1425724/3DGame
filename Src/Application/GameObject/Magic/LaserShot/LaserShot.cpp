#include "LaserShot.h"

#include "../../../Scene/SceneManager.h"
#include "../../Chara/Enemy/Enemy.h"

void LaserShot::Fire(const Math::Vector3& _pos, const Math::Vector3& _dir)
{
	m_dir = _dir;
	m_dir.Normalize();

	// 当たり判定用に自身の座標を発射位置に置く(回転は判定(球)に不要なので平行移動のみ)
	SetPos(_pos);

	// エフェクトを発射位置・正面向きで再生する(保持・毎フレーム適用はMagicBaseが行う)
	PlayEffect("Magic/BlueLaser/BlueLaser.efk", Math::Matrix::CreateWorld(_pos, m_dir, Math::Vector3::Up));
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

		// 当たったかどうか(bool)だけ分かればよいので詳細リザルトは不要(nullptrを渡す)
		if (spEnemy->Intersects(attackSphere, nullptr))
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
