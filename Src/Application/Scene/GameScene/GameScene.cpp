#include "GameScene.h"
#include"../SceneManager.h"

#include "../../GameObject/Ground/Ground.h"
#include "../../GameObject/Chara/Player/Player.h"
#include "../../GameObject/Chara/Enemy/Enemy.h"
#include "../../GameObject/Camera/TPSCamera/TPSCamera.h"

void GameScene::Event()
{
	if (GetAsyncKeyState('T') & 0x8000)
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

	// プレイヤー
	std::shared_ptr<Player> spPlayer = std::make_shared<Player>();
	spPlayer->Init();
	spPlayer->SetPos(Math::Vector3(0, 1.0f, 0));
	AddObject(spPlayer);

	// TPSカメラ(プレイヤーを追従、地面との当たり判定でめり込み防止)
	std::shared_ptr<TPSCamera> spCamera = std::make_shared<TPSCamera>();
	spCamera->Init();
	spCamera->SetTarget(spPlayer);
	spCamera->RegistHitObject(spGround);
	AddObject(spCamera);

	// GameSceneからもカメラを参照できるように保持
	m_wpCamera = spCamera;

	// プレイヤーの移動をカメラの水平方向の向き基準にする
	spPlayer->SetCameraReference(spCamera);

	// 敵(プレイヤーにゆっくり追従し、接触すると消滅する)
	std::shared_ptr<Enemy> spEnemy = std::make_shared<Enemy>();
	spEnemy->Init();
	spEnemy->SetPos(Math::Vector3(3.0f, 1.0f, 3.0f));
	spEnemy->SetTarget(spPlayer);
	AddObject(spEnemy);
}
