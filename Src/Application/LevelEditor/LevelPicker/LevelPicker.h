#pragma once

//====================================================
//
// レベルエディタ：3Dビュー上でのクリック選択＆ドラッグ移動
//  ・左クリックした瞬間、以下の優先順位で判定する
//    1. 移動ギズモの軸(X/Y/Z)を掴んだか → その軸方向だけに動かす
//    2. 選択中オブジェクト本体を掴んだか → カメラに正対する平面上で自由に動かす
//    3. どちらでもなければ → 一番近いオブジェクトを選択する
//  ・当たり判定(Collider)の有無に関係なく、オブジェクトの座標だけで判定するので
//    どんな種類のオブジェクトでも(当たり判定を持っていなくても)選択・移動できる
//
// 制約：
//  ・レイキャストの起点となるカメラが必要なため、現状は
//    LevelEditorManagerの編集用フリーカメラが有効な間のみ動作する
//
//====================================================
class LevelPicker
{
public:

	static LevelPicker& Instance()
	{
		static LevelPicker instance;
		return instance;
	}

	// 毎フレームの更新(Application::KdBeginUpdateから呼ばれる)
	void Update();

	// クリックで選択できる距離のしきい値(ワールド単位)
	float m_pickRadius = 1.0f;

	// 選択中オブジェクトを「掴んだ」と判定する距離のしきい値(ワールド単位)
	float m_grabRadius = 1.0f;

private:

	LevelPicker() = default;
	~LevelPicker() = default;
	LevelPicker(const LevelPicker&) = delete;
	void operator=(const LevelPicker&) = delete;

	enum class DragMode
	{
		None,
		Axis,		// ギズモの軸に沿って移動
		FreePlane,	// カメラに正対する平面上で自由に移動
	};

	// 現在のドラッグ状態
	DragMode m_dragMode = DragMode::None;

	// ---- Axisドラッグ用 ----
	Math::Vector3	m_dragAxisDir;			// 動かす方向(単位ベクトル)
	Math::Vector3	m_dragAxisStartObjPos;	// 掴んだ瞬間のオブジェクト座標
	float			m_dragAxisStartS = 0.0f;	// 掴んだ瞬間の「軸上のどのあたりを見ていたか」

	// ---- FreePlaneドラッグ用 ----
	Math::Vector3 m_dragPlaneNormal;
	Math::Vector3 m_dragPlanePoint;
};
