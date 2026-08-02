#pragma once

//====================================================
//
// デバッグ表示のカテゴリ切り替え
//  ・DebugFlags の「デバッグ表示/〜」を読んで、種類ごとに出す/出さないを決める
//  ・以前は s_showColliderDebug ひとつで全部まとめてON/OFFするしかなく、
//    見たいもの以外(カメラの当たり判定など)まで画面に出て邪魔だった
//
// 使い方：
//  if (DebugDraw::IsOn(DebugDraw::Category::Wire))
//  {
//      // ワイヤーの可視化
//  }
//
// ※ 「全て」がONのときは、個別のチェックに関わらず全部出る
//
//====================================================
namespace DebugDraw
{
	// 表示の種類。DebugFlags の「デバッグ表示/〜」に1対1で対応する
	enum class Category : int
	{
		Player,     // プレイヤーの当たり判定・索敵/攻撃範囲
		Enemy,      // 敵の当たり判定・索敵/攻撃範囲
		Wire,       // ワイヤーの拘束範囲・アンカー・探索レイ
		Terrain,    // 地面・建物の当たり判定
		Camera,     // カメラの壁寄せレイ(常に画面内に出るので独立させてある)
	};

	// そのカテゴリを描画してよいか
	bool IsOn(Category _category);

	// どれか1つでもONか。「そもそもデバッグ描画をするか」の大枠の判定に使う
	// (KdGameObject::s_showColliderDebug に入れる値)
	bool IsAnyOn();

	// ImGuiのチェックリストを描く(DebugFlagsのウィンドウから呼ばれる)。
	// 未登録のカテゴリをここで登録するので、起動直後から一覧に並ぶ
	void RegisterAll();

	//------------------------------------------------
	// ワールド座標に文字を出す(値を「その場所」で読みたいとき用)
	//------------------------------------------------
	// KdDebugWireFrameは線しか描けず、DebugWatchは別ウィンドウに数値が並ぶだけなので、
	// 「どの関節がどのHPか」のように【位置と値の対応】を見たい用途に応えられない。
	//
	// 【描き方】ワールド座標を KdCamera::ConvertWorldToScreenDetail でスクリーン座標へ
	// 変換し、2D描画パス(KdSpriteShader)で文字を出す。2D描画は3Dの絵が全部出た後に
	// 深度テスト無しで行われるので、【体の内側にある関節でもモデルに埋まらない】。
	//
	// 値を持っているのは更新中(Update/PostUpdate)で、描けるのは2Dパスなので、
	// 1フレーム分ためてから描く。
	//  ・AddText3D  … 更新中どこからでも積む
	//  ・DrawText3D … Application::DrawSprite から呼ぶ(2D描画パス)
	//  ・ClearText3D… DebugManager::BeginFrame から呼ぶ(DebugWatchと同じ流儀)

	// ワールド座標に文字を積む(実際に描かれるのはこのフレームの2D描画パス)
	void AddText3D(const Math::Vector3& _worldPos, const std::string& _text);

	// 積まれた文字をスクリーン座標へ変換して描く。2D描画パスから呼ぶ
	void DrawText3D();

	// 前フレームぶんを捨てる
	void ClearText3D();

	// 座標変換に使うカメラを渡す。CameraBase::PreDrawから毎フレーム設定する
	// (Effekseerへカメラを渡しているのと同じ場所・同じ理由)
	void SetCamera(const std::shared_ptr<KdCamera>& _spCamera);

	// KdGameObject::DrawDebug() は s_showColliderDebug しか見ないため、
	// 登録済みコライダーの可視化(KdColliderが緑の球などで描く)がカテゴリを無視して
	// 全部出てしまう。基底を呼ぶ前後でフラグを一時的に絞るための入れ物。
	//
	//   void Enemy::DrawDebug()
	//   {
	//       DebugDraw::ScopedGate gate(DebugDraw::Category::Enemy);
	//       KdGameObject::DrawDebug();
	//   }
	//
	// ※ Framework(KdGameObject)を変更せずにカテゴリ分けするための手段
	class ScopedGate
	{
	public:
		explicit ScopedGate(Category _category);
		~ScopedGate();

		ScopedGate(const ScopedGate&) = delete;
		void operator=(const ScopedGate&) = delete;

	private:
		bool m_prev = false;
	};
}
