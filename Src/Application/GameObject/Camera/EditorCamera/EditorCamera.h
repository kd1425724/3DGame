#pragma once

#include "../CameraBase.h"

//====================================================
//
// 編集モード用フリーカメラ
//  ・WASDで水平移動、Q/Eで上下移動、マウスで視点回転(CameraBase::UpdateRotateByMouseを流用)
//  ・Shiftを押している間は移動速度が上がる
//  ・ターゲット(注視対象)は持たない、完全に自由な視点移動カメラ
//
//====================================================
class EditorCamera : public CameraBase
{
public:

	EditorCamera()				{}
	~EditorCamera() override	{}

	void Init()			override;
	void PostUpdate()	override;

private:

	// 1秒あたりの移動速度
	float m_moveSpeed = 10.0f;

	// Shiftを押している間の速度倍率
	float m_boostRate = 3.0f;
};
