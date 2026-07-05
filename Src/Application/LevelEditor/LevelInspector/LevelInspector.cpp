#include "LevelInspector.h"

#include "../LevelEditorManager.h"

void LevelInspector::Draw()
{
	ImGui::Text(U8("インスペクター"));

	std::shared_ptr<KdGameObject> obj = LevelEditorManager::Instance().GetSelected();

	if (!obj)
	{
		ImGui::TextDisabled(U8("( オブジェクトが選択されていません )"));
		return;
	}

	// 名前(imgui_stdlibのstd::string対応InputTextを使用)
	std::string name = obj->GetName();
	if (ImGui::InputText(U8("名前"), &name))
	{
		obj->SetName(name);
	}

	// 座標
	Math::Vector3 pos = obj->GetPos();
	if (ImGui::DragFloat3(U8("座標"), &pos.x, 0.1f))
	{
		obj->SetPos(pos);
	}

	// 回転(度数)
	Math::Vector3 rot = obj->GetRot();
	if (ImGui::DragFloat3(U8("回転"), &rot.x, 1.0f))
	{
		obj->SetRot(rot);
	}

	// スケール
	Math::Vector3 scale = obj->GetScale();
	if (ImGui::DragFloat3(U8("スケール"), &scale.x, 0.05f))
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
		LevelEditorManager::Instance().RemoveObject(obj);
	}
}
