#include "CharaBase.h"

#include "../../main.h"
#include "../../Scene/SceneManager.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Collision/CollisionGrid.h"   // 静的コリジョンのbroadphase(近傍の建物だけ問い合わせる)

void CharaBase::SetAsset(const std::string& assetName)
{
	m_modelWork.SetModelData(KdAssets::Instance().m_modeldatas.GetData(assetName));
}

void CharaBase::DrawLit()
{
	if (!m_modelWork.IsEnable()) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld, m_color);
}

void CharaBase::GenerateDepthMapFromLight()
{
	if (!m_modelWork.IsEnable()) { return; }

	// 深度パス用シェーダはBaseScene側のBeginGenerateDepthMapFromLightで既にセット済み。
	// ここは通常のモデル描画を呼ぶだけで、光から見た深度が深度マップに書き込まれる
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}

// ※ IsWallBetween は 2026/07/19 に CollisionGrid へ移動(照準の遮蔽判定からも使うため)

void CharaBase::GroundCheck()
{
	ZoneScoped;	// Tracy計測(2026/07/19)：接地/天井/壁の解決をまとめて呼ぶ入口

	float deltaTime = Application::Instance().GetDeltaTime();

	// 重力を垂直速度に加える(重力はDebugParamsで調整可能)
	// m_gravityScale は壁走り中に0にされる(壁に張り付いている間は落ちない)。
	// 重力を止める側(WallAction)は「意思決定」のUpdateで倍率を書き、ここは解決するだけにしてある
	float gravity = DebugParams::Instance().Float(U8("キャラ/重力"), 20.0f, 0.0f, 100.0f);
	m_velocity.y -= gravity * m_gravityScale * deltaTime;

	// 速度で位置を進める(縦横まとめて)。水平速度は各キャラのUpdateが設定する
	// ※ Enemyは水平をUpdateで直接動かすためm_velocity.xzは0のまま＝縦だけ動く
	Math::Vector3 pos = GetPos();
	Math::Vector3 startPos = pos;   // 移動前の位置(スイープの始点)
	pos += m_velocity * deltaTime;

	// 地面に潜っていたら押し上げて落下を止める(接地状態もここで更新される)
	ResolveGround(pos);

	// 上昇中に頭上の天井へ潜り込むのを止める(接地とは上昇/落下で排他)
	ResolveCeiling(startPos, pos);

	// 高速移動で壁を飛び越える(トンネリング)のを先に止める
	ResolveBumpSweep(startPos, pos);

	// 壁(TypeBump=Block等)にめり込んでいたら水平に押し出す
	ResolveBump(pos);

	SetPos(pos);
}

