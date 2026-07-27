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
