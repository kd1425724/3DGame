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
}
