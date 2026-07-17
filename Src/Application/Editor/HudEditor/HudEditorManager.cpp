#include "HudEditorManager.h"

#include "../../GameObject/UI/HudSprite.h"
#include "../../Utility/JsonManager.h"

HudEditorManager::~HudEditorManager() = default;

void HudEditorManager::Draw()
{
	ImGui::Begin(U8("HUDエディタ"));

	// スプライト追加
	if (ImGui::Button(U8("＋ スプライト追加")))
	{
		m_spSprites.push_back(std::make_shared<HudSprite>());
		m_selected = static_cast<int>(m_spSprites.size()) - 1;
	}

	// ファイル保存/読込
	ImGui::InputText(U8("ファイルパス"), &m_filePath);
	if (ImGui::Button(U8("Save")))
	{
		m_lastResultMessage = Save(m_filePath) ? U8("保存しました") : U8("保存に失敗しました");
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("Load")))
	{
		m_lastResultMessage = Load(m_filePath) ? U8("読み込みました") : U8("読み込めるファイルがありません");
	}
	if (!m_lastResultMessage.empty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", m_lastResultMessage.c_str());
	}

	ImGui::Separator();

	// 一覧(選択)
	for (int i = 0; i < static_cast<int>(m_spSprites.size()); ++i)
	{
		if (!m_spSprites[i]) { continue; }

		std::string label = std::to_string(i) + ": " + m_spSprites[i]->GetName();
		if (ImGui::Selectable(label.c_str(), m_selected == i))
		{
			m_selected = i;
		}
	}

	ImGui::Separator();

	// 選択中スプライトの編集
	if (m_selected >= 0 && m_selected < static_cast<int>(m_spSprites.size()) && m_spSprites[m_selected])
	{
		m_spSprites[m_selected]->DrawImGuiEdit();

		if (ImGui::Button(U8("この項目を削除")))
		{
			m_spSprites.erase(m_spSprites.begin() + m_selected);
			m_selected = -1;
		}
	}

	ImGui::End();
}

void HudEditorManager::DrawSprites() const
{
	if (m_spSprites.empty()) { return; }

	KdShaderManager::Instance().m_spriteShader.Begin();
	for (auto& sp : m_spSprites)
	{
		if (sp)
		{
			sp->Draw();
		}
	}
	KdShaderManager::Instance().m_spriteShader.End();
}

bool HudEditorManager::Save(const std::string& _filename)
{
	// 保存先フォルダが無ければ作成する
	nlohmann::json json = nlohmann::json::array();
	for (auto& sp : m_spSprites)
	{
		if (!sp) { continue; }
		json.push_back(sp->ToJson());
	}
	return JsonManager::Instance().Write(_filename, json);
}

bool HudEditorManager::Load(const std::string& _filename)
{
	nlohmann::json json;
	if (!JsonManager::Instance().Read(_filename, json)) { return false; }

	m_spSprites.clear();
	m_selected = -1;

	for (auto& j : json)
	{
		// 1要素ずつ保護(壊れた要素はスキップして残りは読む)
		try
		{
			auto sp = std::make_shared<HudSprite>();
			sp->FromJson(j);
			m_spSprites.push_back(sp);
		}
		catch (const nlohmann::json::exception&)
		{
			continue;
		}
	}

	return true;
}
