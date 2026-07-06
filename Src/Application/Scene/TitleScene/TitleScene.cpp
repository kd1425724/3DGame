#include "TitleScene.h"
#include "../SceneManager.h"

void TitleScene::Event()
{
	if (KdInputManager::Instance().IsHold("Confirm"))
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Game
		);
	}
}

void TitleScene::Init()
{
}
