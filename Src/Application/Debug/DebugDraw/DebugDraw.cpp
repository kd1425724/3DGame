#include "DebugDraw.h"

#include "../DebugFlags/DebugFlags.h"
#include "../DebugWatch/DebugWatch.h"   // [TEXT3D] 切り分け用の計測。原因が確定したら外す

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

	void DrawText3D()
	{
		// [TEXT3D] 切り分け用の計測。原因が確定したらこのタグで grep して撤去する。
		//   ここが0なら「積む側(AddText3D)まで届いていない」、0以外なら「描く側の問題」
		DebugWatch::Instance().Watch("[TEXT3D] 積まれた数", static_cast<int>(s_texts3D.size()));

		if (s_texts3D.empty()) { return; }

		// 【なぜカメラを引数で受け取らないか】この関数はImGuiのフレーム内(描画の後)から
		//   呼ばれるので、そこまでカメラを渡す配線が要る。GPUへ送るカメラ用の定数バッファは
		//   CameraBase::PreDrawが毎フレーム更新していて中身が同じものなので、そこから読む。
		//   ※ 影の深度パスはこのバッファを書き換えないので、ここで読めるのは常にゲームのカメラ
		const KdShaderManager::cbCamera& camera = KdShaderManager::Instance().GetCameraCB();
		const Math::Matrix viewProj = camera.mView * camera.mProj;

		ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
		const ImVec2 screenSize = ImGui::GetIO().DisplaySize;

		// 文字が点のど真ん中に乗ると読みにくいので少し上へずらす
		constexpr float kOffsetY = -8.0f;

		for (const Text3DEntry& entry : s_texts3D)
		{
			const Math::Vector4 clip = Math::Vector4::Transform(
				Math::Vector4(entry.worldPos.x, entry.worldPos.y, entry.worldPos.z, 1.0f), viewProj);

			// カメラの後ろにある点は捨てる。wが負のまま割ると符号が反転して
			// 「振り向くと背後の文字が画面の反対側に出る」という誤表示になる
			if (clip.w <= 0.0f) { continue; }

			const float ndcX = clip.x / clip.w;
			const float ndcY = clip.y / clip.w;

			// NDC(-1〜1・Yは上が正) → スクリーン座標(左上原点・Yは下が正)
			const ImVec2 screenPos(
				(ndcX * 0.5f + 0.5f) * screenSize.x,
				(-ndcY * 0.5f + 0.5f) * screenSize.y + kOffsetY);

			// [TEXT3D] 切り分け用。文字と同じ座標に塗り潰しの丸を出す。
			//   丸は出るのに文字が出ない → AddText(フォント)側の問題
			//   丸も出ない                → 座標変換か、この関数まで来ていない
			//   丸が変な場所に出る        → 座標変換の問題
			pDrawList->AddCircleFilled(screenPos, 4.0f, IM_COL32(255, 0, 255, 255));

			// [TEXT3D] 1件目の画面座標を出す(画面外に飛んでいないかを数値で見る)
			if (&entry == &s_texts3D.front())
			{
				DebugWatch::Instance().Watch("[TEXT3D] 1件目X", screenPos.x);
				DebugWatch::Instance().Watch("[TEXT3D] 1件目Y", screenPos.y);
				DebugWatch::Instance().Watch("[TEXT3D] 画面幅", screenSize.x);
			}

			pDrawList->AddText(screenPos, IM_COL32(255, 220, 60, 255), entry.text.c_str());
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
