#include "Framework/KdFramework.h"

#include "KdStandardShader.h"


//================================================
// 描画準備
//================================================

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 陰影をつけるオブジェクトの描画の直前処理（不透明な物体やキャラクタの板ポリゴン）
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シェーダーのパイプライン変更
// LitShaderで使用するリソースのバッファー設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::BeginLit()
{
	// 追加(GPUインスタンシング)：現在のパスを記録する。DrawModelInstancedがどの専用VSを使うか判断に使う
	m_curPass = Pass::Lit;

	// 頂点シェーダーのパイプライン変更
	if (KdShaderManager::Instance().SetVertexShader(m_VS_Lit))
	{
		KdShaderManager::Instance().SetInputLayout(m_inputLayout);

		KdShaderManager::Instance().SetVSConstantBuffer(0, m_cb0_Obj.GetAddress());
		KdShaderManager::Instance().SetVSConstantBuffer(1, m_cb1_Mesh.GetAddress());
	}

	// ピクセルシェーダーのパイプライン変更
	if (KdShaderManager::Instance().SetPixelShader(m_PS_Lit))
	{
		KdShaderManager::Instance().SetPSConstantBuffer(0, m_cb0_Obj.GetAddress());
		KdShaderManager::Instance().SetPSConstantBuffer(2, m_cb2_Material.GetAddress());
	}

	// ボーン情報をセット(スキンメッシュ対応)
	KdShaderManager::Instance().SetVSConstantBuffer(3, m_cb3_Bone.GetAddress());

	// シャドウマップのテクスチャをセット
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(10, 1, m_depthMapFromLightRTPack.m_RTTexture->WorkSRViewAddress());

	// 通常テクスチャ用サンプラーのセット
	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Anisotropic_Wrap, 0);

	// 影ぼかし用の比較機能付きサンプラーのセット
	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp_Cmp, 1);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 陰影ありオブジェクトの描画修了
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 影を書き込む用に使用していたGenDepthFromLightで生成した深度SRVの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::EndLit()
{
	ID3D11ShaderResourceView* pNullSRV = nullptr;
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(10, 1, &pNullSRV);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 陰影をつけないオブジェクトの描画の直前処理（エフェクトや半透明物）
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シェーダーのパイプライン変更
// UnLitShaderで使用するリソースのバッファー設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::BeginUnLit()
{
	// 追加(GPUインスタンシング)：現在のパスを記録する(UnLitはインスタンシング非対応)
	m_curPass = Pass::UnLit;

	if (KdShaderManager::Instance().SetVertexShader(m_VS_UnLit))
	{
		KdShaderManager::Instance().SetInputLayout(m_inputLayout);

		KdShaderManager::Instance().SetVSConstantBuffer(0, m_cb0_Obj.GetAddress());
		KdShaderManager::Instance().SetVSConstantBuffer(1, m_cb1_Mesh.GetAddress());
	}

	if (KdShaderManager::Instance().SetPixelShader(m_PS_UnLit))
	{
		KdShaderManager::Instance().SetPSConstantBuffer(0, m_cb0_Obj.GetAddress());
		KdShaderManager::Instance().SetPSConstantBuffer(2, m_cb2_Material.GetAddress());
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 陰影なしオブジェクトの描画終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::EndUnLit()
{
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 影を生み出すオブジェクトの情報描画（光を遮る物体）
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シェーダーのパイプライン変更
// GenDepthMapFromLightShaderで使用するリソースのバッファー設定
// 書き込むテクスチャーを深度用の赤一色のテクスチャに切り替え
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::BeginGenerateDepthMapFromLight()
{
	// 追加(GPUインスタンシング)：現在のパスを記録する。DrawModelInstancedがどの専用VSを使うか判断に使う
	m_curPass = Pass::GenDepth;

	if (KdShaderManager::Instance().SetVertexShader(m_VS_GenDepthFromLight))
	{
		KdShaderManager::Instance().SetInputLayout(m_inputLayout);

		KdShaderManager::Instance().SetVSConstantBuffer(0, m_cb0_Obj.GetAddress());
		KdShaderManager::Instance().SetVSConstantBuffer(1, m_cb1_Mesh.GetAddress());
	}

	// ボーン情報をセット(スキンメッシュ対応)
	KdShaderManager::Instance().SetVSConstantBuffer(3, m_cb3_Bone.GetAddress());

	if (KdShaderManager::Instance().SetPixelShader(m_PS_GenDepthFromLight))
	{
		KdShaderManager::Instance().SetPSConstantBuffer(0, m_cb0_Obj.GetAddress());
	}

	m_depthMapFromLightRTPack.ClearTexture(kRedColor);
	m_depthMapFromLightRTChanger.ChangeRenderTarget(m_depthMapFromLightRTPack);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 影を生み出すオブジェクトの描画終了
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// レンダーターゲットを元に戻す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::EndGenerateDepthMapFromLight()
{
	m_depthMapFromLightRTChanger.UndoRenderTarget();
}


//================================================
// 描画関数
//================================================

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// メッシュを描画
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// メッシュの頂点データや3Dワールド情報・マテリアル情報をシェーダー(GPU)に転送する
// サブセットごとに描画命令を呼び出す：サブセットの個数分処理が重くなる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::DrawMesh(const KdMesh* mesh, const Math::Matrix& mWorld,
	const std::vector<KdMaterial>& materials, const Math::Vector4& colRate, const Math::Vector3& emissive)
{
	if (mesh == nullptr) { return; }

	// メッシュの頂点情報転送
	mesh->SetToDevice();

	// 3Dワールド行列転送
	m_cb1_Mesh.Work().mW = mWorld;
	m_cb1_Mesh.Write();

	// 全サブセット
	for (UINT subi = 0; subi < mesh->GetSubsets().size(); subi++)
	{
		// 面が１枚も無い場合はスキップ
		if (mesh->GetSubsets()[subi].FaceCount == 0)continue;

		// マテリアルデータの転送
		const KdMaterial& material = materials[mesh->GetSubsets()[subi].MaterialNo];
		WriteMaterial(material, colRate, emissive);

		//-----------------------
		// サブセット描画
		//-----------------------
		mesh->DrawSubset(subi);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// モデルデータを描画（スタティック(アニメーションをしない)なモデル専用
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// データに所属する全ての描画用メッシュを描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::DrawModel(const KdModelData& rModel, const Math::Matrix& mWorld,
	const Math::Color& colRate, const Math::Vector3& emissive)
{
	// オブジェクト単位の情報転送
	if (m_dirtyCBObj)
	{
		m_cb0_Obj.Write();
	}

	auto& dataNodes = rModel.GetOriginalNodes();

	// 全描画用メッシュノードを描画
	for (auto& nodeIdx : rModel.GetDrawMeshNodeIndices())
	{
		// 描画
		DrawMesh(dataNodes[nodeIdx].m_spMesh.get(), dataNodes[nodeIdx].m_worldTransform * mWorld, 
			rModel.GetMaterials(), colRate, emissive);
	}

	// 定数に変更があった場合は自動的に初期状態に戻す
	if(m_dirtyCBObj)
	{
		ResetCBObject();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// モデルワークを描画（ダイナミック(アニメーションをする)なモデルに対応
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// データに所属する全ての描画用メッシュをワークの3D行列に従って描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::DrawModel(KdModelWork& rModel, const Math::Matrix& mWorld,
	const Math::Color& colRate, const Math::Vector3& emissive)
{
	if (!rModel.IsEnable()) { return; }

	const std::shared_ptr<KdModelData>& data = rModel.GetData();

	// データがないときはスキップ
	if (data == nullptr) { return; }

	if (rModel.NeedCalcNodeMatrices())
	{
		rModel.CalcNodeMatrices();
	}

	// オブジェクト単位の情報転送(スキンメッシュ対応)
	SetIsSkinMeshObj(data->IsSkinMesh());
	if (m_dirtyCBObj)
	{
		m_cb0_Obj.Write();
	}

	auto& workNodes = rModel.GetNodes();
	auto& dataNodes = data->GetOriginalNodes();

	// スキンメッシュモデルの場合：ボーン情報を書き込み(スキンメッシュ対応)
	if (data->IsSkinMesh())
	{
		// ノード内からボーン情報を取得
		for (auto&& nodeIdx : data->GetBoneNodeIndices())
		{
			if (nodeIdx >= KdStandardShader::maxBoneBufferSize) { assert(0 && "転送できるボーンの上限数を超えました"); return; }

			auto& dataNode = dataNodes[nodeIdx];
			auto& workNode = workNodes[nodeIdx];

			// ボーン情報からGPUに渡す行列の計算
			m_cb3_Bone.Work().mBones[dataNode.m_boneIndex] = dataNode.m_boneInverseWorldMatrix * workNode.m_worldTransform;

			m_cb3_Bone.Write();
		}
	}
	

	// 全描画用メッシュノードを描画
	for (auto& nodeIdx : data->GetDrawMeshNodeIndices())
	{
		// 描画
		DrawMesh(dataNodes[nodeIdx].m_spMesh.get(), workNodes[nodeIdx].m_worldTransform * mWorld,
			data->GetMaterials(), colRate, emissive);
	}

	// 定数に変更があった場合は自動的に初期状態に戻す
	if (m_dirtyCBObj)
	{
		ResetCBObject();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 追加(GPUインスタンシング)
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 通常のDrawModel/DrawMeshは「1オブジェクト＝1ドロー(＋ワールド行列の定数バッファ更新)」。
// 静的プロップを大量配置するとこのドローコール数がCPUのボトルネックになるため、
// ワールド行列をインスタンスバッファ(頂点slot1)へまとめて入れ、DrawIndexedInstancedで
// 同じモデルを一括描画できるようにした。
// ※ 既存のDrawModel/DrawMeshは一切変更していない(キャラ等は従来経路のまま使う)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

// インスタンス行列をGPUの動的頂点バッファへ書き込む。容量が足りなければ作り直して拡張する
bool KdStandardShader::WriteInstanceBuffer(const std::vector<Math::Matrix>& mats)
{
	if (mats.empty()) { return false; }

	const UINT need = static_cast<UINT>(mats.size());

	// 容量不足のときだけ作り直す(毎フレーム作らないようにするため、縮小はしない)
	if (m_instanceBuf == nullptr || m_instanceCapacity < need)
	{
		KdSafeRelease(m_instanceBuf);
		m_instanceCapacity = 0;

		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(Math::Matrix) * need;
		desc.Usage = D3D11_USAGE_DYNAMIC;			// 毎フレームCPUから書き換える
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// 頂点ストリーム(slot1)として使う
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateBuffer(&desc, nullptr, &m_instanceBuf)))
		{
			assert(0 && "インスタンスバッファ作成失敗");
			return false;
		}

		m_instanceCapacity = need;
	}

	// 行列をそのまま流し込む。Math::Matrixはrow-majorのXMFLOAT4X4なので、シェーダ側の
	// float4x4(mtx0..mtx3) が「各float4=1行」として通常版のg_mWorldと同じ向きに組み上がる
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(KdDirect3D::Instance().WorkDevContext()->Map(m_instanceBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		return false;
	}

	memcpy(mapped.pData, mats.data(), sizeof(Math::Matrix) * need);
	KdDirect3D::Instance().WorkDevContext()->Unmap(m_instanceBuf, 0);

	return true;
}

// DrawMeshのインスタンシング版。頂点(slot0)とインスタンス行列(slot1)をバインドして一括描画する
void KdStandardShader::DrawMeshInstanced(const KdMesh* mesh, const std::vector<KdMaterial>& materials,
	const Math::Vector4& col, const Math::Vector3& emissive, UINT instanceCount)
{
	if (mesh == nullptr || instanceCount == 0) { return; }

	// 頂点バッファ(slot0)・インデックスバッファ・トポロジをセット
	mesh->SetToDevice();

	// インスタンス行列バッファ(slot1)をセット
	UINT stride = sizeof(Math::Matrix);
	UINT offset = 0;
	KdDirect3D::Instance().WorkDevContext()->IASetVertexBuffers(1, 1, &m_instanceBuf, &stride, &offset);

	// サブセット(=マテリアル)ごとに描画。1サブセットにつき1ドローで全インスタンスを描く
	for (UINT subi = 0; subi < mesh->GetSubsets().size(); subi++)
	{
		// 面が無いサブセットはスキップ
		if (mesh->GetSubsets()[subi].FaceCount == 0) { continue; }

		// マテリアルセット(通常版DrawMeshと同じ扱いにする)
		WriteMaterial(materials[mesh->GetSubsets()[subi].MaterialNo], col, emissive);

		mesh->DrawSubsetInstanced(subi, instanceCount);
	}

	// 後続の通常描画がslot1のインスタンスストリームを引きずらないよう外しておく
	ID3D11Buffer* pNullBuf = nullptr;
	UINT zero = 0;
	KdDirect3D::Instance().WorkDevContext()->IASetVertexBuffers(1, 1, &pNullBuf, &zero, &zero);
}

// 同じモデルを worldList の数だけ、まとめて1回のドローで描く
void KdStandardShader::DrawModelInstanced(KdModelWork& rModel, const std::vector<Math::Matrix>& worldList,
	const Math::Color& colRate, const Math::Vector3& emissive)
{
	if (!rModel.IsEnable() || worldList.empty()) { return; }

	const std::shared_ptr<KdModelData>& data = rModel.GetData();
	if (data == nullptr) { return; }

	// スキンメッシュはインスタンシング非対応(静的プロップ用)。呼び出し側で通常のDrawModelを使うこと
	if (data->IsSkinMesh()) { return; }

	// 今のパスに応じて専用VSを選ぶ。対応外(UnLit等)なら何もしない
	ID3D11VertexShader* pInstVS = nullptr;
	ID3D11VertexShader* pNormalVS = nullptr;
	if (m_curPass == Pass::Lit)
	{
		pInstVS = m_VS_Lit_Inst;
		pNormalVS = m_VS_Lit;
	}
	else if (m_curPass == Pass::GenDepth)
	{
		pInstVS = m_VS_GenDepthFromLight_Inst;
		pNormalVS = m_VS_GenDepthFromLight;
	}
	if (pInstVS == nullptr || m_inputLayout_Inst == nullptr) { return; }

	if (rModel.NeedCalcNodeMatrices())
	{
		rModel.CalcNodeMatrices();
	}

	// オブジェクト単位の定数(UV/フォグ等)は通常描画と同じものを使う
	SetIsSkinMeshObj(false);
	if (m_dirtyCBObj)
	{
		m_cb0_Obj.Write();
	}

	// インスタンシング用のVS・入力レイアウトへ切り替える
	KdShaderManager::Instance().SetVertexShader(pInstVS);
	KdShaderManager::Instance().SetInputLayout(m_inputLayout_Inst);

	const std::vector<KdModelData::Node>& dataNodes = data->GetOriginalNodes();
	const std::vector<KdModelWork::Node>& workNodes = rModel.GetNodes();

	std::vector<Math::Matrix> mats;
	mats.reserve(worldList.size());

	for (auto& nodeIdx : data->GetDrawMeshNodeIndices())
	{
		if (nodeIdx < 0 || nodeIdx >= (int)dataNodes.size()) { continue; }

		const KdMesh* pMesh = dataNodes[nodeIdx].m_spMesh.get();
		if (pMesh == nullptr) { continue; }

		// ノードのモデル内行列を各インスタンスのワールド行列に掛けてから渡す
		// (通常版DrawModelの workNodes[nodeIdx].m_worldTransform * mWorld と同じ計算)
		const Math::Matrix& nodeMat = workNodes[nodeIdx].m_worldTransform;
		mats.clear();
		for (const Math::Matrix& w : worldList)
		{
			mats.push_back(nodeMat * w);
		}

		if (!WriteInstanceBuffer(mats)) { continue; }

		DrawMeshInstanced(pMesh, data->GetMaterials(), colRate, emissive, static_cast<UINT>(mats.size()));
	}

	// 通常描画に戻す(この後にキャラ等の非インスタンス描画が来ても壊れないように)
	KdShaderManager::Instance().SetVertexShader(pNormalVS);
	KdShaderManager::Instance().SetInputLayout(m_inputLayout);

	// 定数に変更があった場合は自動的に初期状態に戻す(通常版DrawModelと同じ)
	if (m_dirtyCBObj)
	{
		ResetCBObject();
	}
}

// ビルボード用にworld行列をカメラ基準で作り直す（追加 2026/07/14）
// ・板ポリはスケールを頂点側に持ち、world行列は純粋な回転＋平行移動なので、
//   回転部分だけカメラで置き換えれば正対/軸固定のビルボードになる
// ・eNoneは何もせずそのまま返す（従来のDrawPolygon呼び出しは挙動不変）
static Math::Matrix ApplyBillboardToWorld(const KdPolygon& rPolygon, const Math::Matrix& mWorld)
{
	KdPolygon::BillboardMode mode = rPolygon.GetBillboardMode();
	if (mode == KdPolygon::BillboardMode::eNone) { return mWorld; }

	// カメラのワールド行列（ビュー行列の逆行列）
	Math::Matrix mCam = KdShaderManager::Instance().GetCameraCB().mView.Invert();
	Math::Vector3 pos = mWorld.Translation();

	if (mode == KdPolygon::BillboardMode::eScreen)
	{
		// 点ビルボード：カメラ回転へ差し替え。world側の回転(spin等)は面内の先回転として温存
		Math::Matrix rotOnly = mWorld;
		rotOnly.Translation(Math::Vector3::Zero);
		Math::Matrix camRot = mCam;
		camRot.Translation(Math::Vector3::Zero);

		Math::Matrix result = rotOnly * camRot;
		result.Translation(pos);
		return result;
	}

	// eAxis：軸固定ビルボード（world.Up()を軸として保持し、軸まわりだけカメラを向く）
	Math::Vector3 axis = mWorld.Up();
	axis.Normalize();

	Math::Vector3 toCam = mCam.Translation() - pos;
	Math::Vector3 side = axis.Cross(toCam);
	if (side.LengthSquared() < 1e-6f) { side = Math::Vector3::Right; }	// 軸と視線が平行な時の保険
	side.Normalize();
	Math::Vector3 normal = side.Cross(axis);

	Math::Matrix result = Math::Matrix::Identity;
	result.Right(side);
	result.Up(axis);
	result.Backward(normal);
	result.Translation(pos);
	return result;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ポリゴンを描画（モデル以外のプログラム上で生成された頂点の集合体
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// データに所属する全ての描画用メッシュをワークの3D行列に従って描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::DrawPolygon(const KdPolygon& rPolygon, const Math::Matrix& mWorld,
	const Math::Color& colRate, const Math::Vector3& emissive)
{
	if (!rPolygon.IsEnable()) { return; }

	// ビルボード指定があればworld行列をカメラ基準に作り直す（追加 2026/07/14）
	Math::Matrix mDrawWorld = ApplyBillboardToWorld(rPolygon, mWorld);

	// ポリゴン描画用の頂点取得
	auto& vertices = rPolygon.GetVertices();

	// 頂点数が3より少なければポリゴンが形成できないので描画不能
	if (vertices.size() < 3) { return; }

	// オブジェクト単位の定数バッファで変更があった場合のみ情報転送
	if (m_dirtyCBObj)
	{
		m_cb0_Obj.Write();
	}

	// 3Dワールド行列転送（ビルボード適用後の行列を使う。追加 2026/07/14）
	m_cb1_Mesh.Work().mW = mDrawWorld;
	m_cb1_Mesh.Write();

	// マテリアルの転送
	if (rPolygon.GetMaterial())
	{
		WriteMaterial(*rPolygon.GetMaterial(), colRate, emissive);
	}
	else
	{
		WriteMaterial(KdMaterial(), colRate, emissive);
	}

	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);

	// サンプラーステートの変更:ポリゴンの描画なので、テクスチャの末端が繰り返されると不自然な描画になるため変更が必要
	if (KdShaderManager::Instance().IsPixelArtStyle())
	{
		KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Point_Clamp);
	}
	else
	{
		KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Anisotropic_Clamp);
	}

	// 描画パイプラインのチェック
	ID3D11VertexShader* pNowVS = nullptr;
	KdDirect3D::Instance().WorkDevContext()->VSGetShader(&pNowVS, nullptr, nullptr);
	bool isLitShader = m_VS_Lit == pNowVS;
	KdSafeRelease(pNowVS);

	// 陰影ありのシェーダーで2Dオブジェクトを描画する時
	if (isLitShader && rPolygon.Is2DObject())
	{
		std::vector<KdPolygon::Vertex> _2DVertices = vertices;

		// ポリゴンの法線を光に向ける処理：どの方向に向いていても光の影響を正面からに受けるように変換
		ConvertNormalsFor2D(_2DVertices, mDrawWorld);	// ビルボード適用後の行列を使う（追加 2026/07/14）

		// 2DObject用に変換した頂点配列を描画
		KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, (signed)_2DVertices.size(), &_2DVertices[0], sizeof(KdPolygon::Vertex));
	}
	else
	{
		// 頂点配列を描画
		KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, (signed)vertices.size(), &vertices[0], sizeof(KdPolygon::Vertex));
	}

	KdShaderManager::Instance().UndoSamplerState();

	KdShaderManager::Instance().UndoRasterizerState();

	// 定数に変更があった場合は自動的に初期状態に戻す
	if (m_dirtyCBObj)
	{
		ResetCBObject();
	}
}

void KdStandardShader::DrawVertices(const std::vector<KdPolygon::Vertex>& vertices, const Math::Matrix& mWorld,
	const Math::Color& colRate)
{
	// 頂点数が2より少なければポリゴンが形成できないので描画不能
	if (vertices.size() < 2) { return; }

	// オブジェクト単位の定数バッファで変更があった場合のみ情報転送
	if (m_dirtyCBObj)
	{
		m_cb0_Obj.Write();
	}

	// 3Dワールド行列転送
	m_cb1_Mesh.Work().mW = mWorld;
	m_cb1_Mesh.Write();

	// マテリアルの転送
	WriteMaterial(KdMaterial(), colRate, Math::Vector3::Zero);

	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);
	KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZDisable);

	// サンプラーステートの変更:ポリゴンの描画なので、テクスチャの末端が繰り返されると不自然な描画になるため変更が必要
	if (KdShaderManager::Instance().IsPixelArtStyle())
	{
		KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Point_Clamp);
	}
	else
	{
		KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Anisotropic_Clamp);
	}

	// 描画パイプラインのチェック
	ID3D11VertexShader* pNowVS = nullptr;
	KdDirect3D::Instance().WorkDevContext()->VSGetShader(&pNowVS, nullptr, nullptr);

	KdSafeRelease(pNowVS);

	// 頂点配列を描画
	KdDirect3D::Instance().DrawVertices(D3D_PRIMITIVE_TOPOLOGY_LINELIST, (signed)vertices.size(), &vertices[0], sizeof(KdPolygon::Vertex));

	KdShaderManager::Instance().UndoSamplerState();

	KdShaderManager::Instance().UndoDepthStencilState();

	KdShaderManager::Instance().UndoRasterizerState();
	// 定数に変更があった場合は自動的に初期状態に戻す
	if (m_dirtyCBObj)
	{
		ResetCBObject();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// KdShaderManagerの初期化時に呼び出される
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シェーダー本体の生成
// シェーダーで利用する定数バッファの生成
// 影用の光からの深度情報テクスチャを生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool KdStandardShader::Init()
{
	//-------------------------------------
	// 頂点シェーダ(スキンメッシュ対応)
	//-------------------------------------
	{
		// コンパイル済みのシェーダーヘッダーファイルをインクルード
#include "KdStandardShader_VS_Lit.shaderInc"

		// 頂点シェーダー作成
		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS_Lit))) {
			assert(0 && "頂点シェーダー作成失敗");
			Release();
			return false;
		}

		// １頂点の詳細な情報
		std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,			0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,		0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "SKININDEX",	0, DXGI_FORMAT_R16G16B16A16_UINT,	0, 48,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "SKINWEIGHT",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 56,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		// 頂点入力レイアウト作成
		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateInputLayout(
			&layout[0],				// 入力エレメント先頭アドレス
			(UINT)layout.size(),	// 入力エレメント数
			&compiledBuffer[0],		// 頂点バッファのバイナリデータ
			sizeof(compiledBuffer),	// 上記のバッファサイズ
			&m_inputLayout))
			) {
			assert(0 && "CreateInputLayout失敗");
			Release();
			return false;
		}
	}

	// ===== 追加(GPUインスタンシング)：Lit用インスタンシングVS＋専用入力レイアウト =====
	// 通常版と違い、ワールド行列を定数バッファではなく頂点slot1のインスタンスデータで受け取る。
	// そのため per-instance 要素を含む専用の入力レイアウトが必要になる。
	{
#include "KdStandardShader_VS_Lit_Inst.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS_Lit_Inst))) {
			assert(0 && "頂点シェーダー作成失敗(Lit_Inst)");
			Release();
			return false;
		}

		// slot0=頂点ごとのデータ(通常版と同じ) / slot1=インスタンスごとのワールド行列(float4×4=64byte)
		// slot1は D3D11_INPUT_PER_INSTANCE_DATA なので、1インスタンス進むごとに次の行列が読まれる
		std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "COLOR",		0, DXGI_FORMAT_R8G8B8A8_UNORM,		0, 20, D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 24, D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "TANGENT",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 36, D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "SKININDEX",	0, DXGI_FORMAT_R16G16B16A16_UINT,	0, 48, D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "SKINWEIGHT",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 56, D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "INSTMAT",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INSTMAT",	1, DXGI_FORMAT_R32G32B32A32_FLOAT,	1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INSTMAT",	2, DXGI_FORMAT_R32G32B32A32_FLOAT,	1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INSTMAT",	3, DXGI_FORMAT_R32G32B32A32_FLOAT,	1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		};

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateInputLayout(
			&layout[0],
			(UINT)layout.size(),
			&compiledBuffer[0],
			sizeof(compiledBuffer),
			&m_inputLayout_Inst))
			) {
			assert(0 && "CreateInputLayout失敗(インスタンシング用)");
			Release();
			return false;
		}
	}

	// ===== 追加(GPUインスタンシング)：影深度用インスタンシングVS =====
	// 入力レイアウトは上の m_inputLayout_Inst を共用する(頂点要素の並びが同じため)
	{
#include "KdStandardShader_VS_GenDepthMapFromLight_Inst.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS_GenDepthFromLight_Inst))) {
			assert(0 && "頂点シェーダー作成失敗(GenDepth_Inst)");
			Release();
			return false;
		}
	}

	{
#include "KdStandardShader_VS_GenDepthMapFromLight.shaderInc"

		// 頂点シェーダー作成
		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS_GenDepthFromLight))) {
			assert(0 && "頂点シェーダー作成失敗");
			Release();
			return false;
		}
	}

	{
#include "KdStandardShader_VS_UnLit.shaderInc"

		// 頂点シェーダー作成
		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS_UnLit))) {
			assert(0 && "頂点シェーダー作成失敗");
			Release();
			return false;
		}
	}

	//-------------------------------------
	// ピクセルシェーダ
	//-------------------------------------
	{
#include "KdStandardShader_PS_Lit.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Lit))) {
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();
			return false;
		}
	}

	{
#include "KdStandardShader_PS_GenDepthMapFromLight.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_GenDepthFromLight))) {
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();
			return false;
		}
	} 
	
	{
#include "KdStandardShader_PS_UnLit.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_UnLit))) {
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();
			return false;
		}
	}
	//-------------------------------------
	// 定数バッファ作成
	//-------------------------------------
	m_cb0_Obj.Create();
	m_cb1_Mesh.Create();
	m_cb2_Material.Create();
	m_cb3_Bone.Create();

	std::shared_ptr<KdTexture> ds = std::make_shared<KdTexture>();
	ds->CreateDepthStencil(1024, 1024);
	D3D11_VIEWPORT vp = {
		0.0f, 0.0f,
		static_cast<float>(ds->GetWidth()),
		static_cast<float>(ds->GetHeight()),
		0.0f, 1.0f };

	m_depthMapFromLightRTPack.CreateRenderTarget(1024, 1024, true, DXGI_FORMAT_R32_FLOAT);
	m_depthMapFromLightRTPack.ClearTexture(kRedColor);

	SetDissolveTexture(*KdAssets::Instance().m_textures.GetData("Asset/Textures/System/WhiteNoise.png"));


	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// シェーダー本体の解放
