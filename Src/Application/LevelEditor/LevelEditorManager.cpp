#include "LevelEditorManager.h"

#include "../Scene/SceneManager.h"
#include "../GameObject/Camera/EditorCamera/EditorCamera.h"
#include "LevelObjectPanel/LevelObjectPanel.h"
#include "LevelInspector/LevelInspector.h"
#include "LevelFileIO/LevelFileIO.h"
#include "LevelPicker/LevelPicker.h"

std::shared_ptr<KdGameObject> LevelEditorManager::CreateObject(const std::string_view objTypeName)
{
	std::shared_ptr<KdGameObject> spObj = m_factory.CreateGameObject(objTypeName);

	if (spObj)
	{
		// デフォルトの名前として種類名を入れておく(Inspectorで後から変更できる)
		spObj->SetName(std::string(objTypeName));

		// 現在のシーンに追加(SceneManagerが「現在のシーン」への追加を委譲してくれる)
		SceneManager::Instance().AddObject(spObj);

		m_objectTypeNames[spObj.get()] = std::string(objTypeName);

		m_wpSelected = spObj;
	}

	return spObj;
}

void LevelEditorManager::RemoveObject(const std::shared_ptr<KdGameObject>& obj)
{
	if (!obj) { return; }

	SceneManager::Instance().RemoveObject(obj);

	m_objectTypeNames.erase(obj.get());

	if (GetSelected() == obj)
	{
		m_wpSelected.reset();
	}
}

namespace
{
	// キーが「今フレームで押された瞬間」かどうかを判定する(押しっぱなしで連続発火しないように)
	bool IsKeyTriggered(int vk, bool& prevState)
	{
		bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		bool triggered = (down && !prevState);
		prevState = down;
		return triggered;
	}
}

void LevelEditorManager::Update()
{
	LevelPicker::Instance().Update();

	// キー入力の判定(ImGuiのテキスト入力中などはショートカットを無効化する)
	bool cTriggered = IsKeyTriggered('C', m_prevKeyC);
	bool vTriggered = IsKeyTriggered('V', m_prevKeyV);
	bool dTriggered = IsKeyTriggered('D', m_prevKeyD);

	if (m_enabled && !ImGui::GetIO().WantCaptureKeyboard)
	{
		bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

		if (ctrl)
		{
			if (cTriggered) { CopySelected(); }
			if (vTriggered) { Paste(); }
			if (dTriggered) { DuplicateSelected(); }
		}
	}
}

void LevelEditorManager::CopySelected()
{
	std::shared_ptr<KdGameObject> obj = GetSelected();
	if (!obj) { return; }

	// エディタ経由で生成していない(種類名が分からない)オブジェクトは複製できない
	std::string typeName = GetObjectTypeName(obj);
	if (typeName.empty()) { return; }

	m_clipboard.hasData = true;
	m_clipboard.typeName = typeName;
	m_clipboard.name = obj->GetName();
	m_clipboard.pos = obj->GetPos();
	m_clipboard.rot = obj->GetRot();
	m_clipboard.scale = obj->GetScale();
}

std::shared_ptr<KdGameObject> LevelEditorManager::Paste()
{
	if (!m_clipboard.hasData) { return nullptr; }

	std::shared_ptr<KdGameObject> spObj = CreateObject(m_clipboard.typeName);
	if (!spObj) { return nullptr; }

	// 元とぴったり重なると分かりにくいので、少しずらして配置する
	Math::Vector3 pastePos = m_clipboard.pos + Math::Vector3(1.0f, 0.0f, 1.0f);

	spObj->SetName(m_clipboard.name);
	spObj->SetPos(pastePos);
	spObj->SetRot(m_clipboard.rot);
	spObj->SetScale(m_clipboard.scale);

	m_wpSelected = spObj;

	return spObj;
}

std::shared_ptr<KdGameObject> LevelEditorManager::DuplicateSelected()
{
	CopySelected();
	return Paste();
}

void LevelEditorManager::ApplyEditorCameraState(bool wantsEnabled)
{
	if (wantsEnabled)
	{
		if (!m_spEditorCamera)
		{
			m_spEditorCamera = std::make_shared<EditorCamera>();
			m_spEditorCamera->Init();
		}

		SceneManager::Instance().AddObject(m_spEditorCamera);
	}
	else
	{
		if (m_spEditorCamera)
		{
			SceneManager::Instance().RemoveObject(m_spEditorCamera);
		}
	}
}

void LevelEditorManager::Draw()
{
	if (!m_enabled) { return; }

	if (!ImGui::Begin(U8("LevelEditor")))
	{
		ImGui::End();
		return;
	}

	// 編集用フリーカメラのON/OFF
	if (ImGui::Checkbox(U8("編集用フリーカメラ (WASD+QE, マウスで視点回転, Shiftで加速)"), &m_useEditorCamera))
	{
		ApplyEditorCameraState(m_useEditorCamera);
	}

	ImGui::TextDisabled(U8("Ctrl+C: コピー / Ctrl+V: 貼り付け / Ctrl+D: その場で複製"));

	ImGui::Separator();

	LevelObjectPanel::Instance().Draw();

	ImGui::Separator();

	LevelInspector::Instance().Draw();

	ImGui::Separator();

	LevelFileIO::Instance().Draw();

	ImGui::End();
}

void LevelEditorManager::DrawHighlight()
{
	if (!m_enabled) { return; }

	std::shared_ptr<KdGameObject> obj = GetSelected();
	if (!obj) { return; }

	// 選択中オブジェクトを囲むボックス(黄色)
	static const Math::Color highlightColor = Math::Color(1.0f, 1.0f, 0.0f, 1.0f);
	m_highlightWire.AddDebugBox(obj->GetMatrix(), m_highlightBoxExtents, { 0, 0, 0 }, true, highlightColor);

	// 移動ギズモ(ワールド軸に沿った赤:X 緑:Y 青:Z のハンドル)
	Math::Vector3 pos = obj->GetPos();
	m_highlightWire.AddDebugLine(pos, Math::Vector3(1, 0, 0), m_gizmoLength, kRedColor);
	m_highlightWire.AddDebugLine(pos, Math::Vector3(0, 1, 0), m_gizmoLength, kGreenColor);
	m_highlightWire.AddDebugLine(pos, Math::Vector3(0, 0, 1), m_gizmoLength, kBlueColor);

	m_highlightWire.Draw();
}
