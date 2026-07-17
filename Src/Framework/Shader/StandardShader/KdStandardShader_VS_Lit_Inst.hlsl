#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

//================================
// 頂点シェーダ：Lit（GPUインスタンシング版）
//
//  ・通常版(KdStandardShader_VS_Lit.hlsl)との違いは「ワールド行列の受け取り方」だけ。
//      通常版 … 定数バッファ g_mWorld を使う → 1オブジェクトにつき1ドロー必要
//      この版 … 頂点バッファ slot1 から「インスタンスごとのワールド行列」を受け取る
//               → 同じモデルを何個でも DrawIndexedInstanced 1回で描ける
//  ・行列は SimpleMath::Matrix(row-major) の4行をそのまま INSTMAT0〜3 として渡す。
//    そのため float4x4(mtx0..mtx3) は各float4が「行」になり、mul(pos, mWorld) が
//    通常版の mul(pos, g_mWorld) と同じ結果になる。
//  ・静的プロップ用途のためスキニングは行わない(スキンメッシュは通常版を使うこと)。
//    skinIndex/skinWeight は入力レイアウトを通常版と揃えるためだけに受け取る。
//================================
VSOutput main(
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

	VSOutput Out;

	// 座標変換
	Out.Pos = mul(pos, mWorld);		 // ローカル座標系	-> ワールド座標系へ変換
	Out.wPos = Out.Pos.xyz;			 // ワールド座標を別途保存
	Out.Pos = mul(Out.Pos, g_mView); // ワールド座標系	-> ビュー座標系へ変換
	Out.Pos = mul(Out.Pos, g_mProj); // ビュー座標系	-> 射影座標系へ変換

	// 頂点色
	Out.Color = color;

	// 法線
	Out.wN = normalize(mul(normal, (float3x3) mWorld));
	// 接線
	Out.wT = normalize(mul(tangent, (float3x3) mWorld));
	// 従接線
	float3 binormal = cross(normal, tangent);
	Out.wB = normalize(mul(binormal, (float3x3) mWorld));

	// UV座標
	Out.UV = uv * g_UVTiling + g_UVOffset;

	// 出力
	return Out;
}
