#pragma once

class BaseScene;

class SceneManager
{
public :

	// シーン情報
	enum class SceneType
	{
		Title,
		Game,
	};

	// マネージャーの初期化
	// ※ コンストラクタからは呼ばない。Application::Init()などシングルトン初期化が
	//    確実に完了した後の場所で明示的に呼び出すこと。
	//    (コンストラクタ内で呼ぶと、初期化処理の中でSceneManager::Instance()を
	//     再帰的に呼んだ瞬間、static変数の初期化待ちで自己デッドロックするため)
	void Init()
	{
		// 開始シーンに切り替え
		ChangeScene(m_currentSceneType);
	}

	void PreUpdate();
	void Update();
	void PostUpdate();

	void PreDraw();
	void Draw();
	void DrawSprite();
	void DrawDebug();

	// 次のシーンをセット (次のフレームから切り替わる)
	void SetNextScene(SceneType _nextScene)
	{
		m_nextSceneType = _nextScene;
	}

	// 現在のシーンのオブジェクトリストを取得
	const std::list<std::shared_ptr<KdGameObject>>& GetObjList();

	// 現在のシーンにオブジェクトを追加
	void AddObject(const std::shared_ptr<KdGameObject>& _obj);

	// 現在のシーンからオブジェクトを削除
	void RemoveObject(const std::shared_ptr<KdGameObject>& _obj);

private :

	// シーン切り替え関数
	void ChangeScene(SceneType _sceneType);

	// 現在のシーンのインスタンスを保持しているポインタ
	std::shared_ptr<BaseScene> m_currentScene = nullptr;

	// 現在のシーンの種類を保持している変数
	SceneType m_currentSceneType = SceneType::Game;
	
	// 次のシーンの種類を保持している変数
	SceneType m_nextSceneType = m_currentSceneType;

private:

	SceneManager() = default;
	~SceneManager() {}

public:

	// シングルトンパターン
	// 常に存在する && 必ず1つしか存在しない(1つしか存在出来ない)
	// どこからでもアクセスが可能で便利だが
	// 何でもかんでもシングルトンという思考はNG
	static SceneManager& Instance()
	{
		static SceneManager instance;
		return instance;
	}
};
