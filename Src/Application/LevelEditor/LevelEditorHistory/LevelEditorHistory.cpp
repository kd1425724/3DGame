#include "LevelEditorHistory.h"

#include "../LevelEditorManager.h"
#include "../../Scene/SceneManager.h"

void LevelEditorHistory::PushUndo()
{
	m_undoStack.push_back(CaptureSnapshot());

	if (m_undoStack.size() > kMaxHistoryCount)
	{
		m_undoStack.erase(m_undoStack.begin());
	}

	// 新しい操作をした時点で、それより先のRedo履歴は無意味になるため破棄する
	m_redoStack.clear();
}

void LevelEditorHistory::Undo()
{
	if (m_undoStack.empty()) { return; }

	// 今の状態をRedo用に残しておく
	m_redoStack.push_back(CaptureSnapshot());

	Snapshot snapshot = m_undoStack.back();
	m_undoStack.pop_back();

	RestoreSnapshot(snapshot);
}

void LevelEditorHistory::Redo()
{
	if (m_redoStack.empty()) { return; }

	// 今の状態をUndo用に残しておく
	m_undoStack.push_back(CaptureSnapshot());

	Snapshot snapshot = m_redoStack.back();
	m_redoStack.pop_back();

	RestoreSnapshot(snapshot);
}

void LevelEditorHistory::Draw()
{
	ImGui::BeginDisabled(m_undoStack.empty());
	if (ImGui::Button(U8("元に戻す (Ctrl+Z)")))
	{
		Undo();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	ImGui::BeginDisabled(m_redoStack.empty());
	if (ImGui::Button(U8("やり直す (Ctrl+Y)")))
	{
		Redo();
	}
	ImGui::EndDisabled();
}

LevelEditorHistory::Snapshot LevelEditorHistory::CaptureSnapshot() const
{
	Snapshot snapshot;

	LevelEditorManager& mgr = LevelEditorManager::Instance();

	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }

		// レベルエディタ経由で生成していないオブジェクト(種類名が分からない)は対象外
		std::string typeName = mgr.GetObjectTypeName(obj);
		if (typeName.empty()) { continue; }

		ObjectSnapshot s;
		s.typeName = typeName;
		s.name = obj->GetName();
		s.pos = obj->GetPos();
		s.rot = obj->GetRot();
		s.scale = obj->GetScale();

		snapshot.push_back(s);
	}

	return snapshot;
}

void LevelEditorHistory::RestoreSnapshot(const Snapshot& snapshot)
{
	LevelEditorManager& mgr = LevelEditorManager::Instance();

	// 現在配置されているエディタ管理下のオブジェクトを一旦すべて削除する
	std::vector<std::shared_ptr<KdGameObject>> removeList;

	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }
		if (mgr.GetObjectTypeName(obj).empty()) { continue; }

		removeList.push_back(obj);
	}

	for (auto& obj : removeList)
	{
		mgr.RemoveObject(obj);
	}

	// スナップショットの内容から再構築する
	for (auto& s : snapshot)
	{
		std::shared_ptr<KdGameObject> obj = mgr.CreateObject(s.typeName);
		if (!obj) { continue; }

		obj->SetName(s.name);
		obj->SetPos(s.pos);
		obj->SetRot(s.rot);
		obj->SetScale(s.scale);
	}

	mgr.SetSelected(nullptr);
}
