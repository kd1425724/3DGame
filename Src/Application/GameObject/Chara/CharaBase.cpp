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
	pos += m_velocity * deltaTime;

	// あたる側の設定＝＝＝＝＝＝＝＝＝＝
	// レイの始点は少し上(rayStartUp)、長さは「開始高さ + モデルの半分 + このフレームの落下量 + 余裕」
	// で可変にする。落下量を足すことで、高速落下しても地面をすり抜けにくくする(=可変レイ判定)
	const float rayStartUp = 1.0f;
	float fallThisFrame = (m_velocity.y < 0.0f) ? (-m_velocity.y * deltaTime) : 0.0f;
	float rayRange = rayStartUp + (GetScale().y * 0.5f) + fallThisFrame + 0.1f;

	KdCollider::RayInfo ray(KdCollider::TypeGround, pos + Math::Vector3(0, rayStartUp, 0), Math::Vector3::Down, rayRange);

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
			pos.y = standY;
			m_velocity.y = 0.0f;   // 縦の勢いだけ止める(横の勢いはそのまま=着地滑りは各キャラのUpdate側で制御)
			m_isGrounded = true;
			SetPos(pos);
			return;
		}
	}

	// それ以外は空中：重力で移動した位置をそのまま適用する
	m_isGrounded = false;
	SetPos(pos);
}

void CharaBase::Jump()
{
	// 接地しているときだけジャンプできる
	if (!m_isGrounded) { return; }

	// ジャンプ初速はDebugParamsで調整可能(垂直速度に初速を与える)
	m_velocity.y = DebugParams::Instance().Float(U8("キャラ/ジャンプ力"), 8.0f, 0.0f, 30.0f);
	m_isGrounded = false;
}
