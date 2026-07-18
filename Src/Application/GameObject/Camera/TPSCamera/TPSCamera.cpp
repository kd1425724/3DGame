#include "TPSCamera.h"

#include "../../../main.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Collision/CollisionGrid.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../CameraShake.h"

void TPSCamera::Init()
{
	// 親クラスの初期化呼び出し
	CameraBase::Init();

	// カメラのローカル位置の基準。x=0.5でカメラを右へずらし、プレイヤーを画面左寄りにして画面中央を見せる
	m_localBaseOffset = { 0.5f, 0.75f, -4.0f };
	m_mLocalPos = Math::Matrix::CreateTranslation(m_localBaseOffset);

	SetCursorPos(m_FixMousePos.x, m_FixMousePos.y);
}

void TPSCamera::PostUpdate()
{
	// カメラは実時間で動かす(タイムスケール非適用)。空中スロー中でも視点操作・追従が
	// 通常速度のままになり、遅い世界を普通に見渡して狙える
	const float dt = Application::Instance().GetRealDeltaTime();

	// カメラの手応え(着地・壁ヒットの揺れ)を減衰させる
	CameraShake::Instance().Update(dt);

	const std::shared_ptr<const KdGameObject>	_spTarget = m_wpTarget.lock();

	// カメラの回転(Ctrlを押している間はマウスを中央に固定しない。ImGui操作等のため)
	if (!KdInputManager::Instance().IsHold("Ctrl"))
	{
		UpdateRotateByMouse();
	}

	// ロックオン中はマウス操作より優先して、ロックオン対象の方向を向く
	if (m_isLockOn)
	{
		const std::shared_ptr<const KdGameObject> spLockOnTarget = m_wpLockOnTarget.lock();
		if (spLockOnTarget)
		{
			Math::Vector3 targetPos = spLockOnTarget->GetPos();
			targetPos.y += 0.75f;

			Math::Vector3 dir = targetPos - GetPos();
			dir.Normalize();

			// カメラの回転は m_rot ではなく m_DegAng で管理されている(GetRotationMatrix参照)
			// ※ ヨー(左右)のみロックオン対象方向に固定し、ピッチ(上下)はマウス操作のまま残す
			m_DegAng.y = DirectX::XMConvertToDegrees(atan2f(dir.x, dir.z));
		}
	}

	// === 追従対象の位置と移動速度 ===
	Math::Vector3 targetPos = Math::Vector3::Zero;
	if (_spTarget)
	{
		targetPos = _spTarget->GetPos();
	}

	// 初回フレームは現在値に合わせて、起動直後のスムージング暴れを防ぐ
	if (!m_smoothInit)
	{
		m_smoothFollowPos = targetPos;
		m_smoothDegAng    = m_DegAng;
		m_prevTargetPos   = targetPos;
		m_smoothFov       = DebugParams::Instance().Float(U8("カメラ/基準FOV"), 60.0f, 30.0f, 120.0f);
		m_smoothInit      = true;
	}

	// 対象の移動速度(m/s)。Cのカメラ引きに使う
	float targetSpeed = 0.0f;
	if (dt > 0.0f)
	{
		targetSpeed = (targetPos - m_prevTargetPos).Length() / dt;
	}
	m_prevTargetPos = targetPos;

	// === A: 追従スムージング(注視点をLerpで遅れて寄せる=振り子軌道の急な揺れを吸収) ===
	float followK = DebugParams::Instance().Float(U8("カメラ/追従スムーズ"), 12.0f, 1.0f, 60.0f);
	float followT = std::clamp(followK * dt, 0.0f, 1.0f);   // 値が大きいほど追従が速い(=スムージング弱)
	m_smoothFollowPos = Math::Vector3::Lerp(m_smoothFollowPos, targetPos, followT);

	// === B: 回転スムージング(視点角度を最短経路でLerp=ロックオンのスナップやマウス急変を和らげる) ===
	float rotK = DebugParams::Instance().Float(U8("カメラ/回転スムーズ"), 20.0f, 1.0f, 60.0f);
	float rotT = std::clamp(rotK * dt, 0.0f, 1.0f);
	auto lerpAngle = [](float cur, float tgt, float t)
	{
		// 角度差を[-180,180]に畳んで最短方向に補間する(ヨーが720度等に累積してもクルッと回らない)
		float d = tgt - cur;
		while (d > 180.0f)
		{
			d -= 360.0f;
		}
		while (d < -180.0f)
		{
			d += 360.0f;
		}
		return cur + d * t;
	};
	m_smoothDegAng.x = lerpAngle(m_smoothDegAng.x, m_DegAng.x, rotT);
	m_smoothDegAng.y = lerpAngle(m_smoothDegAng.y, m_DegAng.y, rotT);
	m_smoothDegAng.z = lerpAngle(m_smoothDegAng.z, m_DegAng.z, rotT);

	// === C: 速度に応じてカメラを後ろへ引く(情報量を減らして酔い軽減。FOVは一定に保つ) ===
	float pullPerSpeed = DebugParams::Instance().Float(U8("カメラ/速度引き量"), 0.06f, 0.0f, 1.0f);
	float pullMax      = DebugParams::Instance().Float(U8("カメラ/速度引き上限"), 3.0f, 0.0f, 15.0f);
	float pullTarget   = std::clamp(targetSpeed * pullPerSpeed, 0.0f, pullMax);
	// 引き量も急に変えると酔うので、追従と同じレートで滑らかに寄せる
	m_smoothPullback  += (pullTarget - m_smoothPullback) * followT;

	// === F: 速度に応じてFOVを少し広げる(疾走感) ===
	// 広げすぎると酔うので上限つき＋追従と同レートで平滑化。FOV自体はDebugParamsで無効化(量0)も可
	float fovBase     = DebugParams::Instance().Float(U8("カメラ/基準FOV"),     60.0f, 30.0f, 120.0f);
	float fovPerSpeed = DebugParams::Instance().Float(U8("カメラ/速度FOV量"),    0.4f,  0.0f,  3.0f);
	float fovMaxAdd   = DebugParams::Instance().Float(U8("カメラ/速度FOV上限"), 12.0f,  0.0f, 40.0f);
	float fovTarget   = fovBase + std::clamp(targetSpeed * fovPerSpeed, 0.0f, fovMaxAdd);
	m_smoothFov      += (fovTarget - m_smoothFov) * followT;
	if (m_spCamera)
	{
		m_spCamera->SetProjectionMatrix(m_smoothFov);
	}

	// === カメラ行列の構築 ===
	// ローカル位置の基準(Initで設定したm_localBaseOffset)に、速度ぶんの引きをzへ足して後ろへ下げる
	m_mLocalPos = Math::Matrix::CreateTranslation(
		m_localBaseOffset.x, m_localBaseOffset.y, m_localBaseOffset.z - m_smoothPullback);
	// 平滑化した角度で回転行列を作る(CameraBase::GetRotationMatrixは生のm_DegAngを使うのでここでは使わない)
	m_mRotation = Math::Matrix::CreateFromYawPitchRoll(
		DirectX::XMConvertToRadians(m_smoothDegAng.y),
		DirectX::XMConvertToRadians(m_smoothDegAng.x),
		DirectX::XMConvertToRadians(m_smoothDegAng.z));
	// 注視点は平滑化した追従位置を使う
	Math::Matrix _targetMat = Math::Matrix::CreateTranslation(m_smoothFollowPos);
	m_mWorld = m_mLocalPos * m_mRotation * _targetMat;

	// ↓めり込み防止の為の座標補正計算↓
	// ①当たり判定(レイ判定)用の情報作成
	KdCollider::RayInfo rayInfo;
	// レイの発射位置を設定
	rayInfo.m_pos = GetPos();

	// レイの発射方向を設定
	rayInfo.m_dir = Math::Vector3::Down;
	// レイの長さを設定
	rayInfo.m_range = 1000.f;
	if (_spTarget)
	{
		Math::Vector3 _targetPos = _spTarget->GetPos();
		_targetPos.y += 0.1f;
		rayInfo.m_dir = _targetPos - GetPos();
		rayInfo.m_range = rayInfo.m_dir.Length();
		rayInfo.m_dir.Normalize();
	}

	// 当たり判定をしたいタイプを設定
	// 地面(TypeGround)だけでなく塔などのBlock(TypeBump)にもカメラを遮らせる
	rayInfo.m_type = KdCollider::TypeGround | KdCollider::TypeBump;

	// ②カメラ〜プレイヤーのレイ近傍の静的コリジョンだけをbroadphase(CollisionGrid)から取り出して判定する。
	// ※ 以前は全オブジェクト総当たりだったが、建物を大量に置くと重くなるためグリッド経由に変更。
	//   グリッドには地面/建物(IStaticCollider)が載っており、CharaBase側の当たりと同じ対象を見る。
	std::list<KdCollider::CollisionResult> retRayList;
	std::vector<KdGameObject*> camCandidates;
	CollisionGrid::Instance().QueryRay(rayInfo.m_pos, rayInfo.m_dir, rayInfo.m_range, camCandidates);
	for (KdGameObject* spGameObj : camCandidates)
	{
		spGameObj->Intersects(rayInfo, &retRayList);
	}

	// ③ 結果を使って座標を補完する
	// レイに当たったリストから一番遮った(overlapが最大の)障害物を検出
	float maxOverLap = 0;
	Math::Vector3 hitPos = {};
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
	if (hit)
	{
		// 何かしらの障害物に当たっている。障害物の手前にカメラを寄せる
		Math::Vector3 _hitPos = hitPos;
		_hitPos += rayInfo.m_dir * 0.4f;
		SetPos(_hitPos);
	}

	// 手応え(着地・壁ヒット)の揺れを最後に加える。オフセットは視点ローカルなので
	// カメラの回転で世界方向へ直してから平行移動に足す
	Math::Vector3 shakeOffset = CameraShake::Instance().GetOffset();
	if (shakeOffset.LengthSquared() > 0.0f)
	{
		Math::Vector3 shakeWorld = Math::Vector3::TransformNormal(shakeOffset, m_mRotation);
		SetPos(GetPos() + shakeWorld);
	}

	// === 地面より下にカメラが潜らないようにする(上を向いた時の潜り防止) ===
	// 上を向くとカメラが後ろ下へ回り込み、地面(上向き片面)の下に潜る→地面が透明に見え、
	// 画面中央のワイヤーが地面に刺さる。カメラ→プレイヤーのレイは地面を下から通り抜けてしまい防げないので、
	// カメラのXZ真上から下へレイを飛ばして地面の高さを取り、その少し上までカメラを持ち上げる。
	{
		Math::Vector3 _camPos = GetPos();

		KdCollider::RayInfo _groundRay;
		_groundRay.m_pos = Math::Vector3(_camPos.x, _camPos.y + 50.0f, _camPos.z);
		_groundRay.m_dir = Math::Vector3::Down;
		_groundRay.m_range = 1000.0f;
		_groundRay.m_type = KdCollider::TypeGround;

		std::list<KdCollider::CollisionResult> _groundHits;
		std::vector<KdGameObject*> _groundCandidates;
		CollisionGrid::Instance().QueryRay(_groundRay.m_pos, _groundRay.m_dir, _groundRay.m_range, _groundCandidates);
		for (KdGameObject* _obj : _groundCandidates)
		{
			_obj->Intersects(_groundRay, &_groundHits);
		}

		// 一番高い地面ヒットを採用(段差地形でも一番上に合わせる)
		float _groundY = -1e30f;
		bool _foundGround = false;
		for (auto& _h : _groundHits)
		{
			if (_h.m_hitPos.y > _groundY)
			{
				_groundY = _h.m_hitPos.y;
				_foundGround = true;
			}
		}

		const float _floorMargin = DebugParams::Instance().Float(U8("カメラ/地面からの最低高さ"), 0.3f, 0.0f, 3.0f);
		if (_foundGround && _camPos.y < _groundY + _floorMargin)
		{
			_camPos.y = _groundY + _floorMargin;
			SetPos(_camPos);
		}
	}
}
