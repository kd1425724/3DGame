#include "main.h"

#include "Scene/SceneManager.h"
#include "Debug/DebugManager.h"
#include "Debug/DebugFlags/DebugFlags.h"
#include "LevelEditor/LevelEditorManager.h"
#include "GameObject/Block/Block.h"
#include "GameObject/Ground/Ground.h"
#include "GameObject/Chara/Enemy/Enemy.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エントリーポイント
// アプリケーションはこの関数から進行する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_  HINSTANCE, _In_ LPSTR , _In_ int)
{
	// メモリリークを知らせる
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// COM初期化
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		CoUninitialize();

		return 0;
	}

	// mbstowcs_s関数で日本語対応にするために呼ぶ
	setlocale(LC_ALL, "japanese");

	//===================================================================
	// 実行]
	//===================================================================
	Application::Instance().Execute();

	// COM解放
	CoUninitialize();

	return 0;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdBeginUpdate()
{
	// 入力状況の更新
	KdInputManager::Instance().Update();

	// 空間環境の更新
	KdShaderManager::Instance().WorkAmbientController().Update();

	// デバッグ用ImGui機能の更新開始(DebugWatchの前フレーム情報クリアなど)
	DebugManager::Instance().BeginFrame();

	// 当たり判定デバッグ表示の切り替え(全GameObject共通)
	KdGameObject::s_showColliderDebug = DebugFlags::Instance().Get(U8("当たり判定/AABB表示"));

	// レベルエディタの更新(3Dビューのクリック選択など)
	LevelEditorManager::Instance().Update();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostUpdate()
{
	// 3DSoundListnerの行列を更新
	KdAudioManager::Instance().SetListnerMatrix(KdShaderManager::Instance().GetCameraCB().mView.Invert());

	// Effekseerエフェクトの更新
	KdEffekseerManager::GetInstance().Update();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新の前処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PreUpdate()
{
	SceneManager::Instance().PreUpdate();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Update()
{
	SceneManager::Instance().Update();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新の後処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PostUpdate()
{
	SceneManager::Instance().PostUpdate();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdBeginDraw(bool usePostProcess)
{
	KdDirect3D::Instance().ClearBackBuffer();

	KdShaderManager::Instance().WorkAmbientController().Draw();

	if (!usePostProcess) return;
	KdShaderManager::Instance().m_postProcessShader.Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostDraw()
{
	// Imguiのレンダリング
	KdDebugGUI::Instance().GuiProcess();

	// BackBuffer -> 画面表示
	KdDirect3D::Instance().WorkSwapChain()->Present(0, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画の前処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PreDraw()
{
	SceneManager::Instance().PreDraw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Draw()
{
	SceneManager::Instance().Draw();

	// Effekseerエフェクトの描画
	KdEffekseerManager::GetInstance().Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画の後処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PostDraw()
{
	// 画面のぼかしや被写界深度処理の実施
	KdShaderManager::Instance().m_postProcessShader.PostEffectProcess();

	// 現在のシーンのデバッグ描画
	SceneManager::Instance().DrawDebug();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 2Dスプライトの描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::DrawSprite()
{
	SceneManager::Instance().DrawSprite();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション初期設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool Application::Init(int w, int h)
{
	//===================================================================
	// ウィンドウ作成
	//===================================================================
	if (m_window.Create(w, h, "3D GameProgramming", "Window") == false) {
		MessageBoxA(nullptr, "ウィンドウ作成に失敗", "エラー", MB_OK);
		return false;
	}

	//===================================================================
	// フルスクリーン確認
	//===================================================================
	bool bFullScreen = false;
//	if (MessageBoxA(m_window.GetWndHandle(), "フルスクリーンにしますか？", "確認", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
//		bFullScreen = true;
//	}

	//===================================================================
	// Direct3D初期化
	//===================================================================

	// デバイスのデバッグモードを有効にする
	bool deviceDebugMode = false;
#ifdef _DEBUG
	deviceDebugMode = true;
#endif

	// Direct3D初期化
	std::string errorMsg;
	if (KdDirect3D::Instance().Init(m_window.GetWndHandle(), w, h, deviceDebugMode, errorMsg) == false) {
		MessageBoxA(m_window.GetWndHandle(), errorMsg.c_str(), "Direct3D初期化失敗", MB_OK | MB_ICONSTOP);
		return false;
	}

	// フルスクリーン設定
	if (bFullScreen) {
		HRESULT hr;

		hr = KdDirect3D::Instance().SetFullscreenState(TRUE, 0);
		if (FAILED(hr))
		{
			MessageBoxA(m_window.GetWndHandle(), "フルスクリーン設定失敗", "Direct3D初期化失敗", MB_OK | MB_ICONSTOP);
			return false;
		}
	}

	//===================================================================
	// imgui初期化
	//===================================================================
	KdDebugGUI::Instance().GuiInit(w, h);

	//===================================================================
	// シェーダー初期化
	//===================================================================
	KdShaderManager::Instance().Init();

	//===================================================================
	// オーディオ初期化
	//===================================================================
	KdAudioManager::Instance().Init();

	//===================================================================
	// フォント初期化
	//===================================================================
	KdFontManager::Instance().Init(GetWindowHandle());

	//===================================================================
	// Effekseer初期化
	//===================================================================
	KdEffekseerManager::GetInstance().Create(w, h);

	//===================================================================
	// ゲーム固有の初期化
	//===================================================================
	// 例えばカーソルを消したい場合
	//ShowCursor(false);

	// レベルエディタで配置できるオブジェクトの登録
	LevelEditorManager::Instance().RegisterCreatable("Block", []() {
		auto obj = std::make_shared<Block>();
		obj->Init();
		return obj;
	});

	LevelEditorManager::Instance().RegisterCreatable("Ground", []() {
		auto obj = std::make_shared<Ground>();
		obj->Init();
		return obj;
	});

	LevelEditorManager::Instance().RegisterCreatable("Enemy", []() {
		auto obj = std::make_shared<Enemy>();
		obj->Init();
		return obj;
	});

	//===================================================================
	// 入力(キーボード・マウス)の登録
	// ※ ここで名前を付けて登録しておくことで、各クラスはGetAsyncKeyStateを
	//    直接呼ばずKdInputManager::Instance().IsPress/IsHold/IsRelease(名前)
	//    で判定できるようになる(キー配置の変更もここ1箇所で済む)
	//===================================================================
	{
		auto pKeyboardMouse = std::make_unique<KdInputCollector>();

		// 移動軸(WASD：上=W、右=D、下=S、左=A)
		pKeyboardMouse->AddAxis("Move", new KdInputAxisForWindows('W', 'D', 'S', 'A'));

		// EditorCameraの上下移動
		pKeyboardMouse->AddButton("MoveUp", new KdInputButtonForWindows('E'));
		pKeyboardMouse->AddButton("MoveDown", new KdInputButtonForWindows('Q'));

		// 加速(EditorCamera)
		pKeyboardMouse->AddButton("Boost", new KdInputButtonForWindows(VK_SHIFT));

		// 修飾キー
		pKeyboardMouse->AddButton("Ctrl", new KdInputButtonForWindows(VK_CONTROL));

		// マウスボタン
		pKeyboardMouse->AddButton("RightClick", new KdInputButtonForWindows(VK_RBUTTON));
		pKeyboardMouse->AddButton("LeftClick", new KdInputButtonForWindows(VK_LBUTTON));

		// レベルエディタのショートカット(Ctrlとの組み合わせは呼び出し側で判定)
		pKeyboardMouse->AddButton("Copy", new KdInputButtonForWindows('C'));
		pKeyboardMouse->AddButton("Paste", new KdInputButtonForWindows('V'));
		pKeyboardMouse->AddButton("Duplicate", new KdInputButtonForWindows('D'));
		pKeyboardMouse->AddButton("Undo", new KdInputButtonForWindows('Z'));
		pKeyboardMouse->AddButton("Redo", new KdInputButtonForWindows('Y'));

		// その他
		pKeyboardMouse->AddButton("Escape", new KdInputButtonForWindows(VK_ESCAPE));
		pKeyboardMouse->AddButton("SwitchScene", new KdInputButtonForWindows('T'));
		pKeyboardMouse->AddButton("Confirm", new KdInputButtonForWindows(VK_RETURN));

		KdInputManager::Instance().AddDevice("KeyboardMouse", pKeyboardMouse);
	}

	//===================================================================
	// シーンの初期化(開始シーンの生成。SceneManagerのコンストラクタでは行わない)
	//===================================================================
	SceneManager::Instance().Init();

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション実行
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Execute()
{
	KdCSVData windowData("Asset/Data/WindowSettings.csv");
	const std::vector<std::string>& sizeData = windowData.GetLine(0);

	//===================================================================
	// 初期設定(ウィンドウ作成、Direct3D初期化など)
	//===================================================================
	if (Application::Instance().Init(atoi(sizeData[0].c_str()), atoi(sizeData[1].c_str())) == false) {
		return;
	}

	//===================================================================
	// ゲームループ
	//===================================================================

	// 時間
	m_fpsController.Init();

	// ループ
	while (1)
	{
		// 処理開始時間Get
		m_fpsController.UpdateStartTime();

		// ゲーム終了指定があるときはループ終了
		if (m_endFlag)
		{
			break;
		}

		//=========================================
		//
		// ウィンドウ関係の処理
		//
		//=========================================

		// ウィンドウのメッセージを処理する
		m_window.ProcessMessage();

		// ウィンドウが破棄されてるならループ終了
		if (m_window.IsCreated() == false)
		{
			break;
		}

		//=========================================
		//
		// アプリケーション更新処理
		//
		//=========================================

		KdBeginUpdate();
		{
			// ※ KdInputManagerの更新(KdBeginUpdate内)より後で判定する必要がある
			if (KdInputManager::Instance().IsHold("Escape"))
			{
//				if (MessageBoxA(m_window.GetWndHandle(), "本当にゲームを終了しますか？",
//					"終了確認", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
				{
					End();
				}
			}

			PreUpdate();

			Update();

			PostUpdate();
		}
		KdPostUpdate();

		//=========================================
		//
		// アプリケーション描画処理
		//
		//=========================================

		KdBeginDraw();
		{
			PreDraw();

			Draw();

			PostDraw();

			DrawSprite();
		}
		KdPostDraw();

		//=========================================
		//
		// フレームレート制御
		//
		//=========================================

		m_fpsController.Update();

		std::string titlebar = "タイトル名 FPS:" + std::to_string(m_fpsController.m_nowfps);
		SetWindowTextA(m_window.GetWndHandle(), titlebar.c_str());
	}

	//===================================================================
	// アプリケーション解放
	//===================================================================
	Release();
}

// アプリケーション終了
void Application::Release()
{
	KdInputManager::Instance().Release();

	KdShaderManager::Instance().Release();

	KdAudioManager::Instance().Release();

	// KdDirect3Dのデバイス解放より前に、DX11リソースを持つEffekseerを解放しておく
	KdEffekseerManager::GetInstance().Release();

	KdDirect3D::Instance().Release();

	// ウィンドウ削除
	m_window.Release();
}
