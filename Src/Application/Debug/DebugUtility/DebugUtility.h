#pragma once

//====================================================
//
// Debug〇〇クラス共通のちょっとした補助機能
//  ・"カテゴリ名/表示名" という名前をカテゴリごとにツリー表示するための処理
//
//====================================================
namespace DebugUtil
{
	// "カテゴリ名/表示名" 形式の文字列を分割する
	// 区切りが無ければ category は空文字になる
	inline void SplitCategory(const std::string& fullName, std::string& category, std::string& label)
	{
		size_t pos = fullName.find('/');
		if (pos == std::string::npos)
		{
			category.clear();
			label = fullName;
		}
		else
		{
			category = fullName.substr(0, pos);
			label = fullName.substr(pos + 1);
		}
	}

	// 表示名の頭に付いた「数字_」を取り除く。
	// DebugFlags/DebugParamsはmapでキー順に並ぶので、日本語の名前だとバイト順になって
	// 意図した並びにできない。キーに "1_プレイヤー" のように順番の印を付けておき、
	// 表示するときだけここで落とす("1_" は出ない)
	inline std::string StripOrderPrefix(const std::string& label)
	{
		size_t i = 0;
		while (i < label.size() && label[i] >= '0' && label[i] <= '9')
		{
			++i;
		}
		// 数字が1文字以上あって、その直後が '_' のときだけ印とみなす
		if (i > 0 && i < label.size() && label[i] == '_')
		{
			return label.substr(i + 1);
		}

		return label;
	}

	// names に入っている名前を "カテゴリ名/表示名" に従ってグループ分けし、
	// 各項目に対して drawFunc(フルネーム, 表示名) を呼び出す
	// ・カテゴリが付いている項目は折り畳み可能なツリーにまとめる
	// ・カテゴリが無い項目はそのまま並べる
	template<typename DrawFunc>
	void DrawGroupedByCategory(const std::vector<std::string>& names, DrawFunc drawFunc)
	{
		// カテゴリごとに項目名を振り分ける
		std::map<std::string, std::vector<std::string>> grouped;
		for (const std::string& name : names)
		{
			std::string category, label;
			SplitCategory(name, category, label);
			grouped[category].push_back(name);
		}

		for (auto& [category, list] : grouped)
		{
			if (category.empty())
			{
				// カテゴリ無し：そのまま並べる
				for (const std::string& name : list)
				{
					std::string dummy, label;
					SplitCategory(name, dummy, label);
					drawFunc(name, label);
				}
			}
			else
			{
				// カテゴリ有り：折り畳みツリーにまとめる
				if (ImGui::TreeNodeEx(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (const std::string& name : list)
					{
						std::string dummy, label;
						SplitCategory(name, dummy, label);
						drawFunc(name, label);
					}
					ImGui::TreePop();
				}
			}
		}
	}
}
