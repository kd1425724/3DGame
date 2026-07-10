//=====================================================================
//
//  WireAction ── ワイヤーアクションの物理(距離拘束)を担う部品
//
//  ・KdGameObjectではなく、Playerなどが「部品」として1つ持つ想定
//  ・プレイヤーの位置と3D速度を渡すと、ワイヤーの距離拘束を解いて返す
//  ・見た目(ワイヤーの描画)は含まない。ここは物理だけを担当する
//
//  ※ この設計(インターフェイス)はたたき台。使いにくければ自由に変えてよい
//  ※ .cppの中身はコメントだけ。埋めるまではビルドが通らない(returnが無いため)
//
//=====================================================================
#pragma once

class WireAction
{
public:

	// 照準方向へワイヤーを撃ち、当たった地形をアンカー(固定点)にする
	//  _from      ... ワイヤーの発射位置(手元など)
	//  _dir       ... 発射方向(正規化済みを想定)
	//  _maxLength ... ワイヤーが届く最大距離
	//  戻り値     ... 命中してアンカーが決まったか
	bool Shoot(const Math::Vector3& _from, const Math::Vector3& _dir, float _maxLength);

	// ワイヤーを外す(拘束を解除する)
	void Release();

	// 今ワイヤーが繋がっているか
	bool IsAttached() const;

	// 毎フレームの拘束処理。_pos / _vel を拘束後の値に書き換える
	//  _reelInput ... +1でたぐり寄せ(縮む) / -1で伸ばす / 0で維持
	void Update(Math::Vector3& _pos, Math::Vector3& _vel, float _dt, float _reelInput);

	// アンカー(ワイヤーの先端)を取得する(描画側でワイヤーを引くのに使う)
	const Math::Vector3& GetAnchor() const;

private:

	// --- 必要なメンバのたたき台(自由に増減してよい) ---

	// 繋がっているか
	bool m_isAttached = false;

	// 固定点(ワイヤーの先端)
	Math::Vector3 m_anchor;

	// 拘束半径(=現在のワイヤー長)。これより外へは出られない
	float m_length = 0.0f;
};
