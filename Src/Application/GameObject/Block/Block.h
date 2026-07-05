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

	// 表示用モデルワーク
	KdModelWork m_modelWork;
};
