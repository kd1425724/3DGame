#pragma once

//====================================================
//
// HudSprite ── HUD(画面2D)スプライト1枚ぶんのデータと描画・編集・JSON入出力
//
//  ・KdGameObjectではなく、HudEditorManagerが所有して描く「部品」
//  ・座標は画面ピクセル。Kdのスプライトは画面中央が原点(+x:右 / +y:上)
//  ・見た目1枚(テクスチャ+位置+サイズ+基準点+色+表示ON/OFF)だけを持つ最小構成
//
//====================================================
class HudSprite
{
public:

	// テクスチャを読み込む(成功時、サイズ未指定(0)なら画像の元サイズを既定にする)
	bool SetTexturePath(const std::string& _path);

	// 画面に描画する(呼び出し側でスプライトシェーダのBegin/Endで囲む前提)
	void Draw() const;

	// ImGuiでこのスプライトのパラメータを編集する
	void DrawImGuiEdit();

	// JSON入出力
	nlohmann::json ToJson() const;
	void FromJson(const nlohmann::json& _json);

	const std::string& GetName() const { return m_name; }

private:

	std::string					m_name = "Hud";
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
