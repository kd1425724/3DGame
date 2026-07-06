#pragma once

class EditorCamera;

//====================================================
//
// レベルエディタ(配置ツール)の管理クラス
//  ・シーン上にオブジェクトを配置/削除/選択する
//  ・選択中オブジェクトの座標などは LevelInspector が編集する
//  ・配置データの保存/読込は LevelFileIO が行う
//  ・オブジェクトの追加/削除は SceneManager::Instance() 経由で
//    「現在のシーン」に対して行う(SceneManagerが委譲してくれるので、
//    このクラスがどのシーンかを個別に把握する必要は無い)
//
// セットアップ方法：
//  配置可能にしたいクラスをゲームコードのどこかで登録するだけでよい
//    LevelEditorManager::Instance().RegisterCreatable("Enemy_Slime", []() {
//        auto obj = std::make_shared<EnemySlime>();
//        obj->Init();
//        return obj;
//    });
//
//====================================================
class LevelEditorManager
{
public:

	static LevelEditorManager& Instance()
	{
		static LevelEditorManager instance;
		return instance;
	}

	// エディタで配置可能なオブジェクトの種類を登録する
	void RegisterCreatable(const std::string_view name, const std::function<std::shared_ptr<KdGameObject>(void)>& func)
	{
		m_factory.RegisterCreateFunction(name, func);
		m_creatableNames.push_back(std::string(name));
	}

	// 登録済みの種類名一覧(一覧表示用。登録順を保持する)
	const std::vector<std::string>& GetCreatableNames() const { return m_creatableNames; }

	// 名前を指定してオブジェクトを生成し、現在のシーンに追加する
	std::shared_ptr<KdGameObject> CreateObject(const std::string_view objTypeName);

	// 現在のシーンからオブジェクトを削除する
	void RemoveObject(const std::shared_ptr<KdGameObject>& obj);

	// 生成時に使った種類名を取得する(保存時に使用。エディタ経由で生成していない場合は空文字)
	std::string GetObjectTypeName(const std::shared_ptr<KdGameObject>& obj) const
	{
		auto itr = m_objectTypeNames.find(obj.get());
		return (itr != m_objectTypeNames.end()) ? itr->second : std::string();
	}

	// 選択を1つだけにする(既存の選択は全て解除される)
	void SetSelected(const std::shared_ptr<KdGameObject>& obj);

	// 主選択(選択中の先頭)を取得する。何も選択されていなければnullptr
	std::shared_ptr<KdGameObject> GetSelected() const;

	// 選択の追加/解除を切り替える(Ctrl+クリック用：選択済みなら外し、未選択なら追加する)
	void ToggleSelected(const std::shared_ptr<KdGameObject>& obj);

	// 現在選択中の全オブジェクトを取得する(既に破棄されたものは除く)
	std::vector<std::shared_ptr<KdGameObject>> GetSelectedList() const;

	// 指定のオブジェクトが選択中かどうか
	bool IsSelected(const std::shared_ptr<KdGameObject>& obj) const;

	// 選択を全て解除する
	void ClearSelection();

	// 選択中の全オブジェクトを削除する(呼び出し側でPushUndo()しておくこと)
	void RemoveSelectedObjects();

	// エディタモードのON/OFF(選択/ギズモ操作/ハイライト表示などが全て止まる)
	// ※ 直接書き換えても良いが、編集用フリーカメラも一緒に切りたい場合はSetEnabled()を使うこと
	bool m_enabled = true;

	// エディタモードのON/OFFを切り替える(OFFにする場合、編集用フリーカメラも一緒に無効化してゲームプレイ側のカメラに戻す)
	void SetEnabled(bool enabled);

	// ImGui描画(NewFrame~Renderの間で呼び出す。DebugManager::Draw() から呼ばれる)
	void Draw();

	// 選択中オブジェクトのワイヤーフレームハイライトを描画する
	// (BaseScene::DrawDebug() から、通常の3D描画パスの中で呼ばれる。ImGuiとは別)
	void DrawHighlight();

