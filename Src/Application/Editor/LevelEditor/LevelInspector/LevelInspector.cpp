#include "LevelInspector.h"

#include "../LevelEditorManager.h"
#include "../LevelEditorHistory/LevelEditorHistory.h"

void LevelInspector::Draw()
{
	ImGui::Text(U8("インスペクター"));

	std::vector<std::shared_ptr<KdGameObject>> selectedList = LevelEditorManager::Instance().GetSelectedList();

	if (selectedList.empty())
	{
		ImGui::TextDisabled(U8("( オブジェクトが選択されていません )"));
		return;
	}

	// 複数選択中は専用パネルへ切り替える
	if (selectedList.size() > 1)
	{
		DrawMultiSelect(selectedList);
		return;
	}

	std::shared_ptr<KdGameObject> obj = selectedList.front();

	// 名前(imgui_stdlibのstd::string対応InputTextを使用)
	std::string name = obj->GetName();
	bool nameChanged = ImGui::InputText(U8("名前"), &name);
	if (ImGui::IsItemActivated())
	{
		LevelEditorHistory::Instance().PushUndo();
	}
	if (nameChanged)
	{
		obj->SetName(name);
	}

	// 座標
	Math::Vector3 pos = obj->GetPos();
	bool posChanged = ImGui::DragFloat3(U8("座標"), &pos.x, 0.1f);
	if (ImGui::IsItemActivated())
	{
		LevelEditorHistory::Instance().PushUndo();
	}
	if (posChanged)
	{
		obj->SetPos(LevelEditorManager::Instance().SnapPos(pos));
	}

	// 回転(度数)
	Math::Vector3 rot = obj->GetRot();
	bool rotChanged = ImGui::DragFloat3(U8("回転"), &rot.x, 1.0f);
	if (ImGui::IsItemActivated())
	{
		LevelEditorHistory::Instance().PushUndo();
	}
	if (rotChanged)
	{
		obj->SetRot(LevelEditorManager::Instance().SnapRot(rot));
	}

	// スケール
	Math::Vector3 scale = obj->GetScale();
	bool scaleChanged = ImGui::DragFloat3(U8("スケール"), &scale.x, 0.05f);
	if (ImGui::IsItemActivated())
	{
		LevelEditorHistory::Instance().PushUndo();
	}
	if (scaleChanged)
	{
		obj->SetScale(scale);
	}

	ImGui::Separator();

	// 3D画面上のハイライト表示(黄色いボックス)のサイズ調整
	// ※ オブジェクトの実際の大きさに応じて見た目を合わせたい場合に使う
	ImGui::DragFloat3(U8("ハイライト表示サイズ"), &LevelEditorManager::Instance().m_highlightBoxExtents.x, 0.05f, 0.05f, 10.0f);

	// 移動ギズモ(赤緑青のハンドル)の長さ調整
	ImGui::DragFloat(U8("ギズモの長さ"), &LevelEditorManager::Instance().m_gizmoLength, 0.1f, 0.2f, 20.0f);

	ImGui::Spacing();

	if (ImGui::Button(U8("複製 (Ctrl+D)")))
	{
		LevelEditorManager::Instance().DuplicateSelected();
	}

	ImGui::SameLine();

	if (ImGui::Button(U8("選択解除")))
	{
		LevelEditorManager::Instance().SetSelected(nullptr);
	}

	ImGui::SameLine();

	if (ImGui::Button(U8("このオブジェクトを削除")))
	{
		LevelEditorHistory::Instance().PushUndo();
		LevelEditorManager::Instance().RemoveObject(obj);
	}
}

void LevelInspector::DrawMultiSelect(const std::vector<std::shared_ptr<KdGameObject>>& selectedList)
{
	LevelEditorManager& mgr = LevelEditorManager::Instance();

	ImGui::Text(U8("選択中：%d個"), static_cast<int>(selectedList.size()));
	ImGui::TextDisabled(U8("ドラッグした分だけ選択中の全オブジェクトに反映されます"));

	// 移動(相対)：ドラッグした量をそのまま選択中の全オブジェクトに加算する
	Math::Vector3 moveDelta = Math::Vector3::Zero;
	bool moveChanged = ImGui::DragFloat3(U8("移動(相対)"), &moveDelta.x, 0.05f);
	if (ImGui::IsItemActivated())
	{
		LevelEditorHistory::Instance().PushUndo();
	}
	if (moveChanged)
	{
		for (auto& obj : selectedList)
		{
			obj->SetPos(obj->GetPos() + moveDelta);
		}
	}

	// 回転(相対・度数)
	Math::Vector3 rotDelta = Math::Vector3::Zero;
	bool rotChanged = ImGui::DragFloat3(U8("回転(相対)"), &rotDelta.x, 0.5f);
	if (ImGui::IsItemActivated())
	{
		LevelEditorHistory::Instance().PushUndo();
	}
	if (rotChanged)
	{
		for (auto& obj : selectedList)
		{
			obj->SetRot(obj->GetRot() + rotDelta);
		}
	}

	// スケール(相対倍率)：1.0を基準にドラッグした比率を選択中の全オブジェクトの現在のスケールに掛ける
	float scaleFactor = 1.0f;
	bool scaleChanged = ImGui::DragFloat(U8("スケール(相対倍率)"), &scaleFactor, 0.01f, 0.01f, 10.0f);
	if (ImGui::IsItemActivated())
	{
		LevelEditorHistory::Instance().PushUndo();
	}
	if (scaleChanged)
	{
		for (auto& obj : selectedList)
		{
			obj->SetScale(obj->GetScale() * scaleFactor);
		}
	}

	ImGui::Spacing();

	if (ImGui::Button(U8("選択中を全て削除")))
	{
		LevelEditorHistory::Instance().PushUndo();
		mgr.RemoveSelectedObjects();
	}

	ImGui::SameLine();

	if (ImGui::Button(U8("選択解除")))
	{
		mgr.ClearSelection();
	}
}
