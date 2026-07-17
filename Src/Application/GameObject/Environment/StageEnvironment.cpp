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
	Math::Vector3 lightDir = DebugParams::Instance().Vector3Param(U8("環境/平行光の向き"), Math::Vector3(-0.5f, -1.0f, -0.5f));
	Math::Vector3 lightCol = DebugParams::Instance().Vector3Param(U8("環境/平行光の色"),   Math::Vector3(2.2f, 2.1f, 1.9f));
	amb.SetDirLight(lightDir, lightCol);

	// --- 環境光(全体の下地の明るさ。暗いほど陰影・フォグ・発光が際立つ) ---
	Math::Vector3 ambCol = DebugParams::Instance().Vector3Param(U8("環境/環境光の色"), Math::Vector3(0.35f, 0.38f, 0.45f));
	amb.SetAmbientLight(Math::Vector4(ambCol.x, ambCol.y, ambCol.z, 1.0f));

	// --- 距離フォグ(遠くを霞ませて奥行き=疾走感を出す) ---
	bool fogEnable = DebugFlags::Instance().Get(U8("環境/フォグ"), true);
	amb.SetFogEnable(fogEnable, false);
	if (fogEnable)
	{
		Math::Vector3 fogCol = DebugParams::Instance().Vector3Param(U8("環境/フォグ色"), Math::Vector3(0.5f, 0.6f, 0.75f));
		float fogDensity = DebugParams::Instance().Float(U8("環境/フォグ濃度"), 0.02f, 0.0f, 0.2f);
		amb.SetDistanceFog(fogCol, fogDensity);
	}

	// --- 影の生成エリア(カメラ位置を中心とした箱。横=エリア / 奥行き=高さ×2) ---
	// ※ 影はこの箱の中だけに出る。1024x1024の深度に広範囲を詰めるほど1体あたりの影は粗くなる。
	// ※ 高さ(=深度の奥行き)はエリアに自動連動させる。エリア(横)だけ広げると箱の隅が深度範囲から
	//    はみ出して地面に境目の線が出るため、高さをエリアに比例させて確保する。
	//    → 基本は「環境/影エリア」1つだけ調整すればよい(境目が出にくい)
	float shadowArea  = DebugParams::Instance().Float(U8("環境/影エリア"),     40.0f, 5.0f, 200.0f);
	float heightRatio = DebugParams::Instance().Float(U8("環境/影の高さ比率"), 0.75f, 0.3f,   2.0f);
	float shadowHeight = shadowArea * heightRatio;
	if (shadowHeight < 25.0f)
	{
		shadowHeight = 25.0f;
	}   // 小さいエリアでも塔より高い所から照らすための下限
	amb.SetDirLightShadowArea(Math::Vector2(shadowArea, shadowArea), shadowHeight);
}
