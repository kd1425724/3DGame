#include "DebugWatch.h"

#include "../DebugUtility/DebugUtility.h"

void DebugWatch::Watch(const std::string& name, float value)
{
	Entry& e = m_watches[name];
	e.type = ValueType::Float;
	e.f = value;
}

void DebugWatch::Watch(const std::string& name, int value)
{
	Entry& e = m_watches[name];
	e.type = ValueType::Int;
	e.i = value;
}

void DebugWatch::Watch(const std::string& name, const Math::Vector3& value)
{
	Entry& e = m_watches[name];
	e.type = ValueType::Vector3;
	e.v = value;
}

void DebugWatch::Watch(const std::string& name, bool value)
{
	Entry& e = m_watches[name];
	e.type = ValueType::Bool;
	e.b = value;
}

void DebugWatch::Watch(const std::string& name, const std::string& value)
{
	Entry& e = m_watches[name];
	e.type = ValueType::String;
	e.s = value;
}

void DebugWatch::BeginFrame()
{
	// 前フレームの情報を全消去
	// → このフレームで Watch を呼ばなかった項目は表示から消える
	m_watches.clear();
}

void DebugWatch::Draw()
{
	if (!ImGui::Begin(U8("DebugWatch")))
	{
		ImGui::End();
		return;
	}

	if (m_watches.empty())
	{
		ImGui::TextDisabled(U8("( 監視中の値はありません )"));
	}
	else
	{
		std::vector<std::string> names;
		names.reserve(m_watches.size());
		for (auto& keyValue : m_watches)
		{
			names.push_back(keyValue.first);
		}

		DebugUtil::DrawGroupedByCategory(names, [this](const std::string& fullName, const std::string& label)
		{
			const Entry& e = m_watches[fullName];

			switch (e.type)
			{
			case ValueType::Float:
				ImGui::Text("%s : %.3f", label.c_str(), e.f);
				break;
			case ValueType::Int:
				ImGui::Text("%s : %d", label.c_str(), e.i);
				break;
			case ValueType::Vector3:
				ImGui::Text("%s : (%.3f, %.3f, %.3f)", label.c_str(), e.v.x, e.v.y, e.v.z);
				break;
			case ValueType::Bool:
				ImGui::Text("%s : %s", label.c_str(), e.b ? "true" : "false");
				break;
			case ValueType::String:
				ImGui::Text("%s : %s", label.c_str(), e.s.c_str());
				break;
			}
		});
	}

	ImGui::End();
}
