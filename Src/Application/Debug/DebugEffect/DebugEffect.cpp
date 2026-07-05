#include "DebugEffect.h"

void DebugEffect::Draw()
{
	if (!ImGui::Begin(U8("DebugEffect")))
	{
		ImGui::End();
		return;
	}

	ImGui::TextDisabled(U8("Asset/Data/Effect/ からの相対パスを指定(例: Test.efk)"));
	ImGui::TextDisabled(U8("※ 存在しないファイル名を指定するとDEBUG構成ではassertで停止します"));

	ImGui::InputText(U8("ファイル名"), &m_fileName);
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
		ImGui::TextDisabled("%s", m_lastResultMessage.c_str());
	}

	ImGui::End();
}
