#include "CollisionGrid.h"

#include "IStaticCollider.h"
#include "../Scene/SceneManager.h"

void CollisionGrid::EnsureBuilt()
{
	if (!m_dirty) { return; }

	Rebuild();
	m_dirty = false;
}

void CollisionGrid::Rebuild()
{
	// Tracy計測(2026/07/19)：グリッド再構築。ゲーム中は静止していれば呼ばれないはずなので、
	// これが毎フレーム出ていたらMarkDirtyを呼び過ぎている(＝エディタ以外での無駄な再構築)
	ZoneScoped;

	m_cells.clear();
	m_always.clear();

	for (const std::shared_ptr<KdGameObject>& spObj : SceneManager::Instance().GetObjList())
	{
		if (!spObj) { continue; }

		// 静的コリジョンの印(IStaticCollider)を持つ物だけを対象にする
		IStaticCollider* pStatic = dynamic_cast<IStaticCollider*>(spObj.get());
		if (pStatic == nullptr) { continue; }

		const DirectX::BoundingSphere bs = pStatic->GetColliderBounds();
		if (bs.Radius <= 0.0f) { continue; }

		const float minX = bs.Center.x - bs.Radius;
		const float maxX = bs.Center.x + bs.Radius;
		const float minZ = bs.Center.z - bs.Radius;
		const float maxZ = bs.Center.z + bs.Radius;

		const int ix0 = CellIndex(minX);
		const int ix1 = CellIndex(maxX);
		const int iz0 = CellIndex(minZ);
		const int iz1 = CellIndex(maxZ);

		const int cellCount = (ix1 - ix0 + 1) * (iz1 - iz0 + 1);

		// 広すぎてセル分けに向かない物(地面/城壁等)は常時候補へ
		if (cellCount > m_bigCellCount)
		{
			m_always.push_back(spObj);
			continue;
		}

		for (int ix = ix0; ix <= ix1; ++ix)
		{
			for (int iz = iz0; iz <= iz1; ++iz)
			{
				m_cells[CellKey(ix, iz)].push_back(spObj);
			}
		}
	}
}

void CollisionGrid::CollectRange(float _minX, float _maxX, float _minZ, float _maxZ, std::vector<KdGameObject*>& _out)
{
	m_seen.clear();

	// 常時候補(地面など)は必ず入れる
	for (const std::weak_ptr<KdGameObject>& wp : m_always)
	{
		std::shared_ptr<KdGameObject> sp = wp.lock();
		if (!sp) { continue; }

		if (m_seen.insert(sp.get()).second)
		{
			_out.push_back(sp.get());
		}
	}

	// 範囲が重なるセルの物を集める(重複はseenで除去)
	const int ix0 = CellIndex(_minX);
	const int ix1 = CellIndex(_maxX);
	const int iz0 = CellIndex(_minZ);
	const int iz1 = CellIndex(_maxZ);

	for (int ix = ix0; ix <= ix1; ++ix)
	{
		for (int iz = iz0; iz <= iz1; ++iz)
		{
			auto it = m_cells.find(CellKey(ix, iz));
			if (it == m_cells.end()) { continue; }

			for (const std::weak_ptr<KdGameObject>& wp : it->second)
			{
				std::shared_ptr<KdGameObject> sp = wp.lock();
				if (!sp) { continue; }

				if (m_seen.insert(sp.get()).second)
				{
					_out.push_back(sp.get());
				}
			}
		}
	}
}

void CollisionGrid::QuerySphere(const Math::Vector3& _center, float _radius, std::vector<KdGameObject*>& _out)
{
	ZoneScoped;	// Tracy計測(2026/07/19)：broadphaseの球クエリ

	EnsureBuilt();
	CollectRange(_center.x - _radius, _center.x + _radius, _center.z - _radius, _center.z + _radius, _out);
}

void CollisionGrid::QueryRay(const Math::Vector3& _start, const Math::Vector3& _dir, float _length, std::vector<KdGameObject*>& _out)
{
	ZoneScoped;	// Tracy計測(2026/07/19)：broadphaseのレイクエリ

	EnsureBuilt();

	// レイ線分のXZ範囲(始点〜終点)を囲むAABBで、通過しうるセルをまとめて拾う
	const Math::Vector3 end = _start + _dir * _length;
	const float minX = std::min(_start.x, end.x);
	const float maxX = std::max(_start.x, end.x);
	const float minZ = std::min(_start.z, end.z);
	const float maxZ = std::max(_start.z, end.z);

	CollectRange(minX, maxX, minZ, maxZ, _out);
}