void CharaBase::ResolveGround(Math::Vector3& pos, bool _allowLanding)
{
	ZoneScoped;	// Tracy計測(2026/07/19)：真下レイによる接地解決

	float deltaTime = Application::Instance().GetDeltaTime();

	// この呼び出しの前に接地していたか(着地の"瞬間"だけ手応えを出すためのエッジ検出用)
	bool wasGrounded = m_isGrounded;

	// あたる側の設定＝＝＝＝＝＝＝＝＝＝
	// レイの始点は「足元の少し上」。下向きに撃って接地面を拾う。落下が速いほど始点を上げ(fallThisFrame)、
	// 高速落下でも地面を飛び越さない(=可変レイ判定)。
	// ※ 以前は「体の中心から1.0上」=頭より上から出しており、デバッグ表示で邪魔＆過剰だったため足元基準に変更。
	//    足元より上を見るぶん(rayStepUp)が段差の自動乗り上げ許容高さになる(頭上までは伸ばさない)
	float feetY = pos.y - GetScale().y * 0.5f;
	float fallThisFrame = (m_velocity.y < 0.0f) ? (-m_velocity.y * deltaTime) : 0.0f;
	const float rayStepUp = 0.3f;   // 足元より上をどれだけ見るか(=乗り上げできる段差の高さ・接地の許容)
	const float rayBelow  = 0.2f;   // 足元より下をどれだけ見るか(接地の許容)
	Math::Vector3 rayPos(pos.x, feetY + rayStepUp + fallThisFrame, pos.z);
	float rayRange = rayStepUp + fallThisFrame + rayBelow;

	// 地面(TypeGround)に加えてBlock等(TypeBump)の天面にも乗れるようにする
	KdCollider::RayInfo ray(KdCollider::TypeGround | KdCollider::TypeBump, rayPos, Math::Vector3::Down, rayRange);

	// デバッグ表示：地面判定に使用したレイを可視化
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugLine(ray.m_pos, ray.m_dir, ray.m_range, Math::Color(1.0f, 1.0f, 0.0f, 1.0f));
	}

	// レイに当たったオブジェクトを格納するリストを作成
	std::list<KdCollider::CollisionResult> retRayList;

	// 近傍の静的コリジョンだけをグリッドから取り出して判定する(大量配置時のCPU削減)
	std::vector<KdGameObject*> candidates;
	CollisionGrid::Instance().QueryRay(rayPos, Math::Vector3::Down, rayRange, candidates);
	for (KdGameObject* obj : candidates)
	{
		obj->Intersects(ray, &retRayList);
	}

	// レイに当たったリストから一番遮った(overlapが最大の)地面を探す
	float maxOverLap = 0;
	Math::Vector3 hitPos;
	bool hit = false;

	for (auto& ret : retRayList)
	{
		if (maxOverLap < ret.m_overlapDistance)
		{
			maxOverLap = ret.m_overlapDistance;
			hitPos = ret.m_hitPos;
			hit = true;
		}
	}

	// 落下中(velocity.y<=0)に地面へ届いていて、立つべき高さより下に来ていたら着地させる
	// ※ 上昇中(ジャンプ直後)は吸着しないので、そのまま上へ飛べる
	if (hit && m_velocity.y <= 0.0f)
	{
		// 見た目のモデル(単位立方体想定)の半分だけ持ち上げて、地面に埋まらず立たせる
		float standY = hitPos.y + (GetScale().y * 0.5f);
		if (pos.y <= standY)
		{
			// 「着地しない」モード(ワイヤーで地面スレスレを飛ぶ用)。
			// 地面へのめり込みだけ直し、落下は止めるが、接地扱いにはしない。
			// 着地の手応えも記録しない(地面に触れるたびカメラが揺れるのを防ぐ)。
			// 横の勢いはそのままなので、地面を舐めるように飛び続けられる
			if (!_allowLanding)
			{
				pos.y = standY;
				if (m_velocity.y < 0.0f)
				{
					m_velocity.y = 0.0f;
				}
				m_isGrounded = false;
				return;
			}

			// 空中から着地した"瞬間"だけ、落下の速さを手応え(カメラ揺れ)用に記録する
			if (!wasGrounded)
			{
				m_landingImpact = -m_velocity.y;
			}

			pos.y = standY;
			m_velocity.y = 0.0f;   // 縦の勢いだけ止める(横の勢いはそのまま=着地滑りは各キャラのUpdate側で制御)
			m_isGrounded = true;
			return;
		}
	}

	// それ以外は空中
	m_isGrounded = false;
}

