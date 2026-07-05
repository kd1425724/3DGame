#pragma once

//====================================================
//
// デバッグ用モニタークラス
//  ・プレイヤーなどの状態を数値として毎フレーム確認するための読み取り専用表示
//  ・座標、体力、フラグなどをその場でImGui上に一覧表示する
//
// 使い方：
//  各オブジェクトの Update などで毎フレーム呼び出す
//   DebugWatch::Instance().Watch("Player/座標", m_pos);
//   DebugWatch::Instance().Watch("Player/体力", m_hp);
//
//  値は毎フレーム上書きされ、呼ばれなかった項目は自動的に表示から消える
//  (オブジェクトが消滅した場合など、古い情報が残り続けないようにするため)
//
//====================================================
class DebugWatch
{
public:

	static DebugWatch& Instance()
	{
		static DebugWatch instance;
		return instance;
	}

	void Watch(const std::string& name, float value);
	void Watch(const std::string& name, int value);
	void Watch(const std::string& name, const Math::Vector3& value);
	void Watch(const std::string& name, bool value);
	void Watch(const std::string& name, const std::string& value);

	// 毎フレームの更新開始時に呼び出す(前フレーム分の情報をクリアする)
	// ※ DebugManager::BeginFrame から呼ばれるので基本的に直接呼ぶ必要はない
	void BeginFrame();

	// ImGui描画
	void Draw();

private:

	DebugWatch() = default;
	~DebugWatch() = default;
	DebugWatch(const DebugWatch&) = delete;
	void operator=(const DebugWatch&) = delete;

	enum class ValueType
	{
		Float,
		Int,
		Vector3,
		Bool,
		String,
	};

	struct Entry
	{
		ValueType		type = ValueType::Float;
		float			f = 0.0f;
		int				i = 0;
		Math::Vector3	v = {};
		bool			b = false;
		std::string		s;
	};

	std::map<std::string, Entry> m_watches;
};
