#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

//================================
// 頂点シェーダ：影深度（GPUインスタンシング版）
//
//  ・通常版(KdStandardShader_VS_GenDepthMapFromLight.hlsl)との違いは
//    「ワールド行列を定数バッファ g_mWorld ではなく、頂点バッファ slot1 の
//     インスタンスごとの行列から受け取る」点だけ。影パスもまとめて描けるようにする。
//  ・skinIndex/skinWeight は未使用。インスタンス版Litと入力レイアウトを共有するために受け取る。
//================================
VSOutputGenShadow main(
	float4 pos : POSITION,			// 頂点座標
	float2 uv : TEXCOORD0,			// テクスチャUV座標
	float4 color : COLOR,			// 頂点カラー
	float3 normal : NORMAL,			// 法線
	float3 tangent : TANGENT,		// 接線
	uint4 skinIndex : SKININDEX,	// (未使用)
	float4 skinWeight : SKINWEIGHT,	// (未使用)
	// ----- ここからインスタンスごとのデータ(頂点バッファ slot1) -----
	float4 mtx0 : INSTMAT0,			// ワールド行列 1行目
	float4 mtx1 : INSTMAT1,			// ワールド行列 2行目
	float4 mtx2 : INSTMAT2,			// ワールド行列 3行目
	float4 mtx3 : INSTMAT3			// ワールド行列 4行目(平行移動)
)
{
	// インスタンスのワールド行列を組み立てる(通常版の g_mWorld の代わり)
	float4x4 mWorld = float4x4(mtx0, mtx1, mtx2, mtx3);

	VSOutputGenShadow Out;

	// ローカル座標系 -> ワールド座標系へ変換
	Out.Pos = mul(pos, mWorld);

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
