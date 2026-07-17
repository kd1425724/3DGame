#pragma once

#include "../../Collision/IStaticCollider.h"   // 静的コリジョン=CollisionGridに載る印(継承するのでinclude)

// テスト用：Block.gltfを表示するだけの簡易オブジェクト
class Block : public KdGameObject, public IStaticCollider
{
public:
	Block()				{}
	~Block()	override	{}

	void Init()				override;

	void SetAsset(const std::string& assetName) override;

	void DrawLit()			override;

	void GenerateDepthMapFromLight() override;   // 影を落とす側：深度マップへモデルを描く

	void DrawDebug()		override;   // 当たり判定(モデル立方体)を箱で可視化

	// IStaticCollider: CollisionGridに載せる粗い範囲(描画カリングと同じ境界球を流用)
	DirectX::BoundingSphere GetColliderBounds() const override
	{
		return const_cast<Block*>(this)->WorldBoundingSphere();
	}

private:

	// カリング用のワールド境界球を返す(初回に一度モデルからローカル球を計算してキャッシュ)
	DirectX::BoundingSphere WorldBoundingSphere();

	// 表示・当たり判定の両方で共有するモデルワーク
	// (当たり判定側にshared_ptrとして渡して共有するためshared_ptrで保持。Groundと同じ形)
	std::shared_ptr<KdModelWork> m_spModelWork;

	// カリング用のモデル全体のローカル境界球(半径0=未計算。WorldBoundingSphereで遅延計算)
	DirectX::BoundingSphere m_localBS = { { 0.0f, 0.0f, 0.0f }, 0.0f };
};
