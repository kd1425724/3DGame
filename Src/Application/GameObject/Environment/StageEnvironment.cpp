#include "StageEnvironment.h"

#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Debug/DebugFlags/DebugFlags.h"

void StageEnvironment::Init()
{
	// 初回に一度適用しておく。Updateでも毎フレーム適用するが、エディタモード等で
	// Updateが止まっても初期の空気感は保たれるようにするため
	Apply();
}

void StageEnvironment::Update()
{
	Apply();
}

void StageEnvironment::Apply()
{
	KdAmbientController& amb = KdShaderManager::Instance().WorkAmbientController();

	// --- 平行光(向き・色) : SetDirLightが内部で向きを正規化する ---
	// ※ OFFのときは色をゼロにする(KdAmbientControllerに平行光のON/OFFが無いため)。
	//    太陽光の拡散・鏡面がすべて消え、環境光だけで見た絵になる=影の付き方を切り分けられる。
	//    向きは切っても渡し続ける。KdShaderManager::WriteCBShadowArea が
	//    XMMatrixLookAtLH(camPos - 向き x 高さ, camPos, up) で影のビュー行列を作るため、
	//    ゼロベクトルを渡すと視点と注視点が一致してLookAtが破綻する(影が壊れる)
	Math::Vector3 lightDir = DebugParams::Instance().Vector3Param(U8("環境/平行光の向き"), Math::Vector3(-0.5f, -1.0f, -0.5f));
	Math::Vector3 lightCol = DebugParams::Instance().Vector3Param(U8("環境/平行光の色"),   Math::Vector3(2.2f, 2.1f, 1.9f));
	if (!DebugFlags::Instance().Get(U8("環境/平行光"), true))
	{
		lightCol = Math::Vector3::Zero;
	}
	amb.SetDirLight(lightDir, lightCol);

	// --- 環境光(全体の下地の明るさ。暗いほど陰影・フォグ・発光が際立つ) ---
	// ※ OFFのときはRGBだけをゼロにする。影の中が真っ黒になり、影の形がはっきり見える。
	//    第4成分(w)は1.0のまま触らないこと。KdStandardShader_PS_UnLit.hlsl が w を
	//    「全体の明度」として使っており、ここを0にするとUnLit描画まで消えてしまう
	Math::Vector3 ambCol = DebugParams::Instance().Vector3Param(U8("環境/環境光の色"), Math::Vector3(0.35f, 0.38f, 0.45f));
	if (!DebugFlags::Instance().Get(U8("環境/環境光"), true))
	{
		ambCol = Math::Vector3::Zero;
	}
	amb.SetAmbientLight(Math::Vector4(ambCol.x, ambCol.y, ambCol.z, 1.0f));

	// --- 距離フォグ(遠くを霞ませて奥行き=疾走感を出す) ---
	bool fogEnable = DebugFlags::Instance().Get(U8("環境/フォグ"), true);
	amb.SetFogEnable(fogEnable, false);
	if (fogEnable)
	{
		Math::Vector3 fogCol = DebugParams::Instance().Vector3Param(U8("環境/フォグ色"), Math::Vector3(0.5f, 0.6f, 0.75f));
		// 濃度は「カリング距離(既定260)で建物が霧に溶けきる」ことを狙って決める。
		// フォグの式は f = 1/exp(距離 x 濃度) で、fが1なら素の色・0なら完全にフォグ色(KdStandardShader_PS_Lit.hlsl)。
		//   0.010 → 50m:61%残る / 150m:22% / 260m:7%
		// 街を2倍に広げる前の 0.02 は 80m 以遠が一律の霧になり、建物の輪郭が青くにじんでいた。
		// 濃くするほど手前から霞み、薄くするとカリング距離での建物の出現(ポップイン)が見えてくる
		float fogDensity = DebugParams::Instance().Float(U8("環境/フォグ濃度"), 0.010f, 0.0f, 0.2f);
		amb.SetDistanceFog(fogCol, fogDensity);
	}

	// --- 影の生成エリア(カメラ位置を中心とした箱。横=エリア / 奥行き=高さ×2) ---
	// ※ 影はこの箱の中だけに出る。1024x1024の深度に広範囲を詰めるほど1体あたりの影は粗くなる。
	// ※ 高さ(=深度の奥行き)はエリアに自動連動させる。エリア(横)だけ広げると箱の隅が深度範囲から
	//    はみ出して地面に境目の線が出るため、高さをエリアに比例させて確保する。
	//    → 基本は「環境/影エリア」1つだけ調整すればよい(境目が出にくい)
	// エリアは「街の建物の高さ」ではなく「どこまで先の影を出したいか」で決める。
	// 街を2倍(最大29.3m)に広げた後は 40 = カメラ中心±20m しか影が出ず、通りの先が影無しになっていた。
	// 100 にすると±50mまで影が出る。代償は影の精度(1024x1024の深度を広い範囲に伸ばすため
	// 40のとき約4cm/テクセル → 100では約10cm/テクセル)
	float shadowArea  = DebugParams::Instance().Float(U8("環境/影エリア"),    100.0f, 5.0f, 200.0f);
	float heightRatio = DebugParams::Instance().Float(U8("環境/影の高さ比率"), 0.75f, 0.3f,   2.0f);
	float shadowHeight = shadowArea * heightRatio;
	if (shadowHeight < 25.0f)
	{
		shadowHeight = 25.0f;
	}   // 小さいエリアでも塔より高い所から照らすための下限
	amb.SetDirLightShadowArea(Math::Vector2(shadowArea, shadowArea), shadowHeight);
}
