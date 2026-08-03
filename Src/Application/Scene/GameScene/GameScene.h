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

	// 街(StageProp)を物理世界へ静的形状として登録する。
	// ※ レベルを読み終えた【後】に呼ぶこと(StageProp::Initの時点ではまだ配置が入っていない)
	void RegisterStagePropsToPhysics();

	// カメラオブジェクトへの参照(必要な時に取り出せるように保持)
	std::weak_ptr<TPSCamera> m_wpCamera;
};
