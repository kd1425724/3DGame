#pragma once

//====================================================
//
// レベルエディタ：Undo/Redo(元に戻す/やり直す)管理
//  ・エディタで配置したオブジェクトの状態(種類/名前/座標/回転/スケール)を
//    丸ごとスナップショットとして履歴に積んでおき、Undo/Redoで復元する
//  ・値を変更する"直前"にPushUndo()を呼んでおく運用
//    (LevelEditorManager::CreateObject/RemoveObject自体は
//     LevelFileIO::LoadやUndo/Redoの復元処理からも呼ばれるため、
//     そちらでは呼ばずインタラクティブなUI操作側で呼ぶこと)
//
//====================================================
class LevelEditorHistory
{
public:

	static LevelEditorHistory& Instance()
	{
		static LevelEditorHistory instance;
		return instance;
	}

	// 現在の状態を履歴に積む(何か変更を加える直前に呼ぶ)
	void PushUndo();

	// 元に戻す
	void Undo();

	// やり直す
	void Redo();

	// ImGui描画(Undo/Redoボタン)
	void Draw();

private:

	LevelEditorHistory() = default;
	~LevelEditorHistory() = default;
	LevelEditorHistory(const LevelEditorHistory&) = delete;
	void operator=(const LevelEditorHistory&) = delete;

	// 1オブジェクト分のスナップショット
	struct ObjectSnapshot
	{
		std::string		typeName;
		std::string		name;
		Math::Vector3	pos;
		Math::Vector3	rot;
		Math::Vector3	scale;
	};

	// ある時点で配置されていた全オブジェクトの状態
	using Snapshot = std::vector<ObjectSnapshot>;

	// 現在の状態をスナップショットとして取得する
	Snapshot CaptureSnapshot() const;

	// スナップショットの状態を復元する(現在のオブジェクトは一旦全部消してから再構築する)
	void RestoreSnapshot(const Snapshot& snapshot);

	// 積みすぎ防止の上限
	static const size_t kMaxHistoryCount = 50;

	std::vector<Snapshot> m_undoStack;
	std::vector<Snapshot> m_redoStack;
};
