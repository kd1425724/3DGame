#pragma once

// アニメーションキー(クォータニオン
struct KdAnimKeyQuaternion
{
	float				m_time = 0;		// 時間
	Math::Quaternion	m_quat;			// クォータニオンデータ
};

// アニメーションキー(ベクトル
struct KdAnimKeyVector3
{
	float				m_time = 0;		// 時間
	Math::Vector3		m_vec;			// 3Dベクトルデータ
};

//============================
// アニメーションデータ
//============================
struct KdAnimationData
{
	// アニメーション名
	std::string		m_name;
	// アニメの長さ
	float			m_maxLength = 0;

	// １ノードのアニメーションデータ
	struct Node
	{
		int			m_nodeOffset = -1;	// 対象モデルノードのOffset値

		// 各チャンネル
		std::vector<KdAnimKeyVector3>		m_translations;	// 位置キーリスト
		std::vector<KdAnimKeyQuaternion>	m_rotations;	// 回転キーリスト
		std::vector<KdAnimKeyVector3>		m_scales;		// 拡縮キーリスト

		void Interpolate(Math::Matrix& rDst, float time);
		bool InterpolateTranslations(Math::Vector3& result, float time);
		bool InterpolateRotations(Math::Quaternion& result, float time);
		bool InterpolateScales(Math::Vector3& result, float time);
	};

	// 全ノード用アニメーションデータ
	std::vector<Node>	m_nodes;
};

class KdAnimator
{
public:

	// blendTime … 切り替え前のポーズから混ぜる時間。
	//   単位はAdvanceTimeへ渡すspeedと同じ(＝60fps基準のフレーム数)。
	//   0なら従来どおりの即差し替え。既定が0なので、引数を足していない既存の呼び出しは挙動が変わらない
	//
	// 【2026-08-04 追加：クロスフェード】
	//   これが無いと、アニメを切り替えた瞬間にポーズが1フレームで別の形へ飛ぶ
	//   (走る→止まる、歩き→倒れる など)。
	//   混ぜる元になる「切り替え直前のポーズ」はモデル側のノードにしか無くここでは受け取れないので、
	//   次のAdvanceTimeで採取する(その時点のノードには、まだ前のアニメの結果が残っている)
	inline void SetAnimation(const std::shared_ptr<KdAnimationData>& rData, bool isLoop = true, float blendTime = 0.0f)
	{
		m_spAnimation = rData;
		m_isLoop = isLoop;

		m_time = 0.0f;

		m_blendTime = (blendTime > 0.0f) ? blendTime : 0.0f;
		m_blendElapsed = 0.0f;
		m_needsBlendSnapshot = (m_blendTime > 0.0f);
	}

	// アニメーションが終了してる？
	bool IsAnimationEnd() const
	{
		if (m_spAnimation == nullptr) { return true; }
		if (m_time >= m_spAnimation->m_maxLength) { return true; }

		return false;
	}

	// 再生位置と全長(どちらも60fps基準のフレーム数)。
	// 【2026-08-04 追加】攻撃の当たり判定を「アニメの何フレーム目から何フレーム目まで」で
	//   開け閉めするために要る。IsAnimationEnd()だけでは「終わったか」しか分からない
	float GetTime() const { return m_time; }
	float GetMaxLength() const { return m_spAnimation ? m_spAnimation->m_maxLength : 0.0f; }

	// クロスフェードの最中か
	bool IsBlending() const { return m_blendElapsed < m_blendTime; }

	// アニメーションの更新
	void AdvanceTime(std::vector<KdModelWork::Node>& rNodes, float speed = 1.0f);

private:

	// 【2026-08-04 追加】クロスフェードの混ぜ元(切り替え直前のポーズ)。
	// 行列の成分をそのまま線形に混ぜると回転の途中で形が潰れるので、
	// 拡縮・回転・位置に分けて持ち、回転だけslerpで混ぜられるようにしてある
	struct BlendPose
	{
		bool				m_valid = false;	// 分解できなかったノードは混ぜずに新しいポーズを使う
		Math::Vector3		m_scale = { 1.0f, 1.0f, 1.0f };
		Math::Quaternion	m_rotate;
		Math::Vector3		m_translate = {};
	};

	// 切り替え直前のポーズを控える(混ぜ元の採取)
	void TakeBlendSnapshot(const std::vector<KdModelWork::Node>& rNodes);

	// rate=0で混ぜ元、1で新しいポーズになるよう rDst を書き換える
	static void BlendPoseInto(Math::Matrix& rDst, const BlendPose& prev, float rate);

	std::shared_ptr<KdAnimationData>	m_spAnimation = nullptr;	// 再生するアニメーションデータ

	float m_time = 0.0f;

	bool m_isLoop = false;

	// クロスフェード。m_blendPosesは m_spAnimation->m_nodes と同じ並び・同じ個数
	std::vector<BlendPose>	m_blendPoses;
	float					m_blendTime = 0.0f;
	float					m_blendElapsed = 0.0f;
	bool					m_needsBlendSnapshot = false;
};
