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

// ※ KdGameObjectはPch.hの強制インクルードで見えているため、継承でも明示includeは不要
class Targeting : public KdGameObject
{
public:

	// 板ポリ(マーカー)をunique_ptr(前方宣言)で持つため、ctor/dtorは.cpp側で定義する
	Targeting();
	~Targeting();

	// 画面中心に一番近い敵を選び、マーカーのアニメ時間を進める。Playerのcameraとdtを渡す。
	// _keepCurrent=true なら、今の対象が生きている限り選び直さない(ロックオン中)。
	// 【なぜ要るか】毎フレーム選び直すと、関節を狙っている最中に画面中央へ別の敵が
	//   入っただけで狙いが勝手に移ってしまう
	void Update(const std::shared_ptr<CameraBase>& _spCamera, float _dt, bool _keepCurrent = false);

	// マーカーを出す位置を明示的に指定する(狙っている関節の位置)。
	// 指定が無いときは従来どおり対象の少し上に出る
	void SetMarkerOverridePos(const Math::Vector3& _pos)
	{
		m_markerOverridePos = _pos;
		m_hasMarkerOverride = true;
	}
	void ClearMarkerOverridePos() { m_hasMarkerOverride = false; }

	// 選択中の関節(または敵)にマーカーを描く。
	// 【2D描画パスから呼ぶ】ワールド座標をスクリーン座標へ変換して2Dで描く。
	// 3Dの板ポリで描いていた頃は、狙う関節(首・肘・膝)が体の【内側】にあるため
	// 深度テストで必ずモデルに埋まって見えなかった
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

	// マーカーの見た目(照準テクスチャ)。2Dで描くので板ポリは要らない
	std::shared_ptr<KdTexture> m_spMarkerTex;

	// 座標変換に使うカメラ(Updateで受け取ったものを覚えておく)
	std::weak_ptr<CameraBase> m_wpCamera;

	// マーカーの回転/脈動アニメ用の経過時間
	float m_time = 0.0f;

	// マーカーの位置を外から指定するとき(狙っている関節の位置)に使う
	Math::Vector3 m_markerOverridePos = {};
	bool          m_hasMarkerOverride = false;
};
