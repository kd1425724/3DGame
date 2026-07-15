#include "CharaBase.h"

#include "../../main.h"
#include "../../Scene/SceneManager.h"
#include "../../Debug/DebugParams/DebugParams.h"

void CharaBase::SetAsset(const std::string& assetName)
{
	m_modelWork.SetModelData(KdAssets::Instance().m_modeldatas.GetData(assetName));
}

void CharaBase::DrawLit()
{
	if (!m_modelWork.IsEnable()) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld, m_color);
}

void CharaBase::GroundCheck()
{
	float deltaTime = Application::Instance().GetDeltaTime();

	// 重力を垂直速度に加える(重力はDebugParamsで調整可能)
	float gravity = DebugParams::Instance().Float(U8("キャラ/重力"), 20.0f, 0.0f, 100.0f);
	m_velocity.y -= gravity * deltaTime;

	// 速度で位置を進める(縦横まとめて)。水平速度は各キャラのUpdateが設定する
	// ※ Enemyは水平をUpdateで直接動かすためm_velocity.xzは0のまま＝縦だけ動く
	Math::Vector3 pos = GetPos();
	Math::Vector3 startPos = pos;   // 移動前の位置(スイープの始点)
	pos += m_velocity * deltaTime;

	// 地面に潜っていたら押し上げて落下を止める(接地状態もここで更新される)
	ResolveGround(pos);

	// 高速移動で壁を飛び越える(トンネリング)のを先に止める
	ResolveBumpSweep(startPos, pos);

	// 壁(TypeBump=Block等)にめり込んでいたら水平に押し出す
	ResolveBump(pos);

	SetPos(pos);
}

void CharaBase::ResolveGround(Math::Vector3& pos)
{
	float deltaTime = Application::Instance().GetDeltaTime();

	// この呼び出しの前に接地していたか(着地の"瞬間"だけ手応えを出すためのエッジ検出用)
	bool wasGrounded = m_isGrounded;

	// あたる側の設定＝＝＝＝＝＝＝＝＝＝
	// レイの始点は少し上(rayStartUp)、長さは「開始高さ + モデルの半分 + このフレームの落下量 + 余裕」
	// で可変にする。落下量を足すことで、高速落下しても地面をすり抜けにくくする(=可変レイ判定)
	const float rayStartUp = 1.0f;
	float fallThisFrame = (m_velocity.y < 0.0f) ? (-m_velocity.y * deltaTime) : 0.0f;
	float rayRange = rayStartUp + (GetScale().y * 0.5f) + fallThisFrame + 0.1f;

	// 地面(TypeGround)に加えてBlock等(TypeBump)の天面にも乗れるようにする
	KdCollider::RayInfo ray(KdCollider::TypeGround | KdCollider::TypeBump, pos + Math::Vector3(0, rayStartUp, 0), Math::Vector3::Down, rayRange);

	// デバッグ表示：地面判定に使用したレイを可視化
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
		m_pDebugWire->AddDebugLine(ray.m_pos, ray.m_dir, ray.m_range, Math::Color(1.0f, 1.0f, 0.0f, 1.0f));
	}

	// レイに当たったオブジェクトを格納するリストを作成
	std::list<KdCollider::CollisionResult> retRayList;

	// 全オブジェクトと当たり判定を行う
	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }

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
			// 空中から着地した"瞬間"だけ、落下の速さを手応え(カメラ揺れ)用に記録する
			if (!wasGrounded) { m_landingImpact = -m_velocity.y; }

			pos.y = standY;
			m_velocity.y = 0.0f;   // 縦の勢いだけ止める(横の勢いはそのまま=着地滑りは各キャラのUpdate側で制御)
			m_isGrounded = true;
			return;
		}
	}

	// それ以外は空中
	m_isGrounded = false;
}

