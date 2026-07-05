#pragma once

//====================================================
//
// デバッグ用パラメータ調整クラス
//  ・コードを書き換えずにその場でパラメータを調整するための機能
//  ・呼び出した時点で未登録なら自動登録される(2回目以降は登録された値を返す)
//  ・Save / Load ボタンでファイルに保存・復元できる(調整結果を残せる)
//
// 使い方：
//  float speed = DebugParams::Instance().Float("Player/移動速度", 5.0f, 0.0f, 20.0f);
//  int   hp    = DebugParams::Instance().Int("Enemy/HP", 100);
//  bool  invincible = DebugParams::Instance().Bool("Player/無敵");
//
//  "カテゴリ名/名前" のように "/" を入れるとImGui上でカテゴリごとに
//  折り畳み表示される。
//
//  min == max (両方0のままなど)の場合は範囲制限無しの自由入力(Drag)になる
//
//====================================================
class DebugParams
{
public:

	static DebugParams& Instance()
	{
		static DebugParams instance;
		return instance;
	}

	// float パラメータを取得(未登録なら defaultValue で登録)
	float& Float(const std::string& name, float defaultValue = 0.0f, float min = 0.0f, float max = 0.0f);

	// int パラメータを取得(未登録なら defaultValue で登録)
	int& Int(const std::string& name, int defaultValue = 0, int min = 0, int max = 0);

	// Vector3 パラメータを取得(未登録なら defaultValue で登録)
	Math::Vector3& Vector3Param(const std::string& name, const Math::Vector3& defaultValue = {});

	// bool パラメータを取得(未登録なら defaultValue で登録)
	bool& Bool(const std::string& name, bool defaultValue = false);

	// ImGui描画(Save/Loadボタン付き)
	void Draw();

	// 現在のパラメータをファイルに保存(フォルダが無ければ自動作成する)
	// 戻り値：保存できたかどうか
	bool Save(const std::string& filename = "Asset/Data/Debug/DebugParams/DebugParams.json");

	// ファイルからパラメータを読み込む
	// ※ Float/Int/... で登録される前に呼んでも問題ない
	//    (登録時に読み込み済みの値があればそちらを優先して使う)
	// 戻り値：読み込めたかどうか(ファイルが無い場合はfalseを返すだけで正常)
	bool Load(const std::string& filename = "Asset/Data/Debug/DebugParams/DebugParams.json");

private:

	DebugParams() = default;
	~DebugParams() = default;
	DebugParams(const DebugParams&) = delete;
	void operator=(const DebugParams&) = delete;

	enum class ParamType
	{
		Float,
		Int,
		Vector3,
		Bool,
	};

	struct FloatParam { float value = 0.0f; float min = 0.0f; float max = 0.0f; };
	struct IntParam { int value = 0; int min = 0; int max = 0; };

	// 登録済みパラメータ名 → 種類(まとめて描画する際に参照する)
	std::map<std::string, ParamType> m_registry;

	std::map<std::string, FloatParam>		m_floats;
	std::map<std::string, IntParam>		m_ints;
	std::map<std::string, Math::Vector3>	m_vectors;
	std::map<std::string, bool>			m_bools;

	// Load() で読み込んだが、まだ Float/Int/... で登録されていない値の一時置き場
	std::map<std::string, float>			m_pendingFloats;
	std::map<std::string, int>				m_pendingInts;
	std::map<std::string, Math::Vector3>	m_pendingVectors;
	std::map<std::string, bool>			m_pendingBools;

	// Save/LoadボタンでImGuiに一言表示するための直近の結果メッセージ
	std::string m_lastResultMessage;
};
