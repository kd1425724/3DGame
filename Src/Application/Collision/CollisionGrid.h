#pragma once

//====================================================
//
// CollisionGrid ── 静的な世界コリジョンの簡易broadphase(XZ平面の均一グリッド)
//
//  【なぜ要るか】
//   キャラ(プレイヤー＋敵多数)は毎フレーム 接地レイ/壁球判定/スイープ を行う。
//   従来は判定のたびにシーンの全オブジェクトを総当たり(O(N))していたため、建物を大量に
//   置くとキャラ数×クエリ数×建物数でCPUが破綻する。グリッドに空間登録しておけば、
//   「クエリ範囲が重なるセルにいる建物」だけに絞れる(≒O(1))。数千棟でも当たりが破綻しない土台。
//
//  【設計(将来を見越して)】
//   ・内部構造(均一グリッド)はこのクラス内に隠蔽。呼び出し側はQuerySphere/QueryRayだけ見るので、
//     将来もっと賢い構造(BVH/八分木)へ差し替えても呼び出しコードは変えなくてよい。
//   ・対象は IStaticCollider を実装した物だけ(建物/地面/ブロック)。動く物(敵/プレイヤー)は載せない。
//   ・地面や城壁のように広くて多数のセルにまたがる物は「常時候補」バケットに入れ、毎クエリで必ず返す
//     (数千セルへ登録する無駄と、巨大物の取りこぼしを同時に防ぐ)。
//   ・建物の追加/削除/移動やシーン切替では MarkDirty() を呼ぶ。次のクエリで作り直す。
//     ゲーム中(建物は静止)は一度作れば作り直さないのでクエリはO(1)。
//   ・Framework非依存(Application内で完結)。
//
//====================================================
class KdGameObject;

class CollisionGrid
{
public:

	static CollisionGrid& Instance()
	{
		static CollisionGrid instance;
		return instance;
	}

	// 建物の追加/削除/移動・シーン切替時に呼ぶ。次のクエリでグリッドを作り直す
	void MarkDirty() { m_dirty = true; }

	// 球(center,radius)の近くにある静的コリジョン候補を out に集める(重複なし)
	void QuerySphere(const Math::Vector3& _center, float _radius, std::vector<KdGameObject*>& _out);

	// レイ(start から dir 方向へ length)の近くにある静的コリジョン候補を out に集める(重複なし)
	void QueryRay(const Math::Vector3& _start, const Math::Vector3& _dir, float _length, std::vector<KdGameObject*>& _out);

private:

	CollisionGrid() = default;
	~CollisionGrid() = default;
	CollisionGrid(const CollisionGrid&) = delete;
	void operator=(const CollisionGrid&) = delete;

	// dirtyなら SceneManager の現在のシーンから作り直す
	void EnsureBuilt();
	void Rebuild();

	// XZのAABB[minX,maxX]x[minZ,maxZ]が重なる全セル＋常時候補を out へ(重複は seen で除去)
	void CollectRange(float _minX, float _maxX, float _minZ, float _maxZ, std::vector<KdGameObject*>& _out);

	// セル座標→キー(long long)にパックする
	long long CellKey(int _ix, int _iz) const
	{
		return (static_cast<long long>(_ix) << 32) ^ (static_cast<unsigned int>(_iz));
	}

	// セル番号(負の座標でも正しく切り下げる。std::floor相当を<cmath>無しで)
	int CellIndex(float _v) const
	{
		const float d = _v / m_cellSize;
		int i = static_cast<int>(d);
		if (d < 0.0f && static_cast<float>(i) != d) { i -= 1; }
		return i;
	}

	// 1セルの大きさ(m)。建物1棟がだいたい1〜数セルに収まる値
	float m_cellSize = 12.0f;

	// これより多くのセルにまたがる物(地面/城壁等)は常時候補へ回す
	int m_bigCellCount = 16;

	// セル→そこに footprint が重なる静的コリジョン(weak_ptr。消えた物は自動で無効化)
	std::unordered_map<long long, std::vector<std::weak_ptr<KdGameObject>>> m_cells;

	// 広すぎてセル分けに向かない物(毎クエリで必ず候補に含める)
	std::vector<std::weak_ptr<KdGameObject>> m_always;

	// 重複除去用(クエリ毎にclearして使い回す。確保を毎回やらないためメンバに持つ)
	std::unordered_set<KdGameObject*> m_seen;

	bool m_dirty = true;
};
