#include "LevelPicker.h"

#include <cmath>

#include "../../main.h"
#include "../LevelEditorManager.h"
#include "../LevelEditorHistory/LevelEditorHistory.h"
#include "../../Scene/SceneManager.h"
#include "../../GameObject/Camera/EditorCamera/EditorCamera.h"

namespace
{
	// レイと点の最短距離を求める
	// ※ カメラより後ろにある点を誤って拾わないよう、tを0以上にクランプしている
	float ClosestDistanceRayToPoint(const Math::Vector3& rayPos, const Math::Vector3& rayDir, const Math::Vector3& point)
	{
		Math::Vector3 toPoint = point - rayPos;

		float t = toPoint.Dot(rayDir);
		if (t < 0.0f) { t = 0.0f; }

		Math::Vector3 closestPointOnRay = rayPos + rayDir * t;

		return Math::Vector3::Distance(closestPointOnRay, point);
	}

	// 2直線 P1+s*d1(軸) と P2+t*d2(レイ) が最も近づく時の s, t を求める
	// (d1, d2 は単位ベクトルでなくてもよい)
	// 戻り値：求まったかどうか(2直線がほぼ平行だと求まらない)
	bool ClosestParamsBetweenLines(const Math::Vector3& p1, const Math::Vector3& d1,
									const Math::Vector3& p2, const Math::Vector3& d2,
									float& outS, float& outT)
	{
		Math::Vector3 r = p1 - p2;

		float a = d1.Dot(d1);
		float b = d1.Dot(d2);
		float c = d2.Dot(d2);
		float d = d1.Dot(r);
		float e = d2.Dot(r);

		float denom = a * c - b * b;
		if (std::fabs(denom) < 1e-5f) { return false; }

		outS = (b * e - c * d) / denom;
		outT = (a * e - b * d) / denom;
		return true;
	}
}

