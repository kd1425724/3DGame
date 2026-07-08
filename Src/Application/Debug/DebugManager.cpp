#include "DebugManager.h"

#include "DebugFlags/DebugFlags.h"
#include "DebugWatch/DebugWatch.h"
#include "DebugParams/DebugParams.h"
#include "DebugEffect/DebugEffect.h"
#include "../LevelEditor/LevelEditorManager.h"

void DebugManager::BeginFrame()
{
	// F3で全デバッグUIの表示/非表示を一括切替(画面を素に戻して確認したいとき用)
	if (KdInputManager::Instance().IsPress("ToggleDebugGui"))
	{
		m_showAll = !m_showAll;
	}

	DebugWatch::Instance().BeginFrame();
}

void DebugManager::Draw()
{
	if (m_showAll)
	{
		// 上部メニューバー(各ウィンドウの個別ON/OFF)
		DrawMenuBar();

		if (m_showFlags)	{ DebugFlags::Instance().Draw(); }
		if (m_showWatch)	{ DebugWatch::Instance().Draw(); }
		if (m_showParams)	{ DebugParams::Instance().Draw(); }
		if (m_showEffect)	{ DebugEffect::Instance().Draw(); }
	}

	// レベルエディタ(配置ツール)は独自のF1トグルで制御(m_showAllとは独立)
	LevelEditorManager::Instance().Draw();
}

void DebugManager::DrawMenuBar()
{
	if (!ImGui::BeginMainMenuBar()) { return; }

	if (ImGui::BeginMenu(U8("デバッグ")))
	{
		// チェックで各ウィンドウの表示ON/OFF
		ImGui::MenuItem(U8("DebugFlags"), nullptr, &m_showFlags);
		ImGui::MenuItem(U8("DebugWatch"), nullptr, &m_showWatch);
		ImGui::MenuItem(U8("DebugParams"), nullptr, &m_showParams);
		ImGui::MenuItem(U8("DebugEffect"), nullptr, &m_showEffect);

		ImGui::Separator();

		if (ImGui::MenuItem(U8("すべて表示")))
		{
			m_showFlags = m_showWatch = m_showParams = m_showEffect = true;
		}
		if (ImGui::MenuItem(U8("すべて非表示")))
		{
			m_showFlags = m_showWatch = m_showParams = m_showEffect = false;
		}

		ImGui::EndMenu();
	}

	// 右側に操作ヒント
	ImGui::TextDisabled(U8("  (F3でデバッグUI全体の表示切替)"));

	ImGui::EndMainMenuBar();
}
