#pragma once

//====================================================
//
// レベルエディタ：配置データの保存/読込パネル
//  ・DebugParamsと同じ安全策を採用
//    (フォルダが無ければ自動作成、ファイルが無くても落ちない)
//  ・保存形式：種類名,インスタンス名,PosX,PosY,PosZ,RotX,RotY,RotZ,ScaleX,ScaleY,ScaleZ
//
//====================================================
class LevelFileIO
{
public:

	static LevelFileIO& Instance()
	{
		static LevelFileIO instance;
		return instance;
	}

	// ImGui描画(ファイルパス入力欄＋Save/Loadボタン)
	void Draw();

	// 現在の配置データをファイルに保存(フォルダが無ければ自動作成する)
	bool Save(const std::string& filename);

	// ファイルから配置データを読み込む(既存の配置には追加される。全消しはしない)
	bool Load(const std::string& filename);

private:

	LevelFileIO() = default;
	~LevelFileIO() = default;
	LevelFileIO(const LevelFileIO&) = delete;
	void operator=(const LevelFileIO&) = delete;

	// ImGui上で編集するファイルパス
	std::string m_filePath = "Asset/Data/LevelEditor/Level.csv";

	// Save/Loadボタンの結果表示用
	std::string m_lastResultMessage;
};
