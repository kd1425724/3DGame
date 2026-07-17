#pragma once

#include "../../Collision/IStaticCollider.h"   // 静的コリジョン=CollisionGridに載る印(継承するのでinclude)

//====================================================
//
// テスト用：Block.gltfを大きく・薄く潰して地面代わりに使う簡易GameObject
//  ・モデル形状そのものを当たり判定(KdCollider::TypeGround)として登録する
//
//====================================================
class Ground : public KdGameObject, public IStaticCollider
{
public:

	Ground()				{}
	~Ground()	override	{}

	void Init()				override;

	void SetAsset(const std::string& assetName) override;

	void DrawLit()			override;

	void GenerateDepthMapFromLight() override;   // 影を落とす側：深度マップへモデルを描く

	void DrawDebug()		override;   // 当たり判定(モデル立方体)を箱で可視化

	// IStaticCollider: 地面は広大なのでCollisionGridでは「常時候補」に回る(下のGetColliderBounds参照)
	DirectX::BoundingSphere GetColliderBounds() const override;

private:

	// 表示・当たり判定の両方で共有するモデルワーク
	// (当たり判定側にshared_ptrとして渡して共有する必要があるためshared_ptrで保持)
	std::shared_ptr<KdModelWork> m_spModelWork;
};
