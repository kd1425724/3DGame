#include "DebugDraw.h"

#include "../DebugFlags/DebugFlags.h"

namespace
{
	// カテゴリ→DebugFlagsのキー。「デバッグ表示/」を頭に付けると
	// DebugFlagsのImGuiが自動でカテゴリごとに折り畳んで並べてくれる
	const char* CategoryKey(DebugDraw::Category _c)
	{
		switch (_c)
		{
		case DebugDraw::Category::Player:  return U8("デバッグ表示/プレイヤー");
		case DebugDraw::Category::Enemy:   return U8("デバッグ表示/敵");
		case DebugDraw::Category::Wire:    return U8("デバッグ表示/ワイヤー");
		case DebugDraw::Category::Terrain: return U8("デバッグ表示/地形");
		case DebugDraw::Category::Camera:  return U8("デバッグ表示/カメラ");
		}

		return U8("デバッグ表示/その他");
	}

	const char* kAllKey = U8("デバッグ表示/全て");

	// 一覧に並べる順序(ImGuiはキーでソートされるが、登録漏れを防ぐため明示的に持つ)
	const DebugDraw::Category kAllCategories[] =
	{
		DebugDraw::Category::Player,
		DebugDraw::Category::Enemy,
		DebugDraw::Category::Wire,
		DebugDraw::Category::Terrain,
		DebugDraw::Category::Camera,
	};
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
}