// 利用していたコンスタントバッファの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::Release()
{
	KdSafeRelease(m_VS_Lit);
	KdSafeRelease(m_VS_GenDepthFromLight);
	KdSafeRelease(m_VS_UnLit);

	KdSafeRelease(m_inputLayout);

	// 追加(GPUインスタンシング)：専用VS・入力レイアウト・インスタンスバッファの解放
	KdSafeRelease(m_VS_Lit_Inst);
	KdSafeRelease(m_VS_GenDepthFromLight_Inst);
	KdSafeRelease(m_inputLayout_Inst);
	KdSafeRelease(m_instanceBuf);
	m_instanceCapacity = 0;

	KdSafeRelease(m_PS_Lit);
	KdSafeRelease(m_PS_GenDepthFromLight);
	KdSafeRelease(m_PS_UnLit);

	m_cb0_Obj.Release();
	m_cb1_Mesh.Release();
	m_cb2_Material.Release();
	// スキンメッシュ対応
	m_cb3_Bone.Release();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画用マテリアル情報の転送
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// それぞれのマテリアルの影響倍率値とテクスチャを設定
// BaseColor：基本色 / Emissive：自己発光色 / Metalic：金属性(テカテカ) / Roughness：粗さ(材質の色の反映度)
// テクスチャは法線マップ以外は未設定なら白1ピクセルのシステムテクスチャを指定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::WriteMaterial(const KdMaterial& material, const Math::Vector4& colRate, const Math::Vector3& emiRate)
{
	//-----------------------
	// マテリアル情報を定数バッファへ書き込む
	//-----------------------
	m_cb2_Material.Work().BaseColor = material.m_baseColorRate * colRate;
	m_cb2_Material.Work().Emissive = material.m_emissiveRate * emiRate;
	m_cb2_Material.Work().Metallic = material.m_metallicRate;
	m_cb2_Material.Work().Roughness = material.m_roughnessRate;
	m_cb2_Material.Write();

	//-----------------------
	// テクスチャセット
	//-----------------------
	ID3D11ShaderResourceView* srvs[4];

	srvs[0] = material.m_baseColorTex ? material.m_baseColorTex->WorkSRView() : KdDirect3D::Instance().GetWhiteTex()->WorkSRView();
	srvs[1] = material.m_metallicRoughnessTex ? material.m_metallicRoughnessTex->WorkSRView() : KdDirect3D::Instance().GetWhiteTex()->WorkSRView();
	srvs[2] = material.m_emissiveTex ? material.m_emissiveTex->WorkSRView() : KdDirect3D::Instance().GetWhiteTex()->WorkSRView();
	srvs[3] = material.m_normalTex ? material.m_normalTex->WorkSRView() : KdDirect3D::Instance().GetNormalTex()->WorkSRView();

	// セット
	KdDirect3D::Instance().WorkDevContext()->PSSetShaderResources(0, _countof(srvs), srvs);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ポリゴンがどの方向に向いていても光の影響を正面からに受けるように頂点の法線を変換
// 2Dキャラクタを描画する時などは必要
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::ConvertNormalsFor2D(std::vector<KdPolygon::Vertex>& target, const Math::Matrix& mWorld)
{
	// 平行光の向き
	const Math::Vector3& dirLight_Dir = KdShaderManager::Instance().GetLightCB().DirLight_Dir;

	// どの角度を向いていても表面は常に光の方向を向いている状態：横向きの板ポリが暗くならない対策
	Math::Vector3 normal = Math::Vector3::TransformNormal(-dirLight_Dir, mWorld.Invert());
	Math::Vector3 tangent = (normal != Math::Vector3::Up) ?
		normal.Cross(Math::Vector3::Up) : normal.Cross(Math::Vector3::Right);

	for (size_t i = 0; i < target.size(); ++i)
	{
		target[i].normal = normal;
		target[i].tangent = tangent;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// オブジェクト定数バッファを初期状態に戻す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdStandardShader::ResetCBObject()
{
	m_cb0_Obj.Work() = cbObject();

	m_cb0_Obj.Write();

	m_dirtyCBObj = false;
}
