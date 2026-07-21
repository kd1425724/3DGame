#pragma once
//============================================================
//
// 基本シェーダー
//
//============================================================
class KdStandardShader
{
public:
	// スキンメッシュ対応
	static const int maxBoneBufferSize = 300;

	// 定数バッファ(オブジェクト単位更新)
	struct cbObject
	{
		// UV操作
		Math::Vector2	UVOffset = { 0.0f, 0.0f };
		Math::Vector2	UVTiling = { 1.0f, 1.0f };

		// フォグ有効
		int				FogEnable = 1;

		// エミッシブのみの描画
		int				OnlyEmissie = 0;

		// スキンメッシュオブジェクトかどうか(スキンメッシュ対応)
		int				IsSkinMeshObj = 0;

		// ディゾルブ関連
		float			DissolveThreshold = 0.0f;	// 0 ～ 1
		float			DissolveEdgeRange = 0.03f;	// 0 ～ 1

		Math::Vector3	DissolveEmissive = { 0.0f, 1.0f, 1.0f };
	};

	// 定数バッファ(メッシュ単位更新)
	struct cbMesh
	{
		Math::Matrix	mW;
	};

	// 定数バッファ(マテリアル単位更新)
	struct cbMaterial
	{
		Math::Vector4	BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		Math::Vector3	Emissive = { 1.0f, 1.0f, 1.0f };
		float			Metallic = 0.0f;

		float			Roughness = 1.0f;
		float			_blank[3] = { 0.0f, 0.0f ,0.0f };
	};

	// 定数バッファ(ボーン単位更新：スキンメッシュ対応)
	struct cbBone {
		Math::Matrix mBones[300];
	};

	//================================================
	// 設定・取得
	//================================================

	// UVタイリング設定
	void SetUVTiling(const Math::Vector2& tiling)
	{
		m_cb0_Obj.Work().UVTiling = tiling;

		m_dirtyCBObj = true;
	}

	// UVオフセット設定
	void SetUVOffset(const Math::Vector2& offset)
	{
		m_cb0_Obj.Work().UVOffset = offset;

		m_dirtyCBObj = true;
	}

	// フォグ有効/無効
	void SetFogEnable(bool enable)
	{
		m_cb0_Obj.Work().FogEnable = enable;

		m_dirtyCBObj = true;
	}

	// ディゾルブ設定
	void SetDissolve(float threshold, const float* range = nullptr, const Math::Vector3* edgeColor = nullptr)
	{
		auto& cbObj = m_cb0_Obj.Work();

		cbObj.DissolveThreshold = threshold;

		if (range)
		{
			cbObj.DissolveEdgeRange = *range;
		}

		if (edgeColor)
		{
			cbObj.DissolveEmissive = *edgeColor;
		}

		m_dirtyCBObj = true;
	}

	// ディゾルブテクスチャ設定
	void SetDissolveTexture(KdTexture& dissolveMask)
	{
		KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(11, 1, dissolveMask.WorkSRViewAddress());
	}

	// デフォルトディゾルブテクスチャ設定
	void SetDefaultDissolveTexture(std::shared_ptr<KdTexture>& spDissolveMask)
	{
		if (!spDissolveMask) { return; }

		m_dissolveTex = spDissolveMask;

		SetDissolveTexture(*spDissolveMask);
	}

	// デフォルトのディゾルブテクスチャに戻す
	void ResetDissolveTexture()
	{
		if (!m_dissolveTex) { return; }

		SetDissolveTexture(*m_dissolveTex);
	}

	//================================================
	// 各定数バッファの取得
	//================================================
	const cbObject& GetObjectCB() const { return m_cb0_Obj.Get(); }

	const cbMesh& MeshCB() const { return m_cb1_Mesh.Get(); }

	const cbMaterial& WorkMaterialCB() const { return m_cb2_Material.Get(); }

	// スキンメッシュ対応
	const cbBone& WorkBoneCB() const { return m_cb3_Bone.Get(); }

	//================================================
	// 描画準備
	//================================================
	// 陰影をつけるオブジェクト等を描画する前後に行う
	void BeginLit();
	void EndLit();

	// 陰影をつけないオブジェクト等を描画する前後に行う
	void BeginUnLit();
	void EndUnLit();