void CharaBase::ResolveCeiling(const Math::Vector3& fromPos, Math::Vector3& pos)
{
	ZoneScoped;	// Tracy計測(2026/07/19)：上昇中の天井判定

	// 上昇中だけ天井を見る。落下・水平移動時はResolveGround/ResolveBumpが担当
	if (m_velocity.y <= 0.0f) { return; }

	// このフレームで頭がどれだけ上がったか(=掃引区間の長さ)
	float rise = pos.y - fromPos.y;
	if (rise <= 0.0f) { return; }

	float halfH = GetScale().y * 0.5f;

	// レイの始点は「移動前の頭の高さ」。ここは前フレームに天井へ潜っていない安全な位置なので、
	// 下から上へ飛ばしても立っている床の天面を裏から拾わない(体中心より下から飛ばすと誤検知する)。
	// x,z は現在位置(頭が最終的に来る場所)を使う。射程=上昇量+余裕で高速上昇のトンネリングも拾う
	Math::Vector3 rayStart(pos.x, fromPos.y + halfH, pos.z);
	float rayRange = rise + 0.1f;

	KdCollider::RayInfo ray(KdCollider::TypeBump, rayStart, Math::Vector3::Up, rayRange);

	// デバッグ表示：天井判定に使用したレイを可視化(下向きの地面レイと色を変える=マゼンタ)
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugLine(ray.m_pos, ray.m_dir, ray.m_range, Math::Color(1.0f, 0.0f, 1.0f, 1.0f));
	}

	std::list<KdCollider::CollisionResult> retRayList;

	// 近傍の静的コリジョンだけをグリッドから取り出して判定する(大量配置時のCPU削減)
	std::vector<KdGameObject*> candidates;
	CollisionGrid::Instance().QueryRay(rayStart, Math::Vector3::Up, rayRange, candidates);
	for (KdGameObject* obj : candidates)
	{
		obj->Intersects(ray, &retRayList);
	}

	// 一番手前(下)の天井=overlapが最大のヒットを採用(ResolveGroundと同じ選び方)
	float maxOverLap = 0;
	Math::Vector3 hitPos;
	bool hit = false;

	for (auto& ret : retRayList)
	{
		if (maxOverLap < ret.m_overlapDistance)
		{
			maxOverLap = ret.m_overlapDistance;
			hitPos = ret.m_hitPos;
			hit = true;
		}
	}

	// 天井に届いていたら、頭の天面がその直下に収まる高さへ下げて上向きの勢いを止める
	if (hit)
	{
		float ceilCenterY = hitPos.y - halfH - 0.02f;   // 体中心の上限(頭が天井の少し下に来る)
		if (pos.y > ceilCenterY)
		{
			pos.y = ceilCenterY;
			m_velocity.y = 0.0f;   // 上向きの勢いだけ止める(次フレームから重力で落下)
		}
	}
}

