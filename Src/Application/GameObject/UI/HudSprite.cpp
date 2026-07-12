#include "HudSprite.h"

bool HudSprite::SetTexturePath(const std::string& _path)
{
	m_texturePath = _path;

	auto tex = std::make_shared<KdTexture>();
	if (!tex->Load(_path))
	{
		m_spTexture = nullptr;
		return false;
	}
	m_spTexture = tex;

	// サイズ未指定(0)なら画像の元サイズを既定にする
	if (m_width  == 0) { m_width  = static_cast<int>(m_spTexture->GetInfo().Width); }
	if (m_height == 0) { m_height = static_cast<int>(m_spTexture->GetInfo().Height); }

	return true;
}

void HudSprite::Draw() const
{
	if (!m_visible || !m_spTexture) { return; }

	int w = (m_width  > 0) ? m_width  : static_cast<int>(m_spTexture->GetInfo().Width);
	int h = (m_height > 0) ? m_height : static_cast<int>(m_spTexture->GetInfo().Height);

	KdShaderManager::Instance().m_spriteShader.DrawTex(
		m_spTexture.get(), m_posX, m_posY, w, h, nullptr, &m_color, m_pivot);
}

void HudSprite::DrawImGuiEdit()
{
	ImGui::InputText(U8("名前"), &m_name);

	ImGui::InputText(U8("画像パス"), &m_texturePath);
	ImGui::SameLine();
	if (ImGui::Button(U8("読込"))) { SetTexturePath(m_texturePath); }

	if (!m_spTexture)
	{
		ImGui::TextDisabled(U8("(テクスチャ未読込)"));
	}

	ImGui::Checkbox(U8("表示"), &m_visible);

	int pos[2] = { m_posX, m_posY };
	if (ImGui::DragInt2(U8("位置(px)"), pos)) { m_posX = pos[0]; m_posY = pos[1]; }

	int size[2] = { m_width, m_height };
	if (ImGui::DragInt2(U8("サイズ(px)"), size)) { m_width = size[0]; m_height = size[1]; }

	float pivot[2] = { m_pivot.x, m_pivot.y };
	if (ImGui::DragFloat2(U8("基準点(0-1)"), pivot, 0.01f, 0.0f, 1.0f)) { m_pivot.x = pivot[0]; m_pivot.y = pivot[1]; }

	float col[4] = { m_color.x, m_color.y, m_color.z, m_color.w };
	if (ImGui::ColorEdit4(U8("色"), col)) { m_color = Math::Color(col[0], col[1], col[2], col[3]); }
}

nlohmann::json HudSprite::ToJson() const
{
	nlohmann::json j;
	j["name"]    = m_name;
	j["texture"] = m_texturePath;
	j["pos"]     = { m_posX, m_posY };
	j["size"]    = { m_width, m_height };
	j["pivot"]   = { m_pivot.x, m_pivot.y };
	j["color"]   = { m_color.x, m_color.y, m_color.z, m_color.w };
	j["visible"] = m_visible;
	return j;
}

void HudSprite::FromJson(const nlohmann::json& _json)
{
	if (_json.contains("name"))    { m_name = _json["name"].get<std::string>(); }
	if (_json.contains("pos"))     { m_posX = _json["pos"].at(0).get<int>(); m_posY = _json["pos"].at(1).get<int>(); }
	if (_json.contains("size"))    { m_width = _json["size"].at(0).get<int>(); m_height = _json["size"].at(1).get<int>(); }
	if (_json.contains("pivot"))   { m_pivot = Math::Vector2(_json["pivot"].at(0).get<float>(), _json["pivot"].at(1).get<float>()); }
	if (_json.contains("color"))   { m_color = Math::Color(_json["color"].at(0).get<float>(), _json["color"].at(1).get<float>(), _json["color"].at(2).get<float>(), _json["color"].at(3).get<float>()); }
	if (_json.contains("visible")) { m_visible = _json["visible"].get<bool>(); }

	// テクスチャは最後に読み込む(上でm_width/m_heightを設定済みなら元サイズで上書きしない)
	if (_json.contains("texture")) { SetTexturePath(_json["texture"].get<std::string>()); }
}
