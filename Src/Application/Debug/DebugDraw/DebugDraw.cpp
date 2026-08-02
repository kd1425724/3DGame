#include "DebugDraw.h"

#include "../DebugFlags/DebugFlags.h"

namespace
{
	// カテゴリ→DebugFlagsのキー。「デバッグ表示/」を頭に付けると
	// DebugFlagsのImGuiが自動でカテゴリごとに折り畳んで並べてくれる
	// 名前の頭の「数字_」は並び順を決めるための印で、ImGuiには出ない
	// (DebugFlagsはmapでキー順に並ぶため、これが無いと日本語のバイト順になって
	//  意図した並びにできない。DebugUtil::StripOrderPrefix が表示時に取り除く)
	const char* CategoryKey(DebugDraw::Category _c)
	{
		switch (_c)
		{
		case DebugDraw::Category::Player:  return U8("デバッグ表示/1_プレイヤー");
		case DebugDraw::Category::Enemy:   return U8("デバッグ表示/2_敵");
		case DebugDraw::Category::Wire:    return U8("デバッグ表示/3_ワイヤー");
		case DebugDraw::Category::Terrain: return U8("デバッグ表示/4_地形");
		case DebugDraw::Category::Camera:  return U8("デバッグ表示/5_カメラ");
		}

		return U8("デバッグ表示/9_その他");
	}

	const char* kAllKey = U8("デバッグ表示/0_全て");

	// 一覧に並べる順序(ImGuiはキーでソートされるが、登録漏れを防ぐため明示的に持つ)
	const DebugDraw::Category kAllCategories[] =
	{
		DebugDraw::Category::Player,
		DebugDraw::Category::Enemy,
		DebugDraw::Category::Wire,
		DebugDraw::Category::Terrain,
		DebugDraw::Category::Camera,
	};

	// ワールド座標に出す文字の1フレーム分の溜め場
	struct Text3DEntry
	{
		Math::Vector3	worldPos;
		std::string		text;
	};
	std::vector<Text3DEntry> s_texts3D;

	// ワールド→スクリーン変換に使うカメラ(CameraBase::PreDrawが毎フレーム設定する)
	std::weak_ptr<KdCamera> s_wpCamera;
}

namespace DebugDraw
{
	bool IsOn(Category _category)
	{
		// 「全て」は個別のチェックを上書きする
		if (DebugFlags::Instance().Get(kAllKey, false)) { return true; }

		return DebugFlags::Instance().Get(CategoryKey(_category), false);
	}

	bool IsAnyOn()
	{
		if (DebugFlags::Instance().Get(kAllKey, false)) { return true; }

		for (Category c : kAllCategories)
		{
			if (DebugFlags::Instance().Get(CategoryKey(c), false)) { return true; }
		}

		return false;
	}

	void RegisterAll()
	{
		// DebugFlagsは「Getした時に登録される」遅延登録なので、
		// 一度も通らないカテゴリは一覧に出てこない。起動時にまとめて登録しておく
		DebugFlags::Instance().Get(kAllKey, false);
		for (Category c : kAllCategories)
		{
			DebugFlags::Instance().Get(CategoryKey(c), false);
		}
	}

	void AddText3D(const Math::Vector3& _worldPos, const std::string& _text)
	{
		s_texts3D.push_back({ _worldPos, _text });
	}

	void ClearText3D()
	{
		s_texts3D.clear();
	}

	void SetCamera(const std::shared_ptr<KdCamera>& _spCamera)
	{
		s_wpCamera = _spCamera;
	}

	void DrawText3D()
	{
		if (s_texts3D.empty()) { return; }

		std::shared_ptr<KdCamera> spCamera = s_wpCamera.lock();
		if (!spCamera) { return; }

		const Math::Color kTextColor(1.0f, 0.86f, 0.24f, 1.0f);

		// 文字が点のど真ん中に乗ると読みにくいので少し上へずらす(2Dなので画面上方向は-Y)
		constexpr float kOffsetY = -10.0f;

		for (const Text3DEntry& entry : s_texts3D)
		{
			// ワールド座標 → スクリーン座標。原点は画面中央で、2D描画の座標系と一致する
			Math::Vector3 screen{};
			spCamera->ConvertWorldToScreenDetail(entry.worldPos, screen);

			// zにはw(カメラから見た奥行き)が入っている。0以下＝カメラの後ろ。
			// 割った後の値は符号が反転していて、画面の反対側に出てしまうので捨てる
			if (screen.z <= 0.0f) { continue; }

			// 【書式文字列に文字列を直接渡さない】DrawFontはprintf形式なので、
			// 文字に % が含まれると書式指定として解釈されて壊れる
			KdShaderManager::Instance().m_spriteShader.DrawFont(
				Math::Vector2(screen.x, screen.y + kOffsetY), &kTextColor, "%s", entry.text.c_str());
		}
	}

	ScopedGate::ScopedGate(Category _category)
	{
		m_prev = KdGameObject::s_showColliderDebug;
		KdGameObject::s_showColliderDebug = m_prev && IsOn(_category);
	}

	ScopedGate::~ScopedGate()
	{
		KdGameObject::s_showColliderDebug = m_prev;
	}
}
