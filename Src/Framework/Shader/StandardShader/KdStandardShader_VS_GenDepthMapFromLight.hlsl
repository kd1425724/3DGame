#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

//================================
// 頂点シェーダ：影深度
//================================
VSOutputGenShadow main(
	float4 pos : POSITION,			// 頂点座標
	float2 uv : TEXCOORD0,			// テクスチャUV座標
	float4 color : COLOR,			// 頂点カラー
	float3 normal : NORMAL,			// 法線
	float3 tangent : TANGENT,		// 接線
	uint4 skinIndex : SKININDEX,	// スキンメッシュのボーンインデックス
	float4 skinWeight : SKINWEIGHT	// ボーンの影響度
)
{
	// スキニング---------------->
	// 2026/07/20 追加：ここが無いとスキンメッシュの影だけバインドポーズ(Tポーズ)で出る。
	// 入力レイアウト(m_inputLayout)は3パス共通でSKININDEX/SKINWEIGHTを宣言済み、
	// ボーン行列もBeginGenerateDepthMapFromLightがセット済みで、
	// シェーダ側の計算だけが漏れていた。処理内容はVS_Litのスキニングと同じ。
	// ※ 静的モデルは g_IsSkinMeshObj が0なので分岐に入らず、従来と同じ経路を通る
	if (g_IsSkinMeshObj)
	{
		// 行列を合成
		row_major float4x4 mBones = 0; // 行列を0埋め
		[unroll]
		for (int i = 0; i < 4; i++)
		{
			mBones += g_mBones[skinIndex[i]] * skinWeight[i];
		}

		// 座標に適用(影は深度だけなので法線は不要)
		pos = mul(pos, mBones);
	}
	// <----------------スキニング

	VSOutputGenShadow Out;

	// キャラクターの座標変換 : ローカル座標系 -> ワールド座標系へ変換
	Out.Pos = mul(pos, g_mWorld);
	
	// カメラの逆向きに変換 : ワールド座標系 -> ビュー座標系 -> 射影座標系へ変換
	Out.Pos = mul(Out.Pos, g_DL_mLightVP);

	// 射影行列を変換されないように保存
	Out.pPos = Out.Pos;
	
	// UV座標
	Out.UV = uv;

	Out.Color = color;

	// 出力
	return Out;
}
