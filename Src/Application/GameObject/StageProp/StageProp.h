#pragma once

//====================================================
//
// StageProp ── ステージ配置用のモデルオブジェクト(家/建物/小物)
//
//  ・表示モデルのパスを SetModelPath で受け取り、実寸(scale=1)で表示する
//  ・当たり判定はモデル形状で登録。glTF内のノード名に "COL" を含むメッシュは
//    フレームワーク(KdModel)が自動で「当たり専用(非表示)」に仕分けるので、
//    表示モデルをそのまま渡すだけでよい(COL箱を入れれば軽い当たりになる)
//  ・LevelEditorから「<カテゴリ>/<名前>」の種類名で配置される
//    (RegisterAllToEditor が Asset/Models/Stage 配下を走査して一括登録)
//
//====================================================
class StageProp : public KdGameObject
{
public:

	StageProp()				{}
	~StageProp()	override	{}

	// 表示モデルのパスを設定する(Init前に呼ぶこと)
	void SetModelPath(const std::string& _path) { m_modelPath = _path; }

	void Init()						override;
	void DrawLit()					override;
	void GenerateDepthMapFromLight() override;   // 影を落とす側
	void DrawDebug()				override;

	// Asset/Models/Stage/{House,Building,Prop}/ を走査し、各モデルを
	// 「<カテゴリ>/<名前>」の種類名でLevelEditorに一括登録する(起動時に1回呼ぶ)
	static void RegisterAllToEditor();

private:

	// 表示モデルのパス(例 "Asset/Models/Stage/House/house01/house01.gltf")
	std::string m_modelPath;

	// 表示・当たり判定で共有するモデルワーク(Block/Groundと同じ形)
	std::shared_ptr<KdModelWork> m_spModelWork;
};
