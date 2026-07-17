#include "BaseScene.h"

#include "../../main.h"
#include "../../Editor/LevelEditor/LevelEditorManager.h"
#include "../../GameObject/Camera/EditorCamera/EditorCamera.h"
#include "../../Effect/EffectManager.h"
#include "../../Debug/DebugWatch/DebugWatch.h"

void BaseScene::PreUpdate()
{
	// Updateの前の更新処理
	// オブジェクトリストの整理 ・・・ 無効なオブジェクトを削除
	auto it = m_objList.begin();

	while (it != m_objList.end())
	{
		if ((*it)->IsExpired())	// IsExpired() ・・・ 無効ならtrue
		{
			// 無効なオブジェクトをリストから削除
			it = m_objList.erase(it);
		}
		else
		{
			++it;	// 次の要素へイテレータを進める
		}
	}

	// ↑の後には有効なオブジェクトだけのリストになっている

	for (auto& obj : m_objList)
	{
		obj->PreUpdate();
	}
}

void BaseScene::Update()
{
	// デバッグ: シーン内のオブジェクト数をWatchに出す(最適化の効果測定用)
	DebugWatch::Instance().Watch(U8("Scene/オブジェクト数"), static_cast<int>(m_objList.size()));

	// シーン毎のイベント処理
	Event();

	// フリーカメラ使用中はエディタカメラ以外のゲーム進行を止める(自由に見渡せるようにするため)
	std::shared_ptr<KdGameObject> spEditorCamera = LevelEditorManager::Instance().GetEditorCamera();

	// KdGameObjectを継承した全てのオブジェクトの更新 (ポリモーフィズム)
	for (auto& obj : m_objList)
	{
		if (spEditorCamera && obj != spEditorCamera) { continue; }

		obj->Update();
	}
}

void BaseScene::PostUpdate()
{
	// フリーカメラ使用中はエディタカメラ以外のゲーム進行を止める(自由に見渡せるようにするため)
	std::shared_ptr<KdGameObject> spEditorCamera = LevelEditorManager::Instance().GetEditorCamera();

	for (auto& obj : m_objList)
	{
		if (spEditorCamera && obj != spEditorCamera) { continue; }

		obj->PostUpdate();
	}

	// エフェクト(斬撃VFX等)の経過を進める(寿命で自動消滅。dtは各エフェクトが自分で取る)
	EffectManager::Instance().Update();
}

void BaseScene::PreDraw()
{
	for (auto& obj : m_objList)
	{
		obj->PreDraw();
	}
}

void BaseScene::Draw()
{
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 光を遮るオブジェクト(影を生み出す要因となるオブジェクト)をBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_StandardShader.BeginGenerateDepthMapFromLight();
	{
		for (auto& obj : m_objList)
		{
			obj->GenerateDepthMapFromLight();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndGenerateDepthMapFromLight();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 陰影のないオブジェクト(背景など)はBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_StandardShader.BeginUnLit();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawUnLit();
		}

		// エフェクト(斬撃VFX等)も陰影なしパスで描く
		EffectManager::Instance().DrawUnLit();
	}
	KdShaderManager::Instance().m_StandardShader.EndUnLit();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 陰影のあるオブジェクト(光源の影響を受けるオブジェクト)はBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_StandardShader.BeginLit();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawLit();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndLit();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 陰影のないオブジェクト(エフェクトなど)はBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_StandardShader.BeginUnLit();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawEffect();
		}
	}
	KdShaderManager::Instance().m_StandardShader.EndUnLit();

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 光源オブジェクト(自ら光るオブジェクトやエフェクト)はBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_postProcessShader.BeginBright();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawBright();
		}
	}
	KdShaderManager::Instance().m_postProcessShader.EndBright();
}

void BaseScene::DrawSprite()
{
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 2Dの描画はこの間で行う
	KdShaderManager::Instance().m_spriteShader.Begin();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawSprite();
		}
	}
	KdShaderManager::Instance().m_spriteShader.End();
}

void BaseScene::DrawDebug()
{
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// デバッグ情報の描画はこの間で行う
	KdShaderManager::Instance().m_StandardShader.BeginUnLit();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawDebug();
		}

		// レベルエディタで選択中のオブジェクトをハイライト表示
		LevelEditorManager::Instance().DrawHighlight();
	}
	KdShaderManager::Instance().m_StandardShader.EndUnLit();
}

void BaseScene::Event()
{
	// 各シーンで必要な内容を実装(オーバーライド)する
}

void BaseScene::Init()
{
	// 各シーンで必要な内容を実装(オーバーライド)する
}
