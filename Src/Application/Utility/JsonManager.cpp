#include "JsonManager.h"

#include <fstream>
#include <filesystem>

bool JsonManager::Write(const std::string& _path, const nlohmann::json& _json)
{
	// 保存先の親フォルダが無ければ作成する
	// (Asset/Data/… のように階層が深いフォルダは、存在しないと ofstream が
	//  開けず、何も言わずに保存が失敗してしまうため、先に作っておく)
	std::filesystem::path path(_path);
	if (path.has_parent_path())
	{
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
	}

	std::ofstream ofs(_path);
	if (!ofs) { return false; }

	ofs << _json.dump(4);

	return true;
}

bool JsonManager::Read(const std::string& _path, nlohmann::json& _out)
{
	std::ifstream ifs(_path);
	if (!ifs) { return false; }

	// パース失敗(壊れた/空のファイル等)は握りつぶして false を返す。
	// 値の取り出し(get/at)での型不一致等は各呼び出し側で try/catch すること
	try
	{
		ifs >> _out;
	}
	catch (const nlohmann::json::parse_error&)
	{
		return false;
	}

	return true;
}
