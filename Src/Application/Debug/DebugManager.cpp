#include "DebugManager.h"

#include "DebugFlags/DebugFlags.h"
#include "DebugWatch/DebugWatch.h"
#include "DebugParams/DebugParams.h"
#include "DebugEffect/DebugEffect.h"
#include "../LevelEditor/LevelEditorManager.h"

void DebugManager::BeginFrame()
{
	DebugWatch::Instance().BeginFrame();
}

void DebugManager::Draw()
{
	if (m_showFlags)	{ DebugFlags::Instance().Draw(); }
	if (m_showWatch)	{ DebugWatch::Instance().Draw(); }
	if (m_showParams)	{ DebugParams::Instance().Draw(); }
	if (m_showEffect)	{ DebugEffect::Instance().Draw(); }

	// レベルエディタ(配置ツール)
	LevelEditorManager::Instance().Draw();
}
