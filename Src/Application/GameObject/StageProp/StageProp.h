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

	// ===== GPUインスタンシング用 =====
	// StagePropの描画は InstancedPropRenderer が「同じモデルごとにまとめて」行う。
	// そのために必要な情報を公開する(自分ではDrawLit等で描かない。下のDrawLit参照)
	const std::shared_ptr<KdModelWork>& GetModelWork() const { return m_spModelWork; }

	// カリング用のワールド境界球を返す(初回に一度モデルからローカル球を計算してキャッシュ)
	DirectX::BoundingSphere WorldBoundingSphere();

	// _pObj がStageProp(建物)で、_queryPos から _range より遠ければ true(=当たり判定を省略してよい)。
	// 建物を大量配置すると、キャラ毎フレームの接地レイ/壁球判定が全建物のCOLチャンクを
	// 舐めてCPUを食うため、判定前の粗い距離ふるい(broadphase代わり)として使う。
	// StageProp以外(地面/敵など)は常にfalse=従来どおり判定される
	static bool IsFarForCollision(KdGameObject* _pObj, const Math::Vector3& _queryPos, float _range);

private:

	// 表示モデルのパス(例 "Asset/Models/Stage/House/house01/house01.gltf")
	std::string m_modelPath;

	// 表示・当たり判定で共有するモデルワーク(Block/Groundと同じ形)
	std::shared_ptr<KdModelWork> m_spModelWork;

	// カリング用のモデル全体のローカル境界球(半径0=未計算。WorldBoundingSphereで遅延計算)
	DirectX::BoundingSphere m_localBS = { { 0.0f, 0.0f, 0.0f }, 0.0f };
};