	// 最も初めに行う、光を遮るオブジェクトを描画する前後に行う
	void BeginGenerateDepthMapFromLight();
	void EndGenerateDepthMapFromLight();

	//================================================
	// 描画関数
	//================================================
	// メッシュ描画
	void DrawMesh(const KdMesh* mesh, const Math::Matrix& mWorld, const std::vector<KdMaterial>& materials,
		const Math::Vector4& col, const Math::Vector3& emissive);

	// モデルデータ描画：アニメーションに非対応
	void DrawModel(const KdModelData& rModel, const Math::Matrix& mWorld = Math::Matrix::Identity, 
		const Math::Color& colRate = kWhiteColor, const Math::Vector3& emissive = Math::Vector3::Zero);

	// モデルワーク描画：アニメーションに対応
	void DrawModel(KdModelWork& rModel, const Math::Matrix& mWorld = Math::Matrix::Identity,
		const Math::Color& colRate = kWhiteColor, const Math::Vector3& emissive = Math::Vector3::Zero);

	// ===== 追加(GPUインスタンシング) =====
	// 同じモデルを worldList の数だけ、まとめて1回のドローで描く(静的プロップの大量配置用)。
	// 通常のDrawModelは1オブジェクト1ドローだが、こちらはワールド行列をインスタンスバッファ
	// (頂点slot1)で渡すため、100個置いても「マテリアル数ぶん」のドローで済む。
	// ※ BeginLit / BeginGenerateDepthMapFromLight の間で呼ぶこと(パスに応じて専用VSへ自動で切替、描画後に元へ戻す)
	// ※ スキンメッシュ・UnLitパスは非対応(その場合は何もしないので、呼び出し側で通常のDrawModelを使うこと)
	void DrawModelInstanced(KdModelWork& rModel, const std::vector<Math::Matrix>& worldList,
		const Math::Color& colRate = kWhiteColor, const Math::Vector3& emissive = Math::Vector3::Zero);

	// 任意の頂点群からなるポリゴン描画
	void DrawPolygon(const KdPolygon& poly, const Math::Matrix& mWorld = Math::Matrix::Identity,
		const Math::Color& colRate = kWhiteColor, const Math::Vector3& emissive = Math::Vector3::Zero);

	// 任意の頂点群からなるポリゴンライン描画
	void DrawVertices(const std::vector<KdPolygon::Vertex>& vertices, const Math::Matrix& mWorld = Math::Matrix::Identity,
		const Math::Color& colRate = kWhiteColor);

	//================================================
	// 初期化・解放
	//================================================

	// 初期化
	bool Init();
	// 解放
	void Release();

	~KdStandardShader()
	{
		Release();
	}

	std::shared_ptr<KdTexture>& GetDepthTex() { return m_depthMapFromLightRTPack.m_RTTexture; }

	// ===== 追加(シャドウマップ解像度の実行時変更) =====
	// 影の深度マップを作り直して解像度を変える。値が変わった時だけ実際に作り直す。
	// 1テクセルの実サイズ = 影エリア ÷ 解像度 なので、影の細かさはこの2つで決まる。
	// VRAMは 解像度^2 x 4byte x 2枚(レンダーターゲット R32_FLOAT ＋ 深度ステンシル R24G8)。
	//   1024 -> 8MB / 2048 -> 32MB / 4096 -> 128MB
	// ※ 描画中(深度マップがバインドされている間)に呼ばないこと。Updateフェーズから呼ぶ
	void SetShadowMapSize(int _size);
	int  GetShadowMapSize() const { return m_shadowMapSize; }

private:

	// マテリアルのセット
	void WriteMaterial(const KdMaterial& material, const Math::Vector4& colRate, const Math::Vector3& emiRate);

	// ===== 追加(GPUインスタンシング) =====
	// インスタンス行列をGPUの動的頂点バッファへ書き込む(足りなければ作り直して拡張する)
	bool WriteInstanceBuffer(const std::vector<Math::Matrix>& mats);
	// DrawMeshのインスタンシング版(頂点slot0＋インスタンスslot1をバインドしてDrawIndexedInstanced)
	void DrawMeshInstanced(const KdMesh* mesh, const std::vector<KdMaterial>& materials,
		const Math::Vector4& col, const Math::Vector3& emissive, UINT instanceCount);

