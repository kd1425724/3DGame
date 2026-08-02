#include "Targeting.h"

#include "../Camera/CameraBase.h"
#include "../../Scene/SceneManager.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Debug/DebugFlags/DebugFlags.h"   // 遮蔽チェックのON/OFF
#include "../../Collision/CollisionGrid.h"       // IsWallBetween(敵が建物の陰にいるか)
#include "../../API/MathAPI/MathAPI.h"           // 安全な正規化

Targeting::Targeting()
{
	// 照準テクスチャ。テクスチャはKdAssetsキャッシュから取る
	m_spMarkerTex = KdAssets::Instance().m_textures.GetData("Asset/Textures/UI/Reticle.png");
}

Targeting::~Targeting() = default;

void Targeting::Update(const std::shared_ptr<CameraBase>& _spCamera, float _dt, bool _keepCurrent)
{
	// マーカーのアニメ用に時間を進める
	m_time += _dt;

	// マーカーを2Dで描くのに要るので覚えておく(DrawMarkerは引数を取らないため)
	m_wpCamera = _spCamera;

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
	if (!m_spMarkerTex) { return; }

	std::shared_ptr<KdGameObject> spTarget = m_wpTarget.lock();
	if (!spTarget) { return; }

	std::shared_ptr<CameraBase> spCamera = m_wpCamera.lock();
	if (!spCamera || !spCamera->GetCamera()) { return; }

	// 出す位置：狙う関節が指定されていればそこ、無ければ従来どおり敵の少し上
	const Math::Vector3 worldPos = m_hasMarkerOverride
		? m_markerOverridePos
		: spTarget->GetPos() + Math::Vector3(0.0f, 0.9f, 0.0f);

	// ワールド座標 → スクリーン座標(原点は画面中央。2D描画の座標系と一致する)
	Math::Vector3 screen{};
	spCamera->GetCamera()->ConvertWorldToScreenDetail(worldPos, screen);

	// zにはw(カメラから見た奥行き)が入っている。0以下＝カメラの後ろなので描かない
	// (割った後の値は符号が反転していて、画面の反対側に出てしまう)
	if (screen.z <= 0.0f) { return; }

	// マーカーサイズ(px)＋脈動(sinで軽く拡縮=ロック中の呼吸感)。
	// ※ 3Dで描いていた頃は距離で自然に小さくなったが、2Dでは画面上の大きさが一定になる。
	//   ロックしている的の位置を見せるのが目的なので、一定のほうがむしろ見失いにくい
	float baseSize = DebugParams::Instance().Float(U8("照準/マーカーサイズpx"), 48.0f, 8.0f, 256.0f);
	float pulseAmp = DebugParams::Instance().Float(U8("照準/脈動"),             0.15f, 0.0f, 1.0f);
	int   size     = static_cast<int>(baseSize * (1.0f + pulseAmp * sinf(m_time * 6.0f)));

	// ※ 回転(照準がゆっくり回る)は落とした。DrawTexに回転を渡す口が無いため。
	//   脈動だけでもロック中であることは分かるので、回転のために自前で板ポリを
	//   組み直すほどの価値は無いと判断した
	Math::Color col(1.0f, 0.5f, 0.25f, 1.0f);
	KdShaderManager::Instance().m_spriteShader.DrawTex(
		m_spMarkerTex.get(), static_cast<int>(screen.x), static_cast<int>(screen.y), size, size, nullptr, &col);
}
