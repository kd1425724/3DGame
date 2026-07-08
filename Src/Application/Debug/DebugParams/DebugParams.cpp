#include "DebugParams.h"

#include <filesystem>

#include "../DebugUtility/DebugUtility.h"

float& DebugParams::Float(const std::string& name, float defaultValue, float min, float max)
{
	auto itr = m_floats.find(name);
	if (itr == m_floats.end())
	{
		// Load() 済みの値があればそちらを優先する
		auto pendingItr = m_pendingFloats.find(name);
		if (pendingItr != m_pendingFloats.end())
		{
			defaultValue = pendingItr->second;
			m_pendingFloats.erase(pendingItr);
		}

		FloatParam param;
		param.value = defaultValue;
		param.min = min;
		param.max = max;

		itr = m_floats.emplace(name, param).first;
		m_registry[name] = ParamType::Float;
	}

	return itr->second.value;
}

int& DebugParams::Int(const std::string& name, int defaultValue, int min, int max)
{
	auto itr = m_ints.find(name);
	if (itr == m_ints.end())
	{
		auto pendingItr = m_pendingInts.find(name);
		if (pendingItr != m_pendingInts.end())
		{
			defaultValue = pendingItr->second;
			m_pendingInts.erase(pendingItr);
		}

		IntParam param;
		param.value = defaultValue;
		param.min = min;
		param.max = max;

		itr = m_ints.emplace(name, param).first;
		m_registry[name] = ParamType::Int;
	}

	return itr->second.value;
}

Math::Vector3& DebugParams::Vector3Param(const std::string& name, const Math::Vector3& defaultValue)
{
	auto itr = m_vectors.find(name);
	if (itr == m_vectors.end())
	{
		Math::Vector3 initial = defaultValue;

		auto pendingItr = m_pendingVectors.find(name);
		if (pendingItr != m_pendingVectors.end())
		{
			initial = pendingItr->second;
			m_pendingVectors.erase(pendingItr);
		}

		itr = m_vectors.emplace(name, initial).first;
		m_registry[name] = ParamType::Vector3;
	}

	return itr->second;
}

bool& DebugParams::Bool(const std::string& name, bool defaultValue)
{
	auto itr = m_bools.find(name);
	if (itr == m_bools.end())
	{
		auto pendingItr = m_pendingBools.find(name);
		if (pendingItr != m_pendingBools.end())
		{
			defaultValue = pendingItr->second;
			m_pendingBools.erase(pendingItr);
		}

		itr = m_bools.emplace(name, defaultValue).first;
		m_registry[name] = ParamType::Bool;
	}

	return itr->second;
}

void DebugParams::Draw()
{
	if (!ImGui::Begin(U8("DebugParams")))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button(U8("Save")))
	{
		m_lastResultMessage = Save() ? U8("保存しました") : U8("保存に失敗しました");
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("Load")))
	{
		m_lastResultMessage = Load() ? U8("読み込みました") : U8("読み込めるファイルがありません");
	}

	if (!m_lastResultMessage.empty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled(U8("%s"), m_lastResultMessage.c_str());
	}

	ImGui::Separator();

	if (m_registry.empty())
	{
		ImGui::TextDisabled(U8("( 登録されているパラメータはありません )"));
	}
	else
	{
		std::vector<std::string> names;
		names.reserve(m_registry.size());
		for (auto& keyValue : m_registry)
		{
			names.push_back(keyValue.first);
		}

		DebugUtil::DrawGroupedByCategory(names, [this](const std::string& fullName, const std::string& label)
		{
			switch (m_registry[fullName])
			{
			case ParamType::Float:
			{
				FloatParam& p = m_floats[fullName];
				if (p.min < p.max)
					ImGui::SliderFloat(label.c_str(), &p.value, p.min, p.max);
				else
					ImGui::DragFloat(label.c_str(), &p.value, 0.1f);
				break;
			}
			case ParamType::Int:
			{
				IntParam& p = m_ints[fullName];
				if (p.min < p.max)
					ImGui::SliderInt(label.c_str(), &p.value, p.min, p.max);
				else
					ImGui::DragInt(label.c_str(), &p.value);
				break;
			}
			case ParamType::Vector3:
				ImGui::DragFloat3(label.c_str(), &m_vectors[fullName].x, 0.1f);
				break;
			case ParamType::Bool:
				ImGui::Checkbox(label.c_str(), &m_bools[fullName]);
				break;
			}
		});
	}

	ImGui::End();
}

bool DebugParams::Save(const std::string& filename)
{
	// 保存先のフォルダが無い場合は自動作成する
	// (Asset/Data/Debug/DebugParams/ のように階層が深いフォルダは
	//  存在しないと ofstream が開けず、何も言わずに保存が失敗してしまうため)
	std::filesystem::path path(filename);
	if (path.has_parent_path())
	{
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
	}

	std::ofstream ofs(filename);
	if (!ofs) { return false; }

	nlohmann::json json;

	for (auto& [name, param] : m_floats)
	{
		json["floats"][name] = param.value;
	}
	for (auto& [name, param] : m_ints)
	{
		json["ints"][name] = param.value;
	}
	for (auto& [name, v] : m_vectors)
	{
		json["vectors"][name] = { v.x, v.y, v.z };
	}
	for (auto& [name, b] : m_bools)
	{
		json["bools"][name] = b;
	}

	ofs << json.dump(4);

	return true;
}

bool DebugParams::Load(const std::string& filename)
{
	std::ifstream ifs(filename);
	if (!ifs) { return false; }

	nlohmann::json json;
	try
	{
		ifs >> json;
	}
	catch (const nlohmann::json::parse_error&)
	{
		return false;
	}

	if (json.contains("floats"))
	{
		for (auto& [name, value] : json["floats"].items())
		{
			float v = value.get<float>();
			auto itr = m_floats.find(name);
			if (itr != m_floats.end()) { itr->second.value = v; }
			else { m_pendingFloats[name] = v; }
		}
	}

	if (json.contains("ints"))
	{
		for (auto& [name, value] : json["ints"].items())
		{
			int v = value.get<int>();
			auto itr = m_ints.find(name);
			if (itr != m_ints.end()) { itr->second.value = v; }
			else { m_pendingInts[name] = v; }
		}
	}

	if (json.contains("vectors"))
	{
		for (auto& [name, value] : json["vectors"].items())
		{
			Math::Vector3 v(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>());
			auto itr = m_vectors.find(name);
			if (itr != m_vectors.end()) { itr->second = v; }
			else { m_pendingVectors[name] = v; }
		}
	}

	if (json.contains("bools"))
	{
		for (auto& [name, value] : json["bools"].items())
		{
			bool v = value.get<bool>();
			auto itr = m_bools.find(name);
			if (itr != m_bools.end()) { itr->second = v; }
			else { m_pendingBools[name] = v; }
		}
	}

	return true;
}
