#pragma once

//====================================================
//
// SkySphere ── 背景用の天球(スカイスフィア)
//
//  ・内側に空テクスチャを貼った球を、カメラの周りに大きく描く背景オブジェクト
//  ・毎フレーム カメラ位置へ球を移動させる=どれだけ動いても空に近づけない(無限遠に見せる)
//  ・DrawUnLit パスで描く。平行光・影・フォグの影響を受けず、常に一定の明るさで出る
//    (パスの最初に描かれるので、建物やキャラはこの上に重なる=背景になる)
//  ・当たり判定は持たない(見た目だけ)。カリングもしない(常に見えるべきもの)
//
//  ・モデル(Sky.gltf)は法線を内向きに反転済み・球状UV付き。半径1の単位球なので、
//    ここで SetScale して十分大きく(farクリップ2000の内側)する
//
//  ※ GameScene::Init で最初に AddObject して常駐させる
//
//====================================================
class SkySphere : public KdGameObject
{
public:

	SkySphere()				{}
	~SkySphere()	override	{}

	void Init()				override;

	// カメラ位置へ球を追従させる(描画の直前に確定させたいので PostUpdate ではなくここで)
	void Update()			override;

	// 空はライト・影の影響を受けない背景なので UnLit パスで描く
	void DrawUnLit()		override;

private:

	std::shared_ptr<KdModelWork> m_spModelWork;

	// 球の半径(m)。街(±150)より充分外側かつ far クリップ(2000)の内側
	float m_radius = 900.0f;
};
