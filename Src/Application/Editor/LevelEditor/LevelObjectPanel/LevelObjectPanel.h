#pragma once

//====================================================
//
// レベルエディタ：オブジェクト一覧パネル
//  ・配置可能な種類を選んで配置ボタンで生成
//  ・配置済みオブジェクトの一覧表示、クリックで選択、削除ボタンで削除
//
//====================================================
class LevelObjectPanel
{
public:

	static LevelObjectPanel& Instance()
	{
		static LevelObjectPanel instance;
		return instance;
	}

	// ImGui描画
	void Draw();

private:

	LevelObjectPanel() = default;
	~LevelObjectPanel() = default;
	LevelObjectPanel(const LevelObjectPanel&) = delete;
	void operator=(const LevelObjectPanel&) = delete;

	// 配置する種類のコンボボックスで、現在選ばれているindex
	int m_selectTypeIndex = 0;

	// 種類の絞り込み検索(StageProp登録で140件超になるため)
	ImGuiTextFilter m_typeFilter;
};