	// ハイライトボックスの半径(オブジェクトのサイズに応じて調整する)
	Math::Vector3 m_highlightBoxExtents = { 0.5f, 0.5f, 0.5f };

	// 移動ギズモ(X/Y/Z軸ハンドル)の見た目上の長さ
	float m_gizmoLength = 2.0f;

	// 移動ギズモの軸を「掴んだ」と判定する距離のしきい値(ワールド単位)
	float m_gizmoGrabRadius = 0.3f;

	// 編集用フリーカメラのON/OFF(ImGuiのチェックボックスから切り替える)
	// ※ シーンを切り替えた場合、フリーカメラは新しいシーンには自動で付いてこないので
	//    その場合は一度OFF→ONにし直してください
	bool m_useEditorCamera = false;

	// グリッドスナップ(座標)のON/OFFとマス目のサイズ(ワールド単位)
	bool  m_useGridSnap = false;
	float m_gridSnapSize = 1.0f;

	// 回転スナップのON/OFFと刻み角度(度数)
	bool  m_useRotationSnap = false;
	float m_rotationSnapDeg = 15.0f;

	// グリッドスナップが有効な場合、座標を最寄りのマス目に丸めて返す(無効ならそのまま返す)
	Math::Vector3 SnapPos(const Math::Vector3& pos) const;

	// 回転スナップが有効な場合、各軸の角度を最寄りの刻み角度に丸めて返す(無効ならそのまま返す)
	Math::Vector3 SnapRot(const Math::Vector3& rot) const;

	// 現在有効な編集用フリーカメラを取得する(無効なら nullptr。LevelPickerのレイキャストに使用)
	std::shared_ptr<EditorCamera> GetEditorCamera() const
	{
		return m_useEditorCamera ? m_spEditorCamera : nullptr;
	}

	// 毎フレームの更新(Application::KdBeginUpdateから呼ばれる。3Dビューのクリック選択など)
	void Update();

	// 選択中オブジェクトをコピーする(クリップボードに保存するだけで、まだ増えない)
	void CopySelected();

	// クリップボードの内容を新しいオブジェクトとして貼り付ける(元と重ならないよう少しずらす)
	std::shared_ptr<KdGameObject> Paste();

	// 選択中オブジェクトを複製する(コピー+貼り付けを1回で行う。Ctrl+Dやボタン用)
	std::shared_ptr<KdGameObject> DuplicateSelected();

private:

	LevelEditorManager() = default;
	~LevelEditorManager() = default;
	LevelEditorManager(const LevelEditorManager&) = delete;
	void operator=(const LevelEditorManager&) = delete;

	// フリーカメラのON/OFF切り替え(トグルが変化した時だけ内部で呼ばれる)
	void ApplyEditorCameraState(bool wantsEnabled);

	// オブジェクト生成用ファクトリー(このレベルエディタ専用)
	KdGameObjectFactory m_factory;

	// 登録済みの種類名(表示順を保持するため別途保持)
	std::vector<std::string> m_creatableNames;

	// 生成したオブジェクトがどの種類名で作られたか(保存時に使用)
	std::unordered_map<const KdGameObject*, std::string> m_objectTypeNames;

	// 現在選択中のオブジェクト一覧(先頭が主選択。Ctrl+クリックで追加/解除される)
	std::vector<std::weak_ptr<KdGameObject>> m_wpSelectedList;

	// ハイライト表示用のワイヤーフレーム(毎フレームAdd→Drawするだけの使い捨て)
	KdDebugWireFrame m_highlightWire;

	// 編集用フリーカメラの実体
	std::shared_ptr<EditorCamera> m_spEditorCamera;

	// コピー&ペースト用のクリップボード
	struct ClipboardData
	{
		bool			hasData = false;
		std::string		typeName;
		std::string		name;
		Math::Vector3	pos;
		Math::Vector3	rot;
		Math::Vector3	scale;
	};
	ClipboardData m_clipboard;
};