void CharaBase::ResolveBump(Math::Vector3& pos)
{
	ZoneScoped;	// Tracy計測(2026/07/19)：球の押し出しによる壁解決

	// 体を球で近似し、Block等(TypeBump)にめり込んでいたら水平方向へ押し出して壁にする
	// ※ 縦の着地はResolveGroundが担当するので、ここは水平方向だけ押す
	float radius = DebugParams::Instance().Float(U8("キャラ/壁当たり半径"), 0.4f, 0.1f, 2.0f);

	// 球の中心の高さ。球の底が足元のほんの少し上に来るように持ち上げる。
	// こうしないと塔の天面に立ったとき球が天面に潜り込み、水平へ押し出されて引っかかる。
	// (縦の乗り上げ・着地はResolveGroundが担当。ここは壁=体の高さだけを見る)
	float centerY = pos.y - GetScale().y * 0.5f + radius + 0.02f;

	// デバッグ表示：壁当たり用の球を可視化(DebugFlags「当たり判定/AABB表示」でON/OFF)
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugSphere(Math::Vector3(pos.x, centerY, pos.z), radius, Math::Color(0.3f, 0.6f, 1.0f, 1.0f));
	}

	// 壁の接触状態は毎回ここで取り直す(この解決の時点で壁に触れているか)。
	// 壁走り(WallAction)はこの結果を見るだけで、自前の当たり判定を撃たない
	m_isTouchingWall = false;

	// 複数の壁に挟まれても安定するよう数回反復する
	for (int iter = 0; iter < 3; ++iter)
	{
		const Math::Vector3 sphereCenter(pos.x, centerY, pos.z);
		KdCollider::SphereInfo sphere(KdCollider::TypeBump, sphereCenter, radius);

		std::list<KdCollider::CollisionResult> results;
		// 近傍の静的コリジョンだけをグリッドから取り出して判定する(大量配置時のCPU削減)
		std::vector<KdGameObject*> candidates;
		CollisionGrid::Instance().QuerySphere(sphereCenter, radius, candidates);
		for (KdGameObject* obj : candidates)
		{
			obj->Intersects(sphere, &results);
		}

		// 一番深くめり込んでいる相手から押し出す。
		// ただし押し出し方向(≒面の法線)が上向きの接触は「床/歩ける斜面」なので水平押し出しの対象外にする。
		// (StagePropの凸包などの斜め面を壁扱いして水平に押すと、斜面を下へ滑ってしまうため。
		//  斜面での上下位置合わせは縦の接地ResolveGroundが担当する)。
		// しきい値: 法線Yがこれを超えたら床扱い。0=全部壁, 1=真上の面だけ床。0.5≒水平から約60°より緩い面。
		float walkableNormalY = DebugParams::Instance().Float(U8("キャラ/歩ける斜面のしきい値(法線Y)"), 0.5f, 0.0f, 1.0f);

		Math::Vector3 push = Math::Vector3::Zero;
		float maxOverlap = 0.0f;
		for (auto& ret : results)
		{
			// 床/歩ける斜面(法線が上向き)は縦の接地ResolveGroundに任せ、ここでは水平に押さない(斜面滑り防止)
			if (ret.m_hitDir.y > walkableNormalY) { continue; }

			if (ret.m_overlapDistance > maxOverlap)
			{
				maxOverlap = ret.m_overlapDistance;
				push = ret.m_hitDir * ret.m_overlapDistance;
			}
		}

		// どこにもめり込んでいなければ終わり
		if (maxOverlap <= 0.0f) { break; }

		// 水平方向だけ押し出す(縦の乗り上げ・着地はResolveGroundに任せる)
		push.y = 0.0f;
		pos += push;

		// 壁へ向かう速度成分を消して、壁に沿って滑るようにする
		if (push.LengthSquared() > 0.0f)
		{
			Math::Vector3 n = push;
			n.Normalize();

			// 壁走り用に「今どの壁に触れているか」を記録しておく。
			// pushは既に水平化(push.y=0)されているので、これがそのまま水平な壁の法線になる。
			// 最初の反復＝一番深くめり込んでいる壁を採用する(挟まれた時に主となる壁)
			if (!m_isTouchingWall)
			{
				m_isTouchingWall = true;
				m_wallNormal = n;
			}

			float into = m_velocity.Dot(n);
			if (into < 0.0f)
			{
				m_velocity -= n * into;
			}
		}
	}
}

