#pragma once

//====================================================
//
// デバッグ用フラグ管理クラス
//  ・チェックボックスでON/OFFを管理する(当たり判定の可視化など)
//  ・呼び出すだけで自動的に登録される
//
// 使い方：
//  if (DebugFlags::Instance().Get("当たり判定/AABB表示"))
//  {
//      // 可視化処理
//  }
//
//  "カテゴリ名/フラグ名" のように "/" を入れるとImGui上でカテゴリごとに
//  折り畳み表示される。"/" が無ければそのまま表示される。
//
//====================================================
class DebugFlags
{
public:

	static DebugFlags& Instance()
	{
		static DebugFlags instance;
		return instance;
	}

	// フラグを取得する(未登録の場合は defaultValue で新規登録する)
	// 参照を返すので "bool& flg = DebugFlags::Instance().Get(...)" のように
	// 保持しておいて毎フレーム読むような使い方もできる
	bool& Get(const std::string& name, bool defaultValue = false)
	{
		auto itr = m_flags.find(name);
		if (itr == m_flags.end())
		{
			itr = m_flags.emplace(name, defaultValue).first;
		}
		return itr->second;
	}

	// 値を直接書き換えたい場合
	void Set(const std::string& name, bool value)
	{
		m_flags[name] = value;
	}

	// ImGui描画
	void Draw();

private:

	DebugFlags() = default;
	~DebugFlags() = default;
	DebugFlags(const DebugFlags&) = delete;
	void operator=(const DebugFlags&) = delete;

	// キーでソートされるように map を使用(表示を安定させるため)
	std::map<std::string, bool> m_flags;
};