void LevelPicker::Update()
{
	LevelEditorManager& mgr = LevelEditorManager::Instance();

	// エディタが無効なら何もしない
	if (!mgr.m_enabled)
	{
		m_dragMode = DragMode::None;
		return;
	}

	// ImGui側がマウス入力を使っている間(ウィンドウ操作中など)は何もしない
	// ※ この判定はImGuiのNewFrame()で更新されるため、実際には1フレーム遅れの値になる点に注意
	if (ImGui::GetIO().WantCaptureMouse)
	{
		m_dragMode = DragMode::None;
		return;
	}

	// レイキャストの起点となるカメラが必要なので、編集用フリーカメラが有効な間のみ動作する
	std::shared_ptr<EditorCamera> spCamera = mgr.GetEditorCamera();
	if (!spCamera)
	{
		m_dragMode = DragMode::None;
		return;
	}

	bool mouseDown = KdInputManager::Instance().IsHold("LeftClick");

	if (!mouseDown)
	{
		// マウスを離したらドラッグ終了
		m_dragMode = DragMode::None;
		return;
	}

	bool justPressed = KdInputManager::Instance().IsPress("LeftClick");

	// マウス座標(クライアント座標)からレイを生成
	POINT clientPos;
	GetCursorPos(&clientPos);
	ScreenToClient(Application::Instance().GetWindowHandle(), &clientPos);

	Math::Vector3 rayPos, rayDir;
	float rayRange = 0.0f;
	spCamera->GetCamera()->GenerateRayInfoFromClientPos(clientPos, rayPos, rayDir, rayRange);

	if (justPressed)
	{
		std::shared_ptr<KdGameObject> selected = mgr.GetSelected();

		// ---- 1. まず移動ギズモの軸(X/Y/Z)を掴んだかどうかを判定する ----
		if (selected)
		{
			const Math::Vector3 axisDirs[3] =
			{
				Math::Vector3(1, 0, 0),
				Math::Vector3(0, 1, 0),
				Math::Vector3(0, 0, 1),
			};

			Math::Vector3 objPos = selected->GetPos();

			int bestAxis = -1;
			float bestDist = mgr.m_gizmoGrabRadius;
			float bestS = 0.0f;

			for (int i = 0; i < 3; i++)
			{
				float s, t;
				if (!ClosestParamsBetweenLines(objPos, axisDirs[i], rayPos, rayDir, s, t)) { continue; }

				// ギズモの見た目の長さの範囲内で掴んだ場合のみ対象にする
				if (s < 0.0f || s > mgr.m_gizmoLength) { continue; }

				Math::Vector3 pointOnAxis = objPos + axisDirs[i] * s;
				Math::Vector3 pointOnRay = rayPos + rayDir * t;
				float dist = Math::Vector3::Distance(pointOnAxis, pointOnRay);

				if (dist < bestDist)
				{
					bestDist = dist;
					bestAxis = i;
					bestS = s;
				}
			}

			if (bestAxis >= 0)
			{
				LevelEditorHistory::Instance().PushUndo();

				// 軸ドラッグ開始
				m_dragMode = DragMode::Axis;
				m_dragAxisDir = axisDirs[bestAxis];
				m_dragAxisStartObjPos = objPos;
				m_dragAxisStartS = bestS;

				// グループ移動用に、選択中全オブジェクトの開始座標を記録しておく
				m_dragGroupStartPos.clear();
				for (auto& obj : mgr.GetSelectedList())
				{
					m_dragGroupStartPos.emplace_back(obj, obj->GetPos());
				}
				return;
			}
		}

		// ---- 2. 軸を掴んでいなければ、選択中オブジェクト本体を掴んだかどうかを判定する ----
		if (selected && ClosestDistanceRayToPoint(rayPos, rayDir, selected->GetPos()) < m_grabRadius)
		{
			LevelEditorHistory::Instance().PushUndo();

			// カメラに正対する平面上での自由ドラッグ開始
			m_dragMode = DragMode::FreePlane;
			m_dragPlaneNormal = spCamera->GetMatrix().Backward();
			m_dragPlaneNormal.Normalize();
			m_dragPlanePoint = selected->GetPos();

			// グループ移動用に、選択中全オブジェクトの開始座標を記録しておく
			m_dragGroupStartPos.clear();
			for (auto& obj : mgr.GetSelectedList())
			{
				m_dragGroupStartPos.emplace_back(obj, obj->GetPos());
			}
			return;
		}

		// ---- 3. どちらでもなければ、今まで通り「一番近いオブジェクトを選択」する ----
		m_dragMode = DragMode::None;

		std::shared_ptr<KdGameObject> nearestObj;
		float nearestDist = m_pickRadius;

		for (auto& obj : SceneManager::Instance().GetObjList())
		{
			if (!obj) { continue; }
			if (obj == spCamera) { continue; } // 自分自身(編集用カメラ)は選択対象外

			float dist = ClosestDistanceRayToPoint(rayPos, rayDir, obj->GetPos());

			if (dist < nearestDist)
			{
				nearestDist = dist;
				nearestObj = obj;
			}
		}

		if (nearestObj)
		{
			// Ctrlを押しながらのクリックは複数選択(選択済みなら外す/未選択なら追加する)
			if (KdInputManager::Instance().IsHold("Ctrl"))
			{
				mgr.ToggleSelected(nearestObj);
			}
			else
			{
				mgr.SetSelected(nearestObj);
			}
		}

		return;
	}

	// ---- 押しっぱなし：ドラッグ中の追従処理 ----
	std::shared_ptr<KdGameObject> selected = mgr.GetSelected();
	if (!selected)
	{
		m_dragMode = DragMode::None;
		return;
	}

	if (m_dragMode == DragMode::Axis)
	{
		// 軸上の最近接点を求め、掴んだ時点からの移動量(delta)だけオブジェクトを動かす
		// (毎回「今の最近接点」に飛ばすのではなく、掴んだ瞬間からの差分を使うことで
		//  ドラッグ開始時に位置がガクッとズレるのを防いでいる)
		float s, t;
		if (ClosestParamsBetweenLines(m_dragAxisStartObjPos, m_dragAxisDir, rayPos, rayDir, s, t))
		{
			float delta = s - m_dragAxisStartS;

			// 主選択の新しい座標(グリッドスナップ込み)を求め、実際に動いた量を
			// 選択中の他オブジェクトにもそのまま適用して一緒に追従させる
			Math::Vector3 newPrimaryPos = mgr.SnapPos(m_dragAxisStartObjPos + m_dragAxisDir * delta);
			Math::Vector3 appliedDelta = newPrimaryPos - m_dragAxisStartObjPos;

			for (auto& [wpObj, startPos] : m_dragGroupStartPos)
			{
				std::shared_ptr<KdGameObject> obj = wpObj.lock();
				if (obj) { obj->SetPos(startPos + appliedDelta); }
			}
		}
	}
	else if (m_dragMode == DragMode::FreePlane)
	{
		float denom = m_dragPlaneNormal.Dot(rayDir);

		// 平面とレイがほぼ平行(視線と同じ向き)の場合は交点が求まらないので何もしない
		if (std::fabs(denom) > 1e-5f)
		{
			float t = m_dragPlaneNormal.Dot(m_dragPlanePoint - rayPos) / denom;

			if (t > 0.0f)
			{
				Math::Vector3 newPrimaryPos = mgr.SnapPos(rayPos + rayDir * t);
				Math::Vector3 appliedDelta = newPrimaryPos - m_dragPlanePoint;

				for (auto& [wpObj, startPos] : m_dragGroupStartPos)
				{
					std::shared_ptr<KdGameObject> obj = wpObj.lock();
					if (obj) { obj->SetPos(startPos + appliedDelta); }
				}
			}
		}
	}
}
