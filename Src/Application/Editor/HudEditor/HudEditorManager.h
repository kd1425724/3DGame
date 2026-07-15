#pragma once

//====================================================
//
// HudEditorManager ── HUD(画面2D)スプライトの最小エディタ(シングルトン)
//
//  ・ImGuiでスプライトを追加/削除/パラメータ編集し、専用JSONに保存/読込する
//  ・スプライトの描画はDrawSprites()が担当(スプライトシェーダのBegin/Endで囲む)
//  ・ワールド用のLevelEditorとは別系統(あちらは3D transform、こちらは画面2D)
//
//  ※ シングルトンだがコンストラクタからInitは呼ばない(自己再入デッドロック回避)。
//    起動時の読込はApplication::InitからLoad()を明示的に呼ぶ。
//
//====================================================
class HudSprite;

class HudEditorManager
{
public:

	static HudEditorManager& Instance()
	{
		static HudEditorManager instance;
		return instance;
	}

	// ImGui編集パネル(DebugManagerから呼ぶ)
	void Draw();

	// HUDスプライトを画面に描画する(Application::DrawSpriteから呼ぶ)
	void DrawSprites() const;

	// 専用JSONへ保存/読込
	bool Save(const std::string& _filename);
	bool Load(const std::string& _filename);

private:

	HudEditorManager() = default;
	~HudEditorManager();	// vector<shared_ptr<HudSprite>>の破棄のため.cppで定義(前方宣言中)
	HudEditorManager(const HudEditorManager&) = delete;
	void operator=(const HudEditorManager&) = delete;

	std::vector<std::shared_ptr<HudSprite>>	m_spSprites;
	int										m_selected = -1;
	std::string								m_filePath = "Asset/Data/Hud/Hud.json";
	std::string								m_lastResultMessage;
};
