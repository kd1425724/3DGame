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

	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }

		// レベルエディタ経由で生成していないオブジェクト(種類名が分からない)は対象外
		std::string typeName = mgr.GetObjectTypeName(obj);
		if (typeName.empty()) { continue; }

		Math::Vector3 pos = obj->GetPos();
		Math::Vector3 rot = obj->GetRot();
		Math::Vector3 scale = obj->GetScale();

		ofs << typeName << "," << obj->GetName() << ","
			<< pos.x << "," << pos.y << "," << pos.z << ","
			<< rot.x << "," << rot.y << "," << rot.z << ","
			<< scale.x << "," << scale.y << "," << scale.z << "\n";
	}

	return true;
}

bool LevelFileIO::Load(const std::string& filename)
{
	LevelEditorManager& mgr = LevelEditorManager::Instance();

	// KdCSVDataはファイルが無いとassertで止まる仕様なので、事前にチェックする
	{
		std::ifstream existsCheck(filename);
		if (!existsCheck) { return false; }
	}

	// プロジェクト標準のcsv読み込みクラスを使用
	KdCSVData csv(filename);

	for (size_t lineIdx = 0; lineIdx < csv.GetLineSize(); lineIdx++)
	{
		const std::vector<std::string>& cols = csv.GetLine(lineIdx);

		// 種類名,インスタンス名,Pos*3,Rot*3,Scale*3 の11列
		if (cols.size() < 11) { continue; }

		const std::string& typeName = cols[0];
		const std::string& instanceName = cols[1];

		// ※ typeNameが RegisterCreatable で登録されていない場合、
		//    Framework側のKdGameObjectFactoryがassertで止まります
		//   (未登録の種類がデータに紛れているという実装ミスを検知するためなので、
		//    データを直したうえで登録漏れが無いか確認してください)
		std::shared_ptr<KdGameObject> spObj = mgr.CreateObject(typeName);
		if (!spObj) { continue; }

		spObj->SetName(instanceName);

		Math::Vector3 pos(std::stof(cols[2]), std::stof(cols[3]), std::stof(cols[4]));
		Math::Vector3 rot(std::stof(cols[5]), std::stof(cols[6]), std::stof(cols[7]));
		Math::Vector3 scale(std::stof(cols[8]), std::stof(cols[9]), std::stof(cols[10]));

		spObj->SetPos(pos);
		spObj->SetRot(rot);
		spObj->SetScale(scale);
	}

	return true;
}
