#include "EnemySpawner.h"

#include "../../main.h"
#include "../../Scene/SceneManager.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../Chara/Enemy/Enemy.h"

void EnemySpawner::Update()
{
	float dt = Application::Instance().GetDeltaTime();

	// 調整値(すべて仮。DebugParamsで実行中に変えられる)
	float interval = DebugParams::Instance().Float(U8("敵スポナー/間隔"),     1.0f, 0.05f, 10.0f);
	int   maxAlive = static_cast<int>(DebugParams::Instance().Float(U8("敵スポナー/最大数"), 30.0f, 1.0f, 200.0f));
	float range    = DebugParams::Instance().Float(U8("敵スポナー/範囲"),    15.0f, 1.0f, 100.0f);
	float spawnY   = DebugParams::Instance().Float(U8("敵スポナー/出現高さ"), 20.0f, 0.0f, 100.0f);

	// 間隔を待つ
	m_timer += dt;
	if (m_timer < interval) { return; }
	m_timer = 0.0f;

	// 生存数が上限に達していたら出さない(=消えた分だけ補充され続ける)
	if (static_cast<int>(SceneManager::Instance().FindObjectsWithTag(ObjectTag::Enemy).size()) >= maxAlive) { return; }

	// 範囲内のランダムな水平位置に、高所から出す(落ちて地面に接地する)
	auto rnd = []() { return (rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f; };   // -1〜+1
	float rx = rnd() * range;
	float rz = rnd() * range;

	// 敵を生成してシーンに追加(追従対象=Playerは敵が自分でタグ検索して見つける)
	std::shared_ptr<Enemy> spEnemy = std::make_shared<Enemy>();
	spEnemy->Init();
	spEnemy->SetPos(Math::Vector3(rx, spawnY, rz));
	SceneManager::Instance().AddObject(spEnemy);
}
