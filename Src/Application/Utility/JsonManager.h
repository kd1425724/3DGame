#pragma once

//====================================================
//
// JsonManager ── JSONファイルの読み書き(定型のファイルI/O)を1箇所にまとめるシングルトン
//
//  ・保存：親フォルダが無ければ作成し、整形(インデント4)してファイルへ書き出す
//  ・読込：ファイルを開いてJSONとしてパースする(パース失敗は握りつぶしてfalse)
//  ・「どんな中身を組み立てる/取り出すか」は各呼び出し側の責務。ここはファイル入出力だけを担う
//    (DebugParams / LevelFileIO / HudEditorManager が同じ定型を持っていたのを集約)
//
//  ※ nlohmann::json は Pch.h の強制インクルードで全ファイルから見えている
//  ※ シングルトンだがコンストラクタからInitは呼ばない(状態を持たないので初期化も不要)
//
//====================================================
class JsonManager
{
public:

	static JsonManager& Instance()
	{
		static JsonManager instance;
		return instance;
	}

	// _json を _path へ整形(インデント4)保存する。親フォルダが無ければ作る。成功でtrue。
	// (深い階層のフォルダが無いと ofstream が開けず黙って失敗するため、先に作成する)
	bool Write(const std::string& _path, const nlohmann::json& _json);

	// _path をJSONとしてパースし _out へ格納する。
	// 開けない/パース失敗なら false を返す(その場合 _out は変更しない)。
	// ※ 値の取り出し(get/at)での型不一致等は呼び出し側で try/catch すること
	bool Read(const std::string& _path, nlohmann::json& _out);

private:

	JsonManager() = default;
	~JsonManager() = default;
	JsonManager(const JsonManager&) = delete;
	void operator=(const JsonManager&) = delete;
};
