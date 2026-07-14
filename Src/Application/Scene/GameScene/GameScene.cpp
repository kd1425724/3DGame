#include "GameScene.h"
#include"../SceneManager.h"

#include "../../GameObject/Ground/Ground.h"
#include "../../GameObject/Chara/Player/Player.h"
#include "../../GameObject/Chara/Enemy/Enemy.h"
#include "../../GameObject/EnemySpawner/EnemySpawner.h"
#include "../../GameObject/Camera/TPSCamera/TPSCamera.h"

#include "../../LevelEditor/LevelFileIO/LevelFileIO.h"
#include "../../LevelEditor/LevelEditorManager.h"

void GameScene::Event()
{
	if (KdInputManager::Instance().IsHold("SwitchScene"))
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Title
		);
	}
}

void GameScene::Init()
{
	// 地面
	std::shared_ptr<Ground> spGround = std::make_shared<Ground>();
	spGround->Init();
	AddObject(spGround);

	// プレイヤー(高所スタート。起動時に落下してコースへ降りてくる。地面・塔は原点基準で接地)
	std::shared_ptr<Player> spPlayer = std::make_shared<Player>();
	spPlayer->Init();
	spPlayer->SetPos(Math::Vector3(0, 20.0f, 0));
	spPlayer->SetSpawnPos(Math::Vector3(0, 20.0f, 0));   // 落下リセット/Rキーの復帰先
	AddObject(spPlayer);

	// TPSカメラ(プレイヤーを追従、地面との当たり判定でめり込み防止)
	std::shared_ptr<TPSCamera> spCamera = std::make_shared<TPSCamera>();
	spCamera->Init();
	spCamera->SetTarget(spPlayer);
	// 【現在未使用】以前はカメラのめり込み判定対象を手動登録していた。
	// 現在はTPSCameraがSceneManagerの全オブジェクトを走査するため登録不要(CameraBase::RegistHitObject参照)。
	//spCamera->RegistHitObject(spGround);
	AddObject(spCamera);

	// GameSceneからもカメラを参照できるように保持
	m_wpCamera = spCamera;

	// プレイヤーの移動をカメラの水平方向の向き基準にする
	spPlayer->SetCameraReference(spCamera);

	// 【仮処理】敵を次々にスポーンする簡易スポナー(本実装のスポーン設計に置き換える前提)
	// ※ 生成された敵は自分でPlayerをタグ検索して追従する。数/間隔はDebugParams「敵スポナー/…」
	std::shared_ptr<EnemySpawner> spSpawner = std::make_shared<EnemySpawner>();
	AddObject(spSpawner);

	// 保存済みレベル(レベルエディタで配置したBlock等)を読み込んで上に足す
	// ※ 追加ロードなので上のGround/Player/Enemyはそのまま。ファイルが無ければ何も足さない
	LevelFileIO::Instance().Load("Asset/Data/LevelEditor/Level.json");

	// 読み込み直後は最後の生成物が選択状態になるので、起動時のハイライトを消す
	LevelEditorManager::Instance().ClearSelection();
}
