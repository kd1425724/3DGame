#include "LaserShot.h"

#include "../../../main.h"
#include "../../../Scene/SceneManager.h"
#include "../../Chara/Enemy/Enemy.h"

void LaserShot::Fire(const Math::Vector3& _pos, const Math::Vector3& _dir)
{
	m_dir = _dir;
	m_dir.Normalize();

	// 当たり判定用に自身の座標を発射位置に置く(回転は判定(球)に不要なので平行移動のみ)
	SetPos(_pos);

	// 発射位置・正面向きの行列を作っておく(魔法陣・レーザー両方でこの行列を使う)
	m_effectMatrix = Math::Matrix::CreateWorld(_pos, m_dir, Math::Vector3::Up);

	// まず魔法陣を表示してチャージ開始する(レーザーはm_chargeTime秒後に発射)
	m_wpMagicCircle = KdEffekseerManager::GetInstance().Play("Magic/MagicCircle/MagicCircle.efk", _pos);
	std::shared_ptr<KdEffekseerObject> spCircle = m_wpMagicCircle.lock();
	if (spCircle)
	{
		spCircle->SetWorldMatrix(m_effectMatrix);
	}

	m_phase = Phase::Charge;
	m_chargeElapsed = 0.0f;
}

void LaserShot::Update()
{
	// 魔法陣は発射中も出したままにするので、毎フレーム位置を適用し直す
	std::shared_ptr<KdEffekseerObject> spCircle = m_wpMagicCircle.lock();
	if (spCircle)
	{
		spCircle->SetWorldMatrix(m_effectMatrix);
	}

	if (m_phase == Phase::Charge)
	{
		// チャージ中：時間が来たら魔法陣の位置からレーザーを発射する
		m_chargeElapsed += Application::Instance().GetDeltaTime();
		if (m_chargeElapsed >= m_chargeTime)
		{
			// レーザーを再生(以降はMagicBaseが位置再適用+寿命管理を行う)
			PlayEffect("Magic/BlueLaser/BlueLaser.efk", m_effectMatrix);
			m_phase = Phase::Fire;
		}
		return;
	}

	// 発射中：MagicBaseがレーザーの位置再適用と寿命(m_lifeTime)管理を行う
	MagicBase::Update();
}

void LaserShot::PostUpdate()
{
	// レーザー発射中だけ攻撃判定を行う(チャージ中は当たらない)
	if (m_isExpired || m_phase != Phase::Fire) { return; }

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
	// 攻撃判定の球を可視化する(当たり判定表示フラグが有効かつレーザー発射中のみ)
	if (KdGameObject::s_showColliderDebug && m_phase == Phase::Fire)
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }

		Math::Vector3 attackCenter = GetPos() + m_dir * m_hitOffset;
		m_pDebugWire->AddDebugSphere(attackCenter, m_hitRadius, Math::Color(1.0f, 0.3f, 0.3f, 1.0f));
	}

	KdGameObject::DrawDebug();
}

void LaserShot::StopAndExpire()
{
	// 魔法陣も一緒に止める(レーザー本体の停止と消滅化はMagicBase側に任せる)
	std::shared_ptr<KdEffekseerObject> spCircle = m_wpMagicCircle.lock();
	if (spCircle)
	{
		spCircle->StopEffect();
	}

	MagicBase::StopAndExpire();
}
