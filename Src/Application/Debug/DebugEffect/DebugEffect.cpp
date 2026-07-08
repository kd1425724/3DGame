#include "DebugEffect.h"

#include <filesystem>

void DebugEffect::Draw()
{
	if (!ImGui::Begin(U8("DebugEffect")))
	{
		ImGui::End();
		return;
	}

	// 初回だけ実フォルダを自動走査する(以降は「再読み込み」ボタンで更新)
	if (!m_fileListScanned) { RefreshFileList(); }

	ImGui::TextDisabled(U8("Asset/Data/Effect/ 内の .efk をプルダウンから選択(手入力も可)"));

	// 実フォルダから見つけた.efkをプルダウンで選ぶ(存在するものだけなのでassertを踏みにくい)
	const char* preview = m_fileName.empty() ? U8("(選択してください)") : m_fileName.c_str();
	if (ImGui::BeginCombo(U8("エフェクト"), preview))
	{
		for (const std::string& file : m_efkFiles)
		{
			bool selected = (file == m_fileName);
			if (ImGui::Selectable(file.c_str(), selected)) { m_fileName = file; }
			if (selected) { ImGui::SetItemDefaultFocus(); }
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("再読み込み"))) { RefreshFileList(); }

	if (m_efkFiles.empty())
	{
		ImGui::TextDisabled(U8("( %s に .efk が見つかりません )"), EffekseerPath);
	}

	// 手入力での指定も残す(存在しない名前を打つとDEBUGではassertで停止するので注意)
	ImGui::InputText(U8("ファイル名(手入力)"), &m_fileName);
	ImGui::DragFloat3(U8("座標"), &m_pos.x, 0.1f);
	ImGui::DragFloat(U8("サイズ"), &m_size, 0.05f, 0.01f, 100.0f);
	ImGui::DragFloat(U8("速度"), &m_speed, 0.05f, 0.0f, 10.0f);
	ImGui::Checkbox(U8("ループ再生"), &m_isLoop);

	if (ImGui::Button(U8("再生")))
	{
		if (m_fileName.empty())
		{
			m_lastResultMessage = U8("ファイル名が空です");
		}
		else
		{
			KdEffekseerManager::GetInstance().Play(m_fileName, m_pos, m_size, m_speed, m_isLoop);
			m_lastResultMessage = U8("再生しました: ") + m_fileName;
		}
	}

	ImGui::SameLine();

	if (ImGui::Button(U8("全停止")))
	{
		KdEffekseerManager::GetInstance().StopAllEffect();
		m_lastResultMessage = U8("全て停止しました");
	}

	if (!m_lastResultMessage.empty())
	{
		ImGui::TextDisabled(U8("%s"), m_lastResultMessage.c_str());
	}

	ImGui::End();
}

void DebugEffect::RefreshFileList()
{
	m_fileListScanned = true;
	m_efkFiles.clear();

	namespace fs = std::filesystem;
	std::error_code ec;

	// EffekseerPath = "Asset/Data/Effect/" (KdEffekseerManagerが定義)。
	// Play()には この基準からの相対パスを渡すので、走査結果も相対パスで持つ。
	const fs::path base(EffekseerPath);
	if (!fs::exists(base, ec)) { return; }

	for (const fs::directory_entry& entry : fs::recursive_directory_iterator(base, ec))
	{
		if (ec) { break; }
		if (!entry.is_regular_file()) { continue; }

		const fs::path& p = entry.path();
		const std::string ext = p.extension().string();
		if (ext != ".efk" && ext != ".EFK") { continue; }

		// 基準からの相対パス。generic_stringで区切りを'/'に統一(Playが期待する形)
		const std::string rel = fs::relative(p, base, ec).generic_string();
		if (!ec && !rel.empty()) { m_efkFiles.push_back(rel); }
	}

	// 名前順に並べて選びやすくする
	std::sort(m_efkFiles.begin(), m_efkFiles.end());
}
