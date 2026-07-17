#pragma once

//====================================================
//
// InstancedPropRenderer ── StagePropをGPUインスタンシングでまとめて描くレンダラ
//
//  ・シーン内のStagePropを毎フレーム「同じモデル(KdModelData)ごと」にグループ化し、
//    ワールド行列の配列を作って KdStandardShader::DrawModelInstanced を1回呼ぶ。
//    → 同じ家を100軒置いても、その家のドローは「マテリアル数ぶん」だけで済む
//      (従来は 1軒ごとに マテリアル数ぶん のドローが必要だった)
//  ・StageProp自身は描画しない(StageProp::DrawLit等は空)。ここが代わりに描く
//  ・カリングはStagePropが個別に描いていた時と同じ判定を使う
//      通常描画 … CullingManager::IsVisible (視錐台＋距離)
//      影      … CullingManager::IsInRange (距離のみ)
//  ・KdGameObject派生。シーンに1つ常駐させる(GameScene::Initで追加)
//    → BaseSceneの各描画パスから DrawLit / GenerateDepthMapFromLight が呼ばれる
//  ・DebugFlags「カリング/インスタンシング」でOFFにすると1軒ずつの通常描画に戻せる(比較用)
//
//====================================================
class InstancedPropRenderer : public KdGameObject
{
public:

	InstancedPropRenderer()				{}
	~InstancedPropRenderer()	override	{}

	// 通常描画パス(視錐台＋距離でカリングしてからまとめて描く)
	void DrawLit()						override;

	// 影(深度マップ)パス(距離のみでカリングしてからまとめて描く)
	void GenerateDepthMapFromLight()	override;

private:

	// シーンのStagePropを集めてモデルごとに一括描画する
	// _isShadowPass … true:影パス(距離のみ判定) / false:通常描画(視錐台＋距離)
	void DrawBatched(bool _isShadowPass);
};
