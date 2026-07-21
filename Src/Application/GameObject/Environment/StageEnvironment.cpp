#include "StageEnvironment.h"

#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Debug/DebugFlags/DebugFlags.h"

namespace
{
	// 初期配布フレームワークの「素の」環境設定。
	//
	// これらの値の出どころは KdShaderManager.h の cbLight / cbFog の構造体初期化子で、
	// 初期配布では Application 側から SetDirLight / SetAmbientLight を一度も呼んでいないため、
	// その初期化子の値がそのまま画面の見た目になっていた。
	// ここでは値をハードコードせず、まだ誰も上書きしていない起動直後の定数バッファを吸い出す
	// (engine側の既定値が変わっても自動で追従する)。
	struct OriginalEnvironment
	{
		Math::Vector4	ambientColor	= { 0.3f, 0.3f, 0.3f, 1.0f };
		Math::Vector3	dirLightDir		= { 1.0f, -1.0f, 1.0f };
		Math::Vector3	dirLightColor	= { 2.25f, 2.25f, 2.25f };
		bool			fogEnable		= false;
		Math::Vector3	fogColor		= { 1.0f, 1.0f, 1.0f };
		float			fogDensity		= 0.001f;
	};

	OriginalEnvironment	g_original;
	bool				g_originalCaptured = false;
}

void StageEnvironment::CaptureOriginalOnce()
{
	// 【重要】最初の1回だけ吸い出す。
	// シーンを作り直すと StageEnvironment も作り直されるが、その時点の定数バッファには
	// 前回このクラスが適用した調整後の値(ネオンの夜など)が残っている。
	// 毎回取り直すと「調整後の値」を初期配布の値として掴んでしまうため、1回で固定する
	if (g_originalCaptured) { return; }

	const KdShaderManager::cbLight& light = KdShaderManager::Instance().GetLightCB();
	const KdShaderManager::cbFog&   fog   = KdShaderManager::Instance().GetFogCB();

	g_original.ambientColor  = light.AmbientLight;
	g_original.dirLightDir   = light.DirLight_Dir;
	g_original.dirLightColor = light.DirLight_Color;

	g_original.fogEnable  = (fog.DistanceFogEnable != 0);
	g_original.fogColor   = fog.DistanceFogColor;
	g_original.fogDensity = fog.DistanceFogDensity;

	g_originalCaptured = true;
}

void StageEnvironment::ApplyOriginal()
{
	KdAmbientController& amb = KdShaderManager::Instance().WorkAmbientController();

	amb.SetDirLight(g_original.dirLightDir, g_original.dirLightColor);
	amb.SetAmbientLight(g_original.ambientColor);

	amb.SetFogEnable(g_original.fogEnable, false);
	if (g_original.fogEnable)
	{
		amb.SetDistanceFog(g_original.fogColor, g_original.fogDensity);
	}

	// 影エリアだけは定数バッファから読み戻せない(KdAmbientControllerが合成済みの行列しか持たず、
	// エリアと高さはprivate)。初期配布の KdAmbientController::Init() が指定している
	// SetDirLightShadowArea({25,25}, 30) をそのまま書いている
	amb.SetDirLightShadowArea(Math::Vector2(25.0f, 25.0f), 30.0f);
}

