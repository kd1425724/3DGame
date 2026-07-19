#pragma once

//====================================================
//
// Targeting ── 画面中心の敵を自動ロックオンし、マーカーを描く部品
//
//  ・KdGameObject派生。ただしシーンのオブジェクトリストには入れず、Playerが「部品」として
//    所有し、親がUpdate/DrawMarkerを直接呼んで駆動する
//    (KdGameObjectを継承するのは「オブジェクトは全てKdGameObject」方針に合わせるため)
//  ・カメラのフルの向き(画面中心)に一番よく揃う敵を毎フレーム選ぶ(カメラは回さない)
//  ・選んだ敵の少し上に、カメラを向くビルボードのマーカーを回転＋脈動で描く
//  ・GetTarget()で現在の対象を返す(落下攻撃の突撃先に使う)
//
//====================================================
class CameraBase;
class KdSquarePolygon;

// ※ KdGameObjectはPch.hの強制インクルードで見えているため、継承でも明示includeは不要
class Targeting : public KdGameObject
{
public:

	// 板ポリ(マーカー)をunique_ptr(前方宣言)で持つため、ctor/dtorは.cpp側で定義する
	Targeting();
	~Targeting();

	// 画面中心に一番近い敵を選び、マーカーのアニメ時間を進める。Playerのcameraとdtを渡す
	void Update(const std::shared_ptr<CameraBase>& _spCamera, float _dt);

	// 選択中の敵にマーカー(カメラを向く板ポリ)を描く。陰影なしパスから呼ぶ
	void DrawMarker();

	// 現在のターゲット(いなければ空)。落下攻撃の突撃先に使う
	std::shared_ptr<KdGameObject> GetTarget() const { return m_wpTarget.lock(); }

private:

	// 選択中の対象(画面中心に一番近い「見えている」敵)
	std::weak_ptr<KdGameObject> m_wpTarget;

	// 候補1件 = <画面中心への近さ(内積), 敵>
	using Candidate = std::pair<float, std::shared_ptr<KdGameObject>>;

	// 角度内の候補を集めて内積の降順に並べる作業用。毎フレームclearして使い回す
	// (毎フレームvectorを確保し直さないためメンバに持つ)
	std::vector<Candidate> m_candidates;

	// マーカーの見た目(照準テクスチャ・カメラを向く点ビルボード)
	std::unique_ptr<KdSquarePolygon> m_upMarkerPoly;

	// マーカーの回転/脈動アニメ用の経過時間
	float m_time = 0.0f;
};
