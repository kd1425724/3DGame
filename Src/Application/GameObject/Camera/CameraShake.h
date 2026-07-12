#pragma once

//====================================================
//
// CameraShake ── カメラの手応え(着地・壁ヒット等の衝撃)を揺れで表現するシングルトン
//
//  ・AddTrauma()で衝撃を溜め、時間で減衰する(trauma方式)
//  ・GetOffset()が現在の揺れオフセット(視点ローカル: x=右/y=上)を返す
//  ・揺れの強さ/減衰はDebugParams「カメラ/シェイク…」で調整
//  ・発火はゲーム側(Player)から。カメラ(TPSCamera)がUpdate/GetOffsetを使う
//
//====================================================
class CameraShake
{
public:

	static CameraShake& Instance()
	{
		static CameraShake instance;
		return instance;
	}

	// 衝撃を加える(0〜1で累積。大きいほど強く揺れる)
	void AddTrauma(float _amount);

	// 毎フレーム更新(時間を進め、traumaを減衰)。TPSCameraから呼ぶ
	void Update(float _dt);

	// 現在の揺れオフセット(視点ローカル: x=右, y=上)
	Math::Vector3 GetOffset() const;

private:

	CameraShake() = default;
	~CameraShake() = default;
	CameraShake(const CameraShake&) = delete;
	void operator=(const CameraShake&) = delete;

	float m_trauma = 0.0f;	// 現在の衝撃(0〜1)
	float m_time = 0.0f;	// 揺れ用の経過時間
};