void StageEnvironment::Init()
{
	// Applyより先に呼ぶこと。Applyが定数バッファを上書きしてしまうと素の値が取れなくなる
	CaptureOriginalOnce();

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
	// 初期配布フレームワークの素の状態を見たいときは、調整値を一切使わずそちらを適用する。
	// DebugParams側の値は書き換えないので、OFFに戻せば今の見た目がそのまま復帰する
	if (DebugFlags::Instance().Get(U8("環境/初期配布の設定"), false))
	{
		ApplyOriginal();
		return;
	}

	KdAmbientController& amb = KdShaderManager::Instance().WorkAmbientController();

	// ※ 以下の既定値は初期配布フレームワークの素の値に揃えてある(2026/07/21)。
	//    出どころは KdShaderManager.h の cbLight / cbFog の構造体初期化子と、
	//    KdAmbientController::Init() の SetDirLightShadowArea({25,25}, 30)。
	//    JSONが無い環境でも初期配布と同じ見た目で立ち上がるようにするため

	// --- 平行光(向き・色) : SetDirLightが内部で向きを正規化する ---
	Math::Vector3 lightDir = DebugParams::Instance().Vector3Param(U8("環境/平行光の向き"), Math::Vector3(1.0f, -1.0f, 1.0f));
	Math::Vector3 lightCol = DebugParams::Instance().Vector3Param(U8("環境/平行光の色"),   Math::Vector3(2.25f, 2.25f, 2.25f));
	amb.SetDirLight(lightDir, lightCol);

	// --- 環境光(全体の下地の明るさ。暗いほど陰影・フォグ・発光が際立つ) ---
	Math::Vector3 ambCol = DebugParams::Instance().Vector3Param(U8("環境/環境光の色"), Math::Vector3(0.3f, 0.3f, 0.3f));
	amb.SetAmbientLight(Math::Vector4(ambCol.x, ambCol.y, ambCol.z, 1.0f));

	// --- 距離フォグ(遠くを霞ませて奥行き=疾走感を出す) ---
	// ※ 既定を false にしてある。初期配布では cbFog の DistanceFogEnable が 0(無効)で、
	//    フォグは一度も有効化されていなかったため
	bool fogEnable = DebugFlags::Instance().Get(U8("環境/フォグ"), false);
	amb.SetFogEnable(fogEnable, false);
	if (fogEnable)
	{
		Math::Vector3 fogCol = DebugParams::Instance().Vector3Param(U8("環境/フォグ色"), Math::Vector3(1.0f, 1.0f, 1.0f));
		// フォグの式は f = 1/exp(距離 x 濃度) で、fが1なら素の色・0なら完全にフォグ色(KdStandardShader_PS_Lit.hlsl)。
		// 参考(かつて疾走感の演出に使っていたときの目安)：
		//   0.010 → 50m:61%残る / 150m:22% / 260m:7%
		//   濃くするほど手前から霞み、薄くするとカリング距離での建物の出現(ポップイン)が見えてくる
		float fogDensity = DebugParams::Instance().Float(U8("環境/フォグ濃度"), 0.001f, 0.0f, 0.2f);
		amb.SetDistanceFog(fogCol, fogDensity);
	}

	// --- 影の生成エリア(カメラ位置を中心とした箱。横=エリア / 奥行き=高さ×2) ---
	// ※ 影はこの箱の中だけに出る。深度マップに広範囲を詰めるほど1体あたりの影は粗くなる。
	// ※ 高さ(=深度の奥行き)はエリアに自動連動させる。エリア(横)だけ広げると箱の隅が深度範囲から
	//    はみ出して地面に境目の線が出るため、高さをエリアに比例させて確保する。
	//    → 基本は「環境/影エリア」1つだけ調整すればよい(境目が出にくい)
	//
	// 既定 エリア25 / 比率1.2(=高さ30) は初期配布の SetDirLightShadowArea({25,25}, 30) と同値。
	// エリア25は建物1棟(最大29.3m)より小さいので、街ではカメラのごく近くにしか影が出ない。
	// 街向けに広げるときの目安(以前この値で調整していたときの記録)：
	//   40 = カメラ中心±20m / 100 = ±50m。代償は影の精度
	//   (1024x1024の深度を広い範囲へ伸ばすため 40で約4cm/テクセル → 100で約10cm/テクセル)
	float shadowArea  = DebugParams::Instance().Float(U8("環境/影エリア"),    25.0f, 5.0f, 200.0f);
	float heightRatio = DebugParams::Instance().Float(U8("環境/影の高さ比率"), 1.2f, 0.3f,   2.0f);
	float shadowHeight = shadowArea * heightRatio;
	if (shadowHeight < 25.0f)
	{
		shadowHeight = 25.0f;
	}   // 小さいエリアでも塔より高い所から照らすための下限
	amb.SetDirLightShadowArea(Math::Vector2(shadowArea, shadowArea), shadowHeight);

	// --- 影の解像度 ---
	// 1テクセルの実サイズ = 影エリア ÷ 解像度。影の細かさはこの2つで決まるので、
	// 影エリアとセットで実行中に振れるようにしてある。
	// 値が変わった時だけ深度マップを作り直す(SetShadowMapSize側で判定)。
	// ※ ここはUpdateフェーズ＝描画前なので、深度マップがバインド中に作り直すことはない
	// VRAM: 解像度^2 x 4byte x 2枚 → 1024で8MB / 2048で32MB / 4096で128MB
	int shadowRes = DebugParams::Instance().Int(U8("環境/影の解像度"), 1024, 256, 4096);
	KdShaderManager::Instance().m_StandardShader.SetShadowMapSize(shadowRes);
}
