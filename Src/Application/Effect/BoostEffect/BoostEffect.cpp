#include "BoostEffect.h"

#include "../../main.h"   // Application::GetDeltaTime
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../API/MathAPI/MathAPI.h"   // 火花の向き(速度)を正規化する

// 共有の板ポリ実体。KdSquarePolygon(前方宣言)の破棄には完全な型が要るので.cpp側に置く
std::unique_ptr<KdSquarePolygon> BoostEffect::s_upPolyJet;
std::unique_ptr<KdSquarePolygon> BoostEffect::s_upPolySpark;

KdSquarePolygon* BoostEffect::GetSharedPoly(Style _style)
{
	// 初回だけ生成。用途ごとにテクスチャが違うので別々に持つ。
	// ※ 最初はSystem/WhiteNoise.pngを流用したが、あれはノイズ画像でアルファも無く、
	//    四角いノイズの塊がそのまま出て見た目が破綻したため専用テクスチャを用意した
	auto make = [](const char* _path, KdPolygon::BillboardMode _mode) -> std::unique_ptr<KdSquarePolygon>
		{
			std::shared_ptr<KdTexture> spTex = KdAssets::Instance().m_textures.GetData(_path);
			std::unique_ptr<KdSquarePolygon> up = std::make_unique<KdSquarePolygon>(spTex);
			up->Set2DObject(false);
			up->SetBillboardMode(_mode);
			return up;
		};

	if (_style == Style::Spark)
	{
		// 硬い芯を持つ火花用。
		// 【なぜジェットの粒を流用しないのか】あれは中心1.0から外周へなだらかに落ちる
		//   「ふんわりした粒」で、半透明な外周は背景が透ける。そのため色を付けても
		//   中央しか色が乗らず、周りは背景の色のままに見える(2026/07/31にユーザー指摘)。
		//   Spark.pngは可視部分の53%がアルファ0.9以上のべた塗りなので、色がそのまま出る。
		//   生成は Cloude\Project\3DGame\TextureGen\Recipes\Spark.ps1
		// 【軸固定ビルボード】にするのが要点。画面ビルボードだと筋が常に画面の上を向き、
		// 進行方向と無関係になって「飛んでいる粒の残像」に見えない
		if (!s_upPolySpark)
		{
			s_upPolySpark = make("Asset/Textures/Effect/Spark.png", KdPolygon::BillboardMode::eAxis);
		}
		return s_upPolySpark.get();
	}

	if (!s_upPolyJet)
	{
		s_upPolyJet = make("Asset/Textures/Effect/Particle.png", KdPolygon::BillboardMode::eScreen);
	}
	return s_upPolyJet.get();
}

BoostEffect::BoostEffect(const Math::Vector3& _pos, const Math::Vector3& _vel, Style _style)
	: m_pos(_pos), m_vel(_vel), m_style(_style)
{
	// 寿命は用途で分ける。火花はぱっと消えるほうが「弾けた」感じになる
	m_life = (m_style == Style::Spark)
		? DebugParams::Instance().Float(U8("火花/寿命"), 0.25f, 0.05f, 2.0f)
		: DebugParams::Instance().Float(U8("加速エフェクト/寿命"), 0.35f, 0.05f, 2.0f);
}

BoostEffect::~BoostEffect() = default;

void BoostEffect::Update()
{
	float dt = Application::Instance().GetDeltaTime();

	// 置かれた場所から後方へ流れていく
	m_pos += m_vel * dt;

	m_age += dt;
	if (m_age >= m_life)
	{
		m_isExpired = true;
	}   // 寿命が尽きたらEffectManagerが外す
}

