#include "LevelEditorManager.h"

#include <cmath>

#include "../Scene/SceneManager.h"
#include "../GameObject/Camera/EditorCamera/EditorCamera.h"
#include "LevelObjectPanel/LevelObjectPanel.h"
#include "LevelInspector/LevelInspector.h"
#include "LevelFileIO/LevelFileIO.h"
#include "LevelPicker/LevelPicker.h"
#include "LevelEditorHistory/LevelEditorHistory.h"

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

void LevelEditorManager::Update()
{
	LevelPicker::Instance().Update();

	if (m_enabled && !ImGui::GetIO().WantCaptureKeyboard)
	{
		bool ctrl = KdInputManager::Instance().IsHold("Ctrl");

		if (ctrl)
		{
			if (KdInputManager::Instance().IsPress("Copy"))		{ CopySelected(); }
			if (KdInputManager::Instance().IsPress("Paste"))		{ Paste(); }
			if (KdInputManager::Instance().IsPress("Duplicate"))	{ DuplicateSelected(); }
			if (KdInputManager::Instance().IsPress("Undo"))		{ LevelEditorHistory::Instance().Undo(); }
			if (KdInputManager::Instance().IsPress("Redo"))		{ LevelEditorHistory::Instance().Redo(); }
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

	LevelEditorHistory::Instance().PushUndo();

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

Math::Vector3 LevelEditorManager::SnapPos(const Math::Vector3& pos) const
{
	if (!m_useGridSnap || m_gridSnapSize <= 0.0f) { return pos; }

	auto snapAxis = [this](float value)
	{
		return std::round(value / m_gridSnapSize) * m_gridSnapSize;
	};

	return Math::Vector3(snapAxis(pos.x), snapAxis(pos.y), snapAxis(pos.z));
}

Math::Vector3 LevelEditorManager::SnapRot(const Math::Vector3& rot) const
{
	if (!m_useRotationSnap || m_rotationSnapDeg <= 0.0f) { return rot; }

	auto snapAxis = [this](float value)
	{
		return std::round(value / m_rotationSnapDeg) * m_rotationSnapDeg;
	};

	return Math::Vector3(snapAxis(rot.x), snapAxis(rot.y), snapAxis(rot.z));
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

	// グリッドスナップ(座標)
	ImGui::Checkbox(U8("グリッドスナップ"), &m_useGridSnap);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::DragFloat(U8("マス目サイズ"), &m_gridSnapSize, 0.1f, 0.1f, 100.0f);

	// 回転スナップ
	ImGui::Checkbox(U8("回転スナップ"), &m_useRotationSnap);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::DragFloat(U8("刻み角度"), &m_rotationSnapDeg, 1.0f, 1.0f, 180.0f);

	ImGui::TextDisabled(U8("Ctrl+C: コピー / Ctrl+V: 貼り付け / Ctrl+D: その場で複製 / Ctrl+Z: 元に戻す / Ctrl+Y: やり直す"));

	LevelEditorHistory::Instance().Draw();

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
