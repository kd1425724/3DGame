#include "Targeting.h"

#include "../Camera/CameraBase.h"
#include "../../Scene/SceneManager.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Debug/DebugFlags/DebugFlags.h"   // 遮蔽チェックのON/OFF
#include "../../Collision/CollisionGrid.h"       // IsWallBetween(敵が建物の陰にいるか)
#include "../../API/MathAPI/MathAPI.h"           // 安全な正規化

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

void Targeting::Update(const std::shared_ptr<CameraBase>& _spCamera, float _dt, bool _keepCurrent)
{
	// マーカーのアニメ用に時間を進める
	m_time += _dt;

	// カメラの向き(=画面中心の方向)に一番近い敵を選ぶ。カメラ自体は回さない。
	if (!_spCamera)
	{
		m_wpTarget.reset();
		return;
	}

	// ロックオン中は選び直さない。対象が消えた(倒した)ときは空のまま返し、
	// 呼び出し側(Player::UpdateLockOnSelection)がそれを見てロックを解く。
	// ※ ここで次の敵を探しに行くと「倒した瞬間に勝手に隣の敵へ掛かり直す」ことになり、
	//   ロックを明示的な操作にした意味が無くなる
	if (_keepCurrent)
	{
		std::shared_ptr<KdGameObject> spCurrent = m_wpTarget.lock();
		if (!spCurrent || spCurrent->IsExpired())
		{
			m_wpTarget.reset();
		}

		return;
	}

	// カメラの発射方向(ピッチ込み)。ワイヤー発射と同じ「フルの向き」
	Math::Vector3 camPos = _spCamera->GetPos();
	Math::Vector3 camFwd = Math::Vector3::TransformNormal(Math::Vector3::Backward, _spCamera->GetRotationMatrix());
	if (!MathAPI::TryNormalize(camFwd)) { return; }

	// 画面中心からの許容角度。これより外の敵は対象にしない
	float limitDeg = DebugParams::Instance().Float(U8("照準/有効角度"), 40.0f, 5.0f, 90.0f);
	float minDot = cosf(DirectX::XMConvertToRadians(limitDeg));

	// 角度内に入っている敵を「画面中心への近さ(内積)」付きで集める
	// ※ 内積が1に近いほど画面中心。（ベクトルA）toと（ベクトルB）camFwdの向きの一致度
	m_candidates.clear();
	for (auto& spEnemy : SceneManager::Instance().FindObjectsWithTag(KdGameObject::ObjectTag::Enemy))
	{
		if (!spEnemy) { continue; }
		Math::Vector3 to = spEnemy->GetPos() - camPos;
		if (!MathAPI::TryNormalize(to)) { continue; }
		float d = to.Dot(camFwd);
		if (d <= minDot) { continue; }   // 中心から外れすぎ
		m_candidates.emplace_back(d, spEnemy);
	}

	// 画面中心に近い順に並べる
	std::sort(m_candidates.begin(), m_candidates.end(),
		[](const Candidate& _a, const Candidate& _b) { return _a.first > _b.first; });

	// 遮蔽チェックの設定
	bool  useOcclusion = DebugFlags::Instance().Get(U8("照準/遮蔽チェック"), true);
	float aimHeight    = DebugParams::Instance().Float(U8("照準/狙う高さ"),         0.9f, 0.0f, 3.0f);
	float occMargin    = DebugParams::Instance().Float(U8("照準/遮蔽レイのマージン"), 0.5f, 0.0f, 3.0f);

	// 中心に近い順に「カメラから見えているか」を調べ、最初に見えている敵を採用する。
	// 単に遮蔽された敵を捨てるだけだとターゲットが消えてしまうので、
	// 一番近い敵が壁の裏なら「次に近い見えている敵」へ自然に落ちるようにしている。
	// レイは通常1回で済む(先頭が見えていれば即決)
	std::shared_ptr<KdGameObject> best;
	for (const Candidate& c : m_candidates)
	{
		if (useOcclusion)
		{
			// 足元(GetPos)を狙うと地面自身に遮られて常に「見えない」になるので、
			// マーカーと同じ高さ(胴のあたり)を狙う
			Math::Vector3 aim = c.second->GetPos() + Math::Vector3(0.0f, aimHeight, 0.0f);
			if (CollisionGrid::IsWallBetween(camPos, aim, occMargin)) { continue; }
		}

		best = c.second;
		break;
	}

	m_wpTarget = best;   // 中心に一番近い「見えている」敵(いなければ空)
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

	// 面内回転(local Z軸まわり)だけ作って配置。カメラへの正対はeScreenビルボードに任せる。
	// 狙う関節が指定されていればそこに、無ければ従来どおり敵の少し上に出す
	Math::Matrix world = Math::Matrix::CreateRotationZ(rotRad);
	world.Translation(m_hasMarkerOverride
		? m_markerOverridePos
		: spTarget->GetPos() + Math::Vector3(0.0f, 0.9f, 0.0f));

	// 発光色つきで描く(DrawPolygonはCullNoneなので裏表は不問。テクスチャの透過で照準形に抜ける)
	Math::Color   col(1.0f, 0.5f, 0.25f, 1.0f);
	Math::Vector3 emissive(0.8f, 0.35f, 0.1f);

	// 【常に手前に出す】狙う関節(首・肘・膝)は体の【内側】にあるので、
	// 深度テストを効かせたままだと必ずモデルに埋まって見えなくなる。
	// ロックオンのマーカーや敵のHPバーを「壁や体の向こうでも見せる」のは
	// 深度テストを切って描くのが定石(この作品ではKdDepthStencilState::ZDisable)。
	// ※ Changeした分は必ずUndoで戻す。戻さないと以降の描画が全部深度無視になる
	KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZDisable);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*m_upMarkerPoly, world, col, emissive);
	KdShaderManager::Instance().UndoDepthStencilState();
}