void BoostEffect::DrawUnLit()
{
	KdSquarePolygon* pPoly = GetSharedPoly(m_style);
	if (!pPoly) { return; }

	const bool isSpark = (m_style == Style::Spark);

	// 濃さは用途で分ける。
	// 火花の既定を1.0にしてあるのは、半透明だと背景が透けて色が出ないため
	// (ジェットの0.6のままだと、テクスチャを硬くしても薄まって同じ問題が残る)
	float density = isSpark
		? DebugParams::Instance().Float(U8("火花/濃さ"), 1.0f, 0.05f, 1.0f)
		: DebugParams::Instance().Float(U8("加速エフェクト/濃さ"), 0.6f, 0.05f, 1.0f);

	float t = (m_life > 0.0f) ? (m_age / m_life) : 1.0f;   // 0→1
	float shrink = 1.0f - 0.6f * t;                        // だんだん縮む
	float alpha = (1.0f - t) * density;                    // フェードアウト(全体の濃さも掛ける)

	Math::Matrix world = Math::Matrix::Identity;

	if (isSpark)
	{
		// 【火花は丸い粒ではなく、進行方向に伸びた筋】
		//   板を速度方向へ向け、細長い長方形として描く。
		//   Spark.pngは上が頭・下が尾なので、板の+Yを速度方向に合わせると
		//   頭が前を向いて尾が後ろに残る＝飛んでいる粒の残像に見える。
		//   ※ 画面ビルボード(ジェットと同じ)のままだと、筋が常に画面の上を向いて
		//     進行方向と無関係になり不自然。板ポリはスタイルごとに別なので、
		//     火花用だけ軸固定ビルボードにしてある(GetSharedPoly)
		float length  = DebugParams::Instance().Float(U8("火花/長さ"),   0.35f, 0.02f, 3.0f);
		float width   = DebugParams::Instance().Float(U8("火花/太さ"),   0.05f, 0.005f, 1.0f);
		float stretch = DebugParams::Instance().Float(U8("火花/速さで伸びる量"), 0.02f, 0.0f, 0.2f);

		// 速い粒ほど長く尾を引く。これがあると「弾けて散った」感じが強くなる
		float speed = m_vel.Length();
		pPoly->SetScale(Math::Vector2(width * shrink, length * (1.0f + speed * stretch) * shrink));

		Math::Vector3 axis = m_vel;
		if (!MathAPI::TryNormalize(axis))
		{
			axis = Math::Vector3::Up;   // 速度0＝向きが決まらないときの逃げ
		}
		world.Up(axis);
	}
	else
	{
		float baseSize = DebugParams::Instance().Float(U8("加速エフェクト/サイズ"), 0.18f, 0.02f, 3.0f);
		float size = baseSize * shrink;
		pPoly->SetScale(Math::Vector2(size, size));
	}

	world.Translation(m_pos);

	// 【2026/07/31 訂正】ここには以前「色を決めているのは基本色ではなく emissive のほう」
	//   と書いてあったが、UnLitパスでは【誤り】だった。実測して確かめた事実：
	//     ・KdStandardShader_PS_UnLit.hlsl は
	//         outColor = 基本色テクスチャ * 頂点色 * g_BaseColor
	//         if (g_OnlyEmissie) 上書き else 【加算】 発光テクスチャ * g_Emissive * 頂点色
	//     ・g_OnlyEmissie は定義だけで【どこからも設定されておらず常に0】＝必ず加算になる
	//     ・発光テクスチャは未設定なので白が入る(KdStandardShader.cpp の WriteMaterial)
	//     ・Particle.png は【全ピクセルが純白(1,1,1)】で、形はアルファだけで作られている
	//   つまり 白 + 発光色 は必ず1.0で飽和し、【発光色を何にしても白にしかならない】。
	//   ジェットの色(0.65,0.85,1.0)もこれまで一度も効いていなかった。
	//
	//   正しい染め方は基本色(colRate)のほう。ここは基本色テクスチャに掛け算されるので、
	//   純白のテクスチャがそのまま指定した色になる。発光は0にして飽和を避ける
	Math::Vector3 tint = isSpark
		? DebugParams::Instance().Vector3Param(U8("火花/色"), Math::Vector3(1.0f, 0.55f, 0.15f))
		: DebugParams::Instance().Vector3Param(U8("加速エフェクト/色"), Math::Vector3(1.0f, 1.0f, 1.0f));

	Math::Color col(tint.x, tint.y, tint.z, alpha);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*pPoly, world, col, Math::Vector3::Zero);
}
