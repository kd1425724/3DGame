#pragma once

//====================================================
//
// デバッグ用：Effekseerエフェクトのプレビュー再生パネル
//  ・ファイル名(Asset/Data/Effect/からの相対パス)と座標を指定して再生/全停止
//  ・DEBUG構成では存在しないファイル名を指定するとassertで停止する仕様のため注意
//
//====================================================
class DebugEffect
{
public:

	static DebugEffect& Instance()
	{
		static DebugEffect instance;
		return instance;
	}

	// ImGui描画
	void Draw();

private:

	DebugEffect() = default;
	~DebugEffect() = default;
	DebugEffect(const DebugEffect&) = delete;
	void operator=(const DebugEffect&) = delete;

	// Asset/Data/Effect/ を再帰走査して m_efkFiles を作り直す
	void RefreshFileList();

	// 再生するエフェクトファイル名(Asset/Data/Effect/からの相対パス)
	std::string m_fileName;

	// 実フォルダを走査して見つけた.efkの相対パス一覧(プルダウン選択用)
	std::vector<std::string> m_efkFiles;

	// 初回Draw時に一度だけ自動走査するためのフラグ
	bool m_fileListScanned = false;

	// 再生パラメータ
	Math::Vector3	m_pos;
	float			m_size = 1.0f;
	float			m_speed = 1.0f;
	bool			m_isLoop = false;

	// 直近の操作結果メッセージ
	std::string m_lastResultMessage;
};