void CharaBase::ResolveBump(Math::Vector3& pos)
{
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
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
		m_pDebugWire->AddDebugSphere(Math::Vector3(pos.x, centerY, pos.z), radius, Math::Color(0.3f, 0.6f, 1.0f, 1.0f));
	}

	// 複数の壁に挟まれても安定するよう数回反復する
	for (int iter = 0; iter < 3; ++iter)
	{
		KdCollider::SphereInfo sphere(KdCollider::TypeBump, Math::Vector3(pos.x, centerY, pos.z), radius);

		std::list<KdCollider::CollisionResult> results;
		for (auto& obj : SceneManager::Instance().GetObjList())
		{
			if (!obj) { continue; }
			obj->Intersects(sphere, &results);
		}

		// 一番深くめり込んでいる相手から押し出す
		Math::Vector3 push = Math::Vector3::Zero;
		float maxOverlap = 0.0f;
		for (auto& ret : results)
		{
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
			float into = m_velocity.Dot(n);
			if (into < 0.0f) { m_velocity -= n * into; }
		}
	}
}

void CharaBase::ResolveBumpSweep(const Math::Vector3& fromPos, Math::Vector3& pos)
{
	float radius = DebugParams::Instance().Float(U8("キャラ/壁当たり半径"), 0.4f, 0.1f, 2.0f);

	// 水平移動量だけを見る(縦の乗り降り・着地はResolveGroundが担当)
	Math::Vector3 delta(pos.x - fromPos.x, 0.0f, pos.z - fromPos.z);
	float dist = delta.Length();

	// 半径以下の移動ならトンネリングは起きない(体の球が必ず壁と重なる)。
	// その場合は静止時のResolveBump(重なり押し出し)に任せて、ここは何もしない。
	if (dist <= radius) { return; }

	Math::Vector3 dir = delta / dist;

	// レイを飛ばす高さは体の中心あたり(ResolveBumpの球中心と同じ)
	float sampleY = pos.y - GetScale().y * 0.5f + radius + 0.02f;
	Math::Vector3 origin(fromPos.x, sampleY, fromPos.z);

	// レイ長は「移動量 + 半径」。半径ぶん伸ばすのは、体の表面が壁に触れる所で止めたいから
	KdCollider::RayInfo ray(KdCollider::TypeBump, origin, dir, dist + radius);

	// デバッグ表示：スイープに使ったレイを可視化
	if (KdGameObject::s_showColliderDebug)
	{
		if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
		m_pDebugWire->AddDebugLine(ray.m_pos, ray.m_dir, ray.m_range, Math::Color(1.0f, 0.4f, 0.0f, 1.0f));
	}

	std::list<KdCollider::CollisionResult> results;
	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }
		obj->Intersects(ray, &results);
	}

	// 一番手前(overlapが最大=originから最も近い)の壁を採用する
	float maxOverlap = 0.0f;
	Math::Vector3 hitPos;
	bool hit = false;
	for (auto& ret : results)
	{
		if (ret.m_overlapDistance > maxOverlap)
		{
			maxOverlap = ret.m_overlapDistance;
			hitPos = ret.m_hitPos;
			hit = true;
		}
	}
	if (!hit) { m_wasHittingWall = false; return; }   // 壁から離れたら接触フラグを解除

	// 壁の手前(半径ぶん外側)まで水平位置を戻す。縦(y)はいじらない
	Math::Vector3 stop = hitPos - dir * radius;
	pos.x = stop.x;
	pos.z = stop.z;

	// 壁へ向かう水平速度成分を消して、壁に沿って滑るようにする
	float into = m_velocity.Dot(dir);
	if (into > 0.0f)
	{
		// 手応え(カメラ揺れ)は"壁に当たった瞬間"だけ記録する。押し付け続けても毎フレーム
		// 揺らさない(連続攻撃で壁の向こうの敵へ突っ込み続けるとシェイクが終わらない不具合の対策)
		if (!m_wasHittingWall) { m_wallImpact = into; }
		m_wasHittingWall = true;
		m_velocity -= dir * into;
	}
	else
	{
		m_wasHittingWall = false;   // 壁に沿って離れる向きなら接触解除
	}
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
