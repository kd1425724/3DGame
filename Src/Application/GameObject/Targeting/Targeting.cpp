#include "Targeting.h"

#include "../Camera/CameraBase.h"
#include "../../Scene/SceneManager.h"
#include "../../Debug/DebugParams/DebugParams.h"

// 板ポリ(KdSquarePolygon)はPch経由で見える。unique_ptr(前方宣言)の生成/破棄を
// ここ(完全な型が見える.cpp)で行うため、ctor/dtorを定義する
Targeting::Targeting()
{
	// 照準テクスチャの板ポリ(カメラを向く点ビルボード)。テクスチャはKdAssetsキャッシュから取る
	std::shared_ptr<KdTexture> spTex = KdAssets::Instance().m_textures.GetData("Asset/Textures/UI/Reticle.png");
	m_upMarkerPoly = std::make_unique<KdSquarePolygon>(spTex);
	m_upMarkerPoly->Set2DObject(false);
	m_upMarkerPoly->SetBillboardMode(KdPolygon::BillboardMode::eScreen);
}

Targeting::~Targeting() = default;

void Targeting::Update(const std::shared_ptr<CameraBase>& _spCamera, float _dt)
{
	// マーカーのアニメ用に時間を進める
	m_time += _dt;

	// カメラの向き(=画面中心の方向)に一番近い敵を選ぶ。カメラ自体は回さない。
	if (!_spCamera)
	{
		m_wpTarget.reset();
		return;
	}

	// カメラの発射方向(ピッチ込み)。ワイヤー発射と同じ「フルの向き」
	Math::Vector3 camPos = _spCamera->GetPos();
	Math::Vector3 camFwd = Math::Vector3::TransformNormal(Math::Vector3::Backward, _spCamera->GetRotationMatrix());
	if (camFwd.LengthSquared() < 0.0001f) { return; }
	camFwd.Normalize();

	// 画面中心からの許容角度。これより外の敵は対象にしない
	float limitDeg = DebugParams::Instance().Float(U8("照準/有効角度"), 40.0f, 5.0f, 90.0f);
	float minDot = cosf(DirectX::XMConvertToRadians(limitDeg));

	// カメラの前方向に一番よく揃っている(=中心に近い)敵を探す
	std::shared_ptr<KdGameObject> best;
	float bestDot = minDot;   // これ未満は中心から外れすぎなので対象外
	for (auto& spEnemy : SceneManager::Instance().FindObjectsWithTag(KdGameObject::ObjectTag::Enemy))
	{
		if (!spEnemy) { continue; }
		Math::Vector3 to = spEnemy->GetPos() - camPos;
		if (to.LengthSquared() < 0.0001f) { continue; }
		to.Normalize();
		float d = to.Dot(camFwd);   // （ベクトルA）toと（ベクトルB）camFwdの内積が1に近いほど画面中心
		if (d > bestDot)
		{
			bestDot = d;
			best = spEnemy;
		}
	}

	m_wpTarget = best;   // 中心に一番近い敵(いなければ空)
}

void Targeting::DrawMarker()
{
	if (!m_upMarkerPoly) { return; }

	std::shared_ptr<KdGameObject> spTarget = m_wpTarget.lock();
	if (!spTarget) { return; }

	// マーカーサイズ＋脈動(sinで軽く拡縮=ロック中の呼吸感)
	float baseSize  = DebugParams::Instance().Float(U8("照準/マーカーサイズ"), 0.7f, 0.1f, 3.0f);
	float pulseAmp  = DebugParams::Instance().Float(U8("照準/脈動"),          0.15f, 0.0f, 1.0f);
	float size = baseSize * (1.0f + pulseAmp * sinf(m_time * 6.0f));
	m_upMarkerPoly->SetScale(Math::Vector2(size, size));

	// 回転角(照準がゆっくり回る)
	float rotSpeed = DebugParams::Instance().Float(U8("照準/回転速度"), 60.0f, 0.0f, 360.0f);
	float rotRad   = DirectX::XMConvertToRadians(m_time * rotSpeed);

	// 面内回転(local Z軸まわり)だけ作って敵の少し上へ配置。カメラへの正対はeScreenビルボードに任せる
	Math::Matrix world = Math::Matrix::CreateRotationZ(rotRad);
	world.Translation(spTarget->GetPos() + Math::Vector3(0.0f, 0.9f, 0.0f));

	// 発光色つきで描く(DrawPolygonはCullNoneなので裏表は不問。テクスチャの透過で照準形に抜ける)
	Math::Color   col(1.0f, 0.5f, 0.25f, 1.0f);
	Math::Vector3 emissive(0.8f, 0.35f, 0.1f);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*m_upMarkerPoly, world, col, emissive);
}
