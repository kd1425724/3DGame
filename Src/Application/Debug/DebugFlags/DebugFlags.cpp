#include "DebugFlags.h"

#include "../DebugUtility/DebugUtility.h"

void DebugFlags::Draw()
{
	if (!ImGui::Begin(U8("DebugFlags")))
	{
		ImGui::End();
		return;
	}

	if (m_flags.empty())
	{
		ImGui::TextDisabled(U8("( 登録されているフラグはありません )"));
	}
	else
	{
		std::vector<std::string> names;
		names.reserve(m_flags.size());
		for (auto& keyValue : m_flags)
		{
			names.push_back(keyValue.first);
		}

		DebugUtil::DrawGroupedByCategory(names, [this](const std::string& fullName, const std::string& label)
		{
			// 名前の頭の「数字_」は並び順を決めるための印なので表示からは落とす
			ImGui::Checkbox(DebugUtil::StripOrderPrefix(label).c_str(), &m_flags[fullName]);
		});
	}

	ImGui::End();
}
