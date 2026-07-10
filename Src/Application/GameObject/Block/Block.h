#pragma once

// テスト用：Block.gltfを表示するだけの簡易オブジェクト
class Block : public KdGameObject
{
public:
	Block()				{}
	~Block()	override	{}

	void Init()				override;

	void SetAsset(const std::string& assetName) override;

	void DrawLit()			override;

private:

	// 表示・当たり判定の両方で共有するモデルワーク
	// (当たり判定側にshared_ptrとして渡して共有するためshared_ptrで保持。Groundと同じ形)
	std::shared_ptr<KdModelWork> m_spModelWork;
};
