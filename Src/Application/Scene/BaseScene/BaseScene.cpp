#include "BaseScene.h"

#include "../../main.h"
#include "../../Editor/LevelEditor/LevelEditorManager.h"
#include "../../GameObject/Camera/EditorCamera/EditorCamera.h"
#include "../../Effect/EffectManager.h"
#include "../../Debug/DebugWatch/DebugWatch.h"
#include "../../Debug/DebugFlags/DebugFlags.h"   // [SHADOWDEBUG] シャドウマップ表示の切り替え

void BaseScene::PreUpdate()
{
	ZoneScoped;	// Tracy計測(2026/07/19)

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
	ZoneScoped;	// Tracy計測(2026/07/19)

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
	ZoneScoped;	// Tracy計測(2026/07/19)：当たり判定の解決はここに集まる

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
	ZoneScoped;	// Tracy計測(2026/07/19)：カリングの視錐台更新もここ(CameraBase::PreDraw)

	for (auto& obj : m_objList)
	{
		obj->PreDraw();
	}
}

void BaseScene::Draw()
{
	ZoneScoped;	// Tracy計測(2026/07/19)

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 光を遮るオブジェクト(影を生み出す要因となるオブジェクト)をBeginとEndの間にまとめてDrawする
	KdShaderManager::Instance().m_StandardShader.BeginGenerateDepthMapFromLight();
	{
		// Tracy計測(2026/07/19)：影の生成パス。重ければ影を落とす対象を減らす判断材料になる
		ZoneScopedN("Draw: ShadowMap");

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
		// Tracy計測(2026/07/19)：陰影なしパス(背景など)
		ZoneScopedN("Draw: UnLit");

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
		// Tracy計測(2026/07/19)：陰影ありパス。建物のドローはここ。最重要の計測点
		ZoneScopedN("Draw: Lit");

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
		// Tracy計測(2026/07/19)：エフェクト描画パス
		ZoneScopedN("Draw: Effect");

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
		// Tracy計測(2026/07/19)：発光パス(ブルームの元)
		ZoneScopedN("Draw: Bright");

		for (auto& obj : m_objList)
		{
			obj->DrawBright();
		}
	}
	KdShaderManager::Instance().m_postProcessShader.EndBright();
}

void BaseScene::DrawSprite()
{
	ZoneScoped;	// Tracy計測(2026/07/19)：2D(UI/HUD)描画

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 2Dの描画はこの間で行う
	KdShaderManager::Instance().m_spriteShader.Begin();
	{
		for (auto& obj : m_objList)
		{
			obj->DrawSprite();
		}

		// [SHADOWDEBUG] 影の調査用の一時表示(2026/07/20)。
		// 「影の範囲が狭い/遠くが暗い」の原因を、推測でなくシャドウマップを直接見て切り分ける。
		// 白い部分=遠い(何も無い) / 黒っぽい部分=近い(影を落とす物が書き込まれている)。
		// 建物がどこまで書き込まれているか、範囲に対してどれくらい詰まっているかを見る。
		// ※ 原因が確定したらこのブロックごと削除する(grep: SHADOWDEBUG)
		if (DebugFlags::Instance().Get(U8("影/シャドウマップ表示"), false))
		{
			const std::shared_ptr<KdTexture>& spDepth =
				KdShaderManager::Instance().m_StandardShader.GetDepthTex();

			if (spDepth)
			{
				KdShaderManager::Instance().m_spriteShader.DrawTex(spDepth.get(), 0, 0, 512, 512);
			}
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
