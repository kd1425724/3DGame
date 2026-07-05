#include "LevelObjectPanel.h"

#include "../LevelEditorManager.h"
#include "../LevelEditorHistory/LevelEditorHistory.h"
#include "../../Scene/SceneManager.h"

void LevelObjectPanel::Draw()
{
	LevelEditorManager& mgr = LevelEditorManager::Instance();

	//----------------------------------------
	// 配置可能なオブジェクトの選択＋配置ボタン
	//----------------------------------------
	ImGui::Text(U8("配置可能なオブジェクト"));

	const std::vector<std::string>& names = mgr.GetCreatableNames();

	if (names.empty())
	{
		ImGui::TextDisabled(U8("( RegisterCreatable で登録されたオブジェクトがありません )"));
	}
	else
	{
		if (m_selectTypeIndex >= static_cast<int>(names.size()))
		{
			m_selectTypeIndex = 0;
		}

		if (ImGui::BeginCombo(U8("種類"), names[m_selectTypeIndex].c_str()))
		{
			for (int i = 0; i < static_cast<int>(names.size()); i++)
			{
				bool isSelected = (i == m_selectTypeIndex);
				if (ImGui::Selectable(names[i].c_str(), isSelected))
				{
					m_selectTypeIndex = i;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();

		if (ImGui::Button(U8("配置")))
		{
			LevelEditorHistory::Instance().PushUndo();
			mgr.CreateObject(names[m_selectTypeIndex]);
		}
	}

	ImGui::Separator();

	//----------------------------------------
	// 配置済みオブジェクトの一覧
	//----------------------------------------
	ImGui::Text(U8("配置済みオブジェクト一覧"));

	ImGui::BeginChild("LevelObjectList", ImVec2(0, 200), true);

	std::shared_ptr<KdGameObject> selected = mgr.GetSelected();
	std::shared_ptr<KdGameObject> removeTarget;

	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }

		bool isSelected = (obj == selected);

		ImGui::PushID(obj.get());

		std::string label = obj->GetName().empty() ? U8("(名前無し)") : obj->GetName();

		if (ImGui::Selectable(label.c_str(), isSelected))
		{
			mgr.SetSelected(obj);
		}

		ImGui::SameLine();

		if (ImGui::SmallButton(U8("削除")))
		{
			removeTarget = obj;
		}

		ImGui::PopID();
	}

	ImGui::EndChild();

	// 描画ループの最中にリストを書き換えると壊れるので、ループの外で削除する
	if (removeTarget)
	{
		LevelEditorHistory::Instance().PushUndo();
		mgr.RemoveObject(removeTarget);
	}
}
