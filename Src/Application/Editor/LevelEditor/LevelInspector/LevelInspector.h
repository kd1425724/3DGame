#pragma once

//====================================================
//
// レベルエディタ：インスペクターパネル
//  ・選択中オブジェクトの名前/座標/回転/スケールをその場で編集する
//
//====================================================
class LevelInspector
{
public:

	static LevelInspector& Instance()
	{
		static LevelInspector instance;
		return instance;
	}

	// ImGui描画
	void Draw();

private:

	LevelInspector() = default;
	~LevelInspector() = default;
	LevelInspector(const LevelInspector&) = delete;
	void operator=(const LevelInspector&) = delete;

	// 複数選択時：選択中全オブジェクトへの相対移動/相対回転/相対スケール・一括削除を行うパネル
	void DrawMultiSelect(const std::vector<std::shared_ptr<KdGameObject>>& selectedList);
};
