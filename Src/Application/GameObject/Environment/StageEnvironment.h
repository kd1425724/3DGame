#pragma once

//====================================================
//
// StageEnvironment ── ステージの空気感(疾走感)を作る環境設定オブジェクト
//
//  ・平行光 / 環境光 / 距離フォグ / 影の生成エリア を毎フレーム適用する
//    (見た目だけを司る。描画も当たり判定も持たない)
//  ・値はすべて DebugParams「環境/…」・DebugFlags「環境/…」で実行中に調整できる
//  ・KdAmbientController(KdShaderManager経由)で空間環境の定数バッファを書き換える
//
//  ※ 描画パスには一切出ない(Draw系をoverrideしない)ロジックのみのKdGameObject。
//    GameScene::Init で AddObject して常駐させる
//
//====================================================
class StageEnvironment : public KdGameObject
{
public:

	void Init() override;
	void Update() override;

private:

	// DebugParams/DebugFlags の値を KdAmbientController に流し込む(Init/Update共通)
	void Apply();
};
