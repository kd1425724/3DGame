#pragma once

#include"../BaseScene/BaseScene.h"

class TPSCamera;

class GameScene : public BaseScene
{
public :

	GameScene()  { }
	~GameScene() {}

	void Init()  override;

private:

	void Event() override;

	// カメラオブジェクトへの参照(必要な時に取り出せるように保持)
	std::weak_ptr<TPSCamera> m_wpCamera;
};
