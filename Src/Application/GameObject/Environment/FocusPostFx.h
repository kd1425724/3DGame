#pragma once

//====================================================
//
// FocusPostFx ── 空中スロー(エアフォーカス)中の画面演出
//
//  ・Application::GetTimeScale() を読み、スローが掛かっている間だけ
//      被写界深度(背景ボケ) ＋ Bloom強調(高輝度しきい値ダウン=広く光る)
//    を効かせ、通常時は無効(全面くっきり)へ戻す
//  ・Playerには一切触らない。timeScaleは既にPlayerが設定済みで、この演出は
//    「スローが掛かっているか」だけを見て自律的に反応する(描画とgameplayの分離)
//  ・遷移は実時間(GetRealDeltaTime)で滑らかにLerp。スロー中でも演出の切替は等速で進む
//  ・KdPostProcessShader経由で DoF / Bright の定数バッファを毎フレーム設定する
//    (PostEffectProcessはもとから毎フレーム走っているので負荷は変わらない)
//
//  ※ 描画パスには出ないロジックのみのKdGameObject。GameScene::Initで常駐させる
//
//====================================================
class FocusPostFx : public KdGameObject
{
public:

	void Update() override;

private:

	// 0=通常の見た目 / 1=フルスロー演出。目標値へ実時間で滑らかに寄せる(急な切替を防ぐ)
	float m_blend = 0.0f;
};
