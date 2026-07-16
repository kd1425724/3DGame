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

	// --- 影の生成エリア(カメラ位置を中心とした正方形の範囲。塔コースは広いので調整可能に) ---
	// ※ 影はこの範囲の中だけに出る。範囲を広げると全体に影が出るが1024x1024の深度に広範囲を
	//    詰めるほど1体あたりの影は粗くなる。範囲を狭めるほど密で綺麗だが端で影が切れる
	float shadowArea   = DebugParams::Instance().Float(U8("環境/影エリア"),   40.0f, 5.0f, 200.0f);
	float shadowHeight = DebugParams::Instance().Float(U8("環境/影の高さ"),   30.0f, 5.0f, 200.0f);
	amb.SetDirLightShadowArea(Math::Vector2(shadowArea, shadowArea), shadowHeight);
}
