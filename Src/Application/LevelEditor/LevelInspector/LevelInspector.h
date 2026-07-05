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
};
