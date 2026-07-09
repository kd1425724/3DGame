#include "LevelFileIO.h"

#include "../LevelEditorManager.h"
#include "../../Scene/SceneManager.h"

void LevelFileIO::Draw()
{
	ImGui::Text(U8("レベルデータ"));

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
}

bool LevelFileIO::Save(const std::string& filename)
{
	LevelEditorManager& mgr = LevelEditorManager::Instance();

	// 保存先のフォルダが無い場合は自動作成する
	std::filesystem::path path(filename);
	if (path.has_parent_path())
	{
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
	}

	std::ofstream ofs(filename);
	if (!ofs) { return false; }

	nlohmann::json json = nlohmann::json::array();

	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }

		// レベルエディタ経由で生成していないオブジェクト(種類名が分からない)は対象外
		std::string typeName = mgr.GetObjectTypeName(obj);
		if (typeName.empty()) { continue; }

		Math::Vector3 pos = obj->GetPos();
		Math::Vector3 rot = obj->GetRot();
		Math::Vector3 scale = obj->GetScale();

		nlohmann::json object;
		object["type"] = typeName;
		object["name"] = obj->GetName();
		object["pos"] = { pos.x, pos.y, pos.z };
		object["rot"] = { rot.x, rot.y, rot.z };
		object["scale"] = { scale.x, scale.y, scale.z };

		json.push_back(std::move(object));
	}

	ofs << json.dump(4);

	return true;
}

bool LevelFileIO::Load(const std::string& filename)
{
	LevelEditorManager& mgr = LevelEditorManager::Instance();

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

	for (auto& object : json)
	{
		// 1要素ずつ保護する。ある要素が型不一致・要素数不足で壊れていても、
		// その要素だけスキップして残りは読み込む(壊れた1個で全体が読めなくならない)
		try
		{
			if (!object.contains("type") || !object.contains("name")) { continue; }

			const std::string typeName = object["type"].get<std::string>();
			const std::string instanceName = object["name"].get<std::string>();

			// ※ typeNameが RegisterCreatable で登録されていない場合、
			//    Framework側のKdGameObjectFactoryがassertで止まります
			//   (未登録の種類がデータに紛れているという実装ミスを検知するためなので、
			//    データを直したうえで登録漏れが無いか確認してください)
			std::shared_ptr<KdGameObject> spObj = mgr.CreateObject(typeName);
			if (!spObj) { continue; }

			spObj->SetName(instanceName);

			//Math::Vector3に変換
			//ラムダ式をその場で作る
			//指定したkeyの３要素を取り出す
			auto readVec3 = [&object](const char* key) -> Math::Vector3
			{
				//keyが無ければ(0,0,0)を返す(欠けたデータでも落ちないように)
				if (!object.contains(key)) { return {}; }
				auto& v = object[key];

				//jsonにある値を一つずつ取り出す
				return { v.at(0).get<float>(), v.at(1).get<float>(), v.at(2).get<float>() };
			};

			spObj->SetPos(readVec3("pos"));
			spObj->SetRot(readVec3("rot"));
			spObj->SetScale(readVec3("scale"));
		}
		catch (const nlohmann::json::exception&)
		{
			// この要素は壊れているのでスキップして次へ
			continue;
		}
	}

	return true;
}