	// ポリゴンの法線情報を2Dように書き換える
	void ConvertNormalsFor2D(std::vector<KdPolygon::Vertex>& target, const Math::Matrix& mWorld);

	// 定数バッファを初期状態に戻す
	void ResetCBObject();

	// スキンメッシュ有効かどうか(スキンメッシュ対応)
	void SetIsSkinMeshObj(bool isSkinMEshObj)
	{
		if (m_cb0_Obj.Work().IsSkinMeshObj != (int)isSkinMEshObj)
		{
			m_cb0_Obj.Work().IsSkinMeshObj = isSkinMEshObj;
			m_dirtyCBObj = true;
		}
	}

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// Lit：陰影をつけるオブジェクトの描画用（不透明な物体やキャラクタの板ポリなど
	// 平行光・点光源などの影響を受け角度によって色を変化させるオブジェクトを描画するシェーダー
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// UnLit：陰影のつかないオブジェクトの描画用（エフェクトや透明な物体など
	// 光の計算を行わずマテリアルの色をそのまま出力するシェーダー
	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// GenDepthFromLight：光から見たオブジェクトの距離を赤で出力用
	// Litシェーダーで影の描画を行うために必要な情報テクスチャを作成するシェーダー
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

	// 頂点シェーダー
	ID3D11VertexShader* m_VS_Lit = nullptr;					// 陰影あり
	ID3D11VertexShader* m_VS_UnLit = nullptr;				// 陰影なし
	ID3D11VertexShader* m_VS_GenDepthFromLight = nullptr;	// 光からの深度

	// 頂点入力レイアウト
	ID3D11InputLayout* m_inputLayout = nullptr;

	// ===== 追加(GPUインスタンシング) =====
	// 通常版との違いは「ワールド行列を定数バッファではなく頂点slot1のインスタンスデータで受ける」点。
	// そのため専用のVSと、per-instance要素を含む入力レイアウトが要る。既存の物は消していない。
	ID3D11VertexShader* m_VS_Lit_Inst = nullptr;				// 陰影あり(インスタンシング版)
	ID3D11VertexShader* m_VS_GenDepthFromLight_Inst = nullptr;	// 光からの深度(インスタンシング版)
	ID3D11InputLayout*	m_inputLayout_Inst = nullptr;			// 頂点(slot0)＋インスタンス行列(slot1)
	ID3D11Buffer*		m_instanceBuf = nullptr;				// インスタンス行列を入れる動的頂点バッファ
	UINT				m_instanceCapacity = 0;					// 上のバッファが確保済みのインスタンス数

	// 現在の描画パス。インスタンス描画時にどの専用VSを使うか判断するために Begin* で記録する
	enum class Pass
	{
		None,
		Lit,
		UnLit,
		GenDepth,
	};
	Pass m_curPass = Pass::None;
	
	// ピクセルシェーダー
	ID3D11PixelShader* m_PS_Lit = nullptr;					// 陰影あり
	ID3D11PixelShader* m_PS_UnLit = nullptr;				// 陰影なし
	ID3D11PixelShader* m_PS_GenDepthFromLight = nullptr;	// 光からの深度

	// テクスチャ
	std::shared_ptr<KdTexture>	m_dissolveTex = nullptr;	// ディゾルブで使用するデフォルトテクスチャ

	// 定数バッファ
	KdConstantBuffer<cbObject>		m_cb0_Obj;				// オブジェクト単位で更新
	KdConstantBuffer<cbMesh>		m_cb1_Mesh;				// メッシュ毎に更新
	KdConstantBuffer<cbMaterial>	m_cb2_Material;			// マテリアル毎に更新
	KdConstantBuffer<cbBone>		m_cb3_Bone;				// ボーン事に更新(スキンメッシュ対応「)

	KdRenderTargetPack	m_depthMapFromLightRTPack;
	KdRenderTargetChanger m_depthMapFromLightRTChanger;

	// 現在のシャドウマップ解像度(SetShadowMapSizeで変更。Initで初期値をセットする)
	int			m_shadowMapSize = 0;

	bool		m_dirtyCBObj = false;						// 定数バッファのオブジェクトに変更があったかどうか
};