void CharaBase::ResolveBumpSweep(const Math::Vector3& fromPos, Math::Vector3& pos)
{
	// Tracy計測(2026/07/19)：高速移動時の掃引判定。経路上に球を刻んで当てるので
	// 速度が上がるほど判定回数が増える＝ここが伸びたら刻み幅を疑う
	ZoneScoped;

	float radius = DebugParams::Instance().Float(U8("キャラ/壁当たり半径"), 0.4f, 0.1f, 2.0f);

	// 水平移動量だけを見る(縦の乗り降り・着地はResolveGroundが担当)
	Math::Vector3 delta(pos.x - fromPos.x, 0.0f, pos.z - fromPos.z);
	float dist = delta.Length();

	// 半径以下の移動ならトンネリングは起きない(体の球が必ず壁と重なる)。
	// その場合は静止時のResolveBump(重なり押し出し)に任せて、ここは何もしない。
	if (dist <= radius) { return; }

	Math::Vector3 dir = delta / dist;

	// 【トンネリング対策】以前は中心1本のレイだった。高速移動で壁の角をかすめると中心が角を
	// 通り抜けてすり抜けていた。経路を「壁当たり半径以下」のステップに刻み、各点で"体の球"を当て、
	// 進行方向が壁へ向かっている壁と最初に重なったステップの1つ手前で止める(球の幅で角も拾う/
	// 壁沿いの滑りは止めない)。最終的な密着押し出しは後段のResolveBumpに任せる。縦(y)は触らない=壁専用。
	float walkableNormalY = DebugParams::Instance().Float(U8("キャラ/歩ける斜面のしきい値(法線Y)"), 0.5f, 0.0f, 1.0f);
	float centerYOff = -GetScale().y * 0.5f + radius + 0.02f;   // 球中心の高さ=ResolveBumpと同じ
	float step = radius * 0.8f;
	int steps = (int)(dist / step) + 1;   // 切り上げ(最後のステップはt=distにクランプ)

	// デバッグ表示：スイープ経路を可視化
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugLine(Math::Vector3(fromPos.x, pos.y + centerYOff, fromPos.z), dir, dist, Math::Color(1.0f, 0.4f, 0.0f, 1.0f));
	}

	for (int i = 1; i <= steps; ++i)
	{
		float t = step * (float)i;
		if (t > dist) { t = dist; }
		float frac = t / dist;
		// サンプル位置(経路上)。yはfromPos→posを補間して斜め移動にも沿わせる
		float sy = fromPos.y + (pos.y - fromPos.y) * frac;
		Math::Vector3 sphereCenter(fromPos.x + dir.x * t, sy + centerYOff, fromPos.z + dir.z * t);

		KdCollider::SphereInfo sphere(KdCollider::TypeBump, sphereCenter, radius);
		std::list<KdCollider::CollisionResult> results;
		std::vector<KdGameObject*> candidates;
		CollisionGrid::Instance().QuerySphere(sphereCenter, radius, candidates);
		for (KdGameObject* obj : candidates)
		{
			obj->Intersects(sphere, &results);
		}

		// 進行方向が壁へ向かっている(=すり抜ける恐れがある)壁を探す。床/歩ける斜面・壁沿いは無視。
		Math::Vector3 wallNormal = Math::Vector3::Zero;   // 壁から体へ向かう押し出し方向(単位)
		bool wallHit = false;
		for (auto& ret : results)
		{
			if (ret.m_hitDir.y > walkableNormalY) { continue; }   // 床/歩ける斜面は縦の接地ResolveGroundに任せる
			if (ret.m_overlapDistance <= 0.0f) { continue; }
			float toward = -(ret.m_hitDir.x * dir.x + ret.m_hitDir.z * dir.z);   // 進行が壁へ向かう成分
			if (toward > 0.1f)
			{
				wallNormal = ret.m_hitDir;
				wallHit = true;
				break;
			}
		}
		if (wallHit)
		{
			// 1つ手前(安全)の水平位置まで戻す。step<radius なので隙間はできず、密着はResolveBumpが押し出す
			float safeT = step * (float)(i - 1);
			pos.x = fromPos.x + dir.x * safeT;
			pos.z = fromPos.z + dir.z * safeT;

			// 壁法線方向へ向かう速度成分だけ消して壁に沿って滑れるようにする(進行方向まるごとは消さない)
			float intoWall = -(m_velocity.x * wallNormal.x + m_velocity.z * wallNormal.z);
			if (intoWall > 0.0f)
			{
				m_velocity.x += wallNormal.x * intoWall;
				m_velocity.z += wallNormal.z * intoWall;

				// 手応え(カメラ揺れ)は壁に当たった瞬間だけ記録(押し付け続けでは毎フレーム揺らさない)
				if (!m_wasHittingWall)
				{
					m_wallImpact = intoWall;
				}
				m_wasHittingWall = true;
			}
			return;
		}
	}

	// 経路上に(向かってくる)壁が無ければ接触フラグを解除
	m_wasHittingWall = false;
}

void CharaBase::Jump()
{
	// 接地しているときだけジャンプできる
	if (!m_isGrounded) { return; }
	DoJump();
}

void CharaBase::DoJump()
{
	// ジャンプ初速はDebugParamsで調整可能(垂直速度に初速を与える)。接地判定はしない
	m_velocity.y = DebugParams::Instance().Float(U8("キャラ/ジャンプ力"), 8.0f, 0.0f, 30.0f);
	m_isGrounded = false;
}
