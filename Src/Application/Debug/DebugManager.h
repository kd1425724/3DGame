#pragma once

//====================================================
//
// デバッグ用ImGui機能をまとめる管理クラス
//
//  ・DebugFlags  … チェックボックスでフラグ管理(当たり判定の可視化ON/OFFなど)
//  ・DebugWatch  … オブジェクトの状態を数値として毎フレーム確認(座標など)
//  ・DebugParams … コードを書き換えずにその場でパラメータを調整
//
// セットアップ方法：
//  1. 毎フレームの更新開始時(Application::KdBeginUpdateなど)に
//       DebugManager::Instance().BeginFrame();
//     を呼び出す
//
//  2. ImGuiの ImGui::NewFrame() 〜 ImGui::Render() の間
//     (Framework/Utility/KdDebug/KdDebugGUI.cpp の GuiProcess() 内)で
//       DebugManager::Instance().Draw();
//     を呼び出す
//     ※ ImGuiはNewFrame〜Renderの間でしか描画を積めない都合上、
//       この1行だけはFramework側に追加が必要です
//
//====================================================
class DebugManager
{
public:

	static DebugManager& Instance()
	{
		static DebugManager instance;
		return instance;
	}

	// 毎フレームの更新開始時に呼び出す(DebugWatchの前フレーム情報クリアなど)
	void BeginFrame();

	// ImGuiのNewFrame~Renderの間で呼び出す(全デバッグウィンドウの描画)
	void Draw();

	// 全デバッグUIの表示ON/OFF(F3で一括トグル。falseだとメニューバー含め全て隠す)
	bool m_showAll = true;

	// 各ウィンドウの表示ON/OFF(上部メニューバーのチェックで切り替える)
	bool m_showFlags = true;
	bool m_showWatch = true;
	bool m_showParams = true;
	bool m_showEffect = true;
	bool m_showHud = true;

private:

	// 上部メニューバー(各ウィンドウのON/OFF + 一括操作)
	void DrawMenuBar();

	DebugManager() = default;
	~DebugManager() = default;
	DebugManager(const DebugManager&) = delete;
	void operator=(const DebugManager&) = delete;
};
