#pragma once

//====================================================
//
// HudSprite ── HUD(画面2D)スプライト1枚ぶんのデータと描画・編集・JSON入出力
//
//  ・KdGameObject派生。ただしシーンには入れず、HudEditorManagerが「部品」として所有して描く
//    (KdGameObjectを継承するのは「オブジェクトは全てKdGameObject」方針に合わせるため。
//     名前は基底のm_name/SetName/GetNameを流用する)
//  ・座標は画面ピクセル。Kdのスプライトは画面中央が原点(+x:右 / +y:上)
//  ・見た目1枚(テクスチャ+位置+サイズ+基準点+色+表示ON/OFF)だけを持つ最小構成
//
//====================================================
// ※ KdGameObjectはPch.hの強制インクルードで見えているため、継承でも明示includeは不要
class HudSprite : public KdGameObject
{
public:

	// 既定の名前を付ける(名前は基底KdGameObjectのm_nameを使う)
	HudSprite() { m_name = "Hud"; }

	// テクスチャを読み込む(成功時、サイズ未指定(0)なら画像の元サイズを既定にする)
	bool SetTexturePath(const std::string& _path);

	// 画面に描画する(呼び出し側でスプライトシェーダのBegin/Endで囲む前提)
	void Draw() const;

	// ImGuiでこのスプライトのパラメータを編集する
	void DrawImGuiEdit();

	// JSON入出力
	nlohmann::json ToJson() const;
	void FromJson(const nlohmann::json& _json);

	// ※ 名前の取得は基底 KdGameObject::GetName() を使う(m_nameも基底のものを流用)

private:

	std::string					m_texturePath;
	std::shared_ptr<KdTexture>	m_spTexture;

	int				m_posX = 0;					// 画面中央からのオフセット(px)
	int				m_posY = 0;
	int				m_width = 0;				// 0なら画像の元サイズを使う
	int				m_height = 0;
	Math::Vector2	m_pivot = { 0.5f, 0.5f };	// 基準点(0.0〜1.0)
	Math::Color		m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool			m_visible = true;
};
