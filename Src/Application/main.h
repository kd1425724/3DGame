#pragma once

//============================================================
// アプリケーションクラス
//	APP.～ でどこからでもアクセス可能
//============================================================
class Application
{
// メンバ
public:

	// アプリケーション実行
	void Execute();

	// アプリケーション終了
	void End()							{ m_endFlag = true; }

	HWND GetWindowHandle()		const	{ return m_window.GetWndHandle(); }
	int GetMouseWheelValue()	const	{ return m_window.GetMouseWheelVal(); }

	int		GetNowFPS()			const	{ return m_fpsController.m_nowfps; }
	int		GetMaxFPS()			const	{ return m_fpsController.m_maxFps; }

	// 実時間デルタタイム。フレーム落ち・ウィンドウ操作・ロード等でdtが跳ねると、
	// pos += velocity*dt が一気に進んで物理が暴発(壁すり抜け/大フリング)しうる。
	// それを防ぐため上限(1/30秒=約2フレームぶん)で頭打ちにする。
	// ※ タイムスケール(スロー)は掛けない。フォーカスゲージなど"現実の時間"で測るものに使う
	float	GetRealDeltaTime()	const
	{
		constexpr float kMaxDeltaTime = 1.0f / 30.0f;
		float dt = m_fpsController.GetDeltaTime();
		return (dt > kMaxDeltaTime) ? kMaxDeltaTime : dt;
	}

	// ゲーム用デルタタイム。実時間dtにタイムスケールを掛けたもの。ゲームロジック(移動/敵/
	// エフェクト/カメラ等)はこれを使うので、SetTimeScaleで全体を一括スローにできる(空中スロー)
	float	GetDeltaTime()		const
	{
		return GetRealDeltaTime() * m_timeScale;
	}

	// タイムスケール設定(1.0=等速 / 例:0.3=スロー)。空中スロー(エアフォーカス)で使う
	void	SetTimeScale(float _scale)	{ m_timeScale = _scale; }
	float	GetTimeScale()		const	{ return m_timeScale; }
private:

	void KdBeginUpdate();
	void PreUpdate();
	void Update();
	void PostUpdate();
	void KdPostUpdate();

	void KdBeginDraw(bool usePostProcess = true);
	void PreDraw();
	void Draw();
	void PostDraw();
	void DrawSprite();
	void KdPostDraw();

	// アプリケーション初期化
	bool Init(int w, int h);

	// アプリケーション解放
	void Release();

	// ゲームウィンドウクラス
	KdWindow		m_window;

	// FPSコントローラー
	KdFPSController	m_fpsController;

	// ゲーム終了フラグ trueで終了する
	bool		m_endFlag = false;

	// タイムスケール(1.0=等速)。空中スロー中はPlayerが小さくして全体をスローにする
	float		m_timeScale = 1.0f;

//=====================================================
// シングルトンパターン
//=====================================================
private:
	// 
	Application() {}

public:
	static Application &Instance(){
		static Application Instance;
		return Instance;
	}
};
