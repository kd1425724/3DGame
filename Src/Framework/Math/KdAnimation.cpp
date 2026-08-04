#include "KdAnimation.h"
#include "../Direct3D/KdModel.h"

// 二分探索で、指定時間から次の配列要素のKeyIndexを求める関数
// list		… キー配列
// time		… 時間
// 戻り値	… 次の配列要素のIndex
template<class T>
int BinarySearchNextAnimKey(const std::vector<T>& list, float time)
{
	int low = 0;
	int high = (int)list.size();
	while (low < high)
	{
		int mid = (low + high) / 2;
		float midTime = list[mid].m_time;

		if (midTime <= time) low = mid + 1;
		else high = mid;
	}
	return low;
}

bool KdAnimationData::Node::InterpolateTranslations(Math::Vector3& result, float time)
{
	if (m_translations.size() == 0)return false;

	// キー位置検索
	UINT keyIdx = BinarySearchNextAnimKey(m_translations, time);

	// 先頭のキーなら、先頭のデータを返す
	if (keyIdx == 0) {
		result = m_translations.front().m_vec;
		return true;
	}
	// 配列外のキーなら、最後のデータを返す
	else if (keyIdx >= m_translations.size()) {
		result = m_translations.back().m_vec;
		return true;
	}
	// それ以外(中間の時間)なら、その時間の値を補間計算で求める
	else {
		auto& prev = m_translations[keyIdx - 1];	// 前のキー
		auto& next = m_translations[keyIdx];		// 次のキー
		// 前のキーと次のキーの時間から、0～1間の時間を求める
		float f = (time - prev.m_time) / (next.m_time - prev.m_time);
		// 補間
		result = DirectX::XMVectorLerp(
			prev.m_vec,
			next.m_vec,
			f
		);
	}

	return true;
}

bool KdAnimationData::Node::InterpolateRotations(Math::Quaternion& result, float time)
{
	if (m_rotations.size() == 0)return false;

	// キー位置検索
	UINT keyIdx = BinarySearchNextAnimKey(m_rotations, time);
	// 先頭のキーなら、先頭のデータを返す
	if (keyIdx == 0) {
		result = m_rotations.front().m_quat;
	}
	// 配列外のキーなら、最後のデータを返す
	else if (keyIdx >= m_rotations.size()) {
		result = m_rotations.back().m_quat;
	}
	// それ以外(中間の時間)なら、その時間の値を補間計算で求める
	else {
		auto& prev = m_rotations[keyIdx - 1];	// 前のキー
		auto& next = m_rotations[keyIdx];		// 次のキー
		// 前のキーと次のキーの時間から、0～1間の時間を求める
		float f = (time - prev.m_time) / (next.m_time - prev.m_time);
		// 補間
		result = DirectX::XMQuaternionSlerp(
			prev.m_quat,
			next.m_quat,
			f
		);
	}

	return true;
}

bool KdAnimationData::Node::InterpolateScales(Math::Vector3& result, float time)
{
	if (m_scales.size() == 0)return false;

	// キー位置検索
	UINT keyIdx = BinarySearchNextAnimKey(m_scales, time);

	// 先頭のキーなら、先頭のデータを返す
	if (keyIdx == 0) {
		result = m_scales.front().m_vec;
		return true;
	}
	// 配列外のキーなら、最後のデータを返す
	else if (keyIdx >= m_scales.size()) {
		result = m_scales.back().m_vec;
		return true;
	}
	// それ以外(中間の時間)なら、その時間の値を補間計算で求める
	else {
		auto& prev = m_scales[keyIdx - 1];	// 前のキー
		auto& next = m_scales[keyIdx];		// 次のキー
		// 前のキーと次のキーの時間から、0～1間の時間を求める
		float f = (time - prev.m_time) / (next.m_time - prev.m_time);
		// 補間
		result = DirectX::XMVectorLerp(
			prev.m_vec,
			next.m_vec,
			f
		);
	}

	return true;
}

void KdAnimationData::Node::Interpolate(Math::Matrix& rDst, float time)
{
	// ベクターによる拡縮補間
	bool isChange = false;
	Math::Matrix scale;
	Math::Vector3 resultVec;
	if (InterpolateScales(resultVec, time))
	{
		scale = scale.CreateScale(resultVec);
		isChange = true;
	}

	// クォタニオンによる回転補間
	Math::Matrix rotate;
	Math::Quaternion resultQuat;
	if (InterpolateRotations(resultQuat, time))
	{
		rotate = rotate.CreateFromQuaternion(resultQuat);
		isChange = true;
	}

	// ベクターによる座標補間
	Math::Matrix trans;
	if (InterpolateTranslations(resultVec, time))
	{
		trans = trans.CreateTranslation(resultVec);
		isChange = true;
	}

	if (isChange)
	{
		rDst = scale * rotate * trans;
	}
}

// 【2026-08-04 追加：クロスフェード】切り替え直前のポーズを控える。
// 新しいアニメが触るノードだけでよい(触らないノードは値が変わらない＝そもそも飛びようがない)
void KdAnimator::TakeBlendSnapshot(const std::vector<KdModelWork::Node>& rNodes)
{
	m_blendPoses.clear();

	if (!m_spAnimation) { return; }

	m_blendPoses.resize(m_spAnimation->m_nodes.size());

	for (size_t i = 0; i < m_spAnimation->m_nodes.size(); ++i)
	{
		UINT idx = m_spAnimation->m_nodes[i].m_nodeOffset;

		// Decomposeは非constなので、ノードの行列をコピーしてから分解する
		Math::Matrix local = rNodes[idx].m_localTransform;

		BlendPose& rPose = m_blendPoses[i];
		rPose.m_valid = local.Decompose(rPose.m_scale, rPose.m_rotate, rPose.m_translate);
	}
}

// 【2026-08-04 追加：クロスフェード】rate=0で混ぜ元、1で新しいポーズになるよう rDst を書き換える
void KdAnimator::BlendPoseInto(Math::Matrix& rDst, const BlendPose& prev, float rate)
{
	if (!prev.m_valid) { return; }

	Math::Vector3		scale;
	Math::Quaternion	rotate;
	Math::Vector3		translate;

	// Decomposeは非constなのでコピーを取ってから分解する
	Math::Matrix now = rDst;
	if (!now.Decompose(scale, rotate, translate)) { return; }

	// 拡縮と位置は線形、回転はslerpで混ぜる。
	// 行列の成分をそのまま線形に混ぜると、回転の途中で軸の長さが縮んで形が潰れる
	scale     = Math::Vector3::Lerp(prev.m_scale, scale, rate);
	rotate    = Math::Quaternion::Slerp(prev.m_rotate, rotate, rate);
	translate = Math::Vector3::Lerp(prev.m_translate, translate, rate);

	rDst = Math::Matrix::CreateScale(scale)
		* Math::Matrix::CreateFromQuaternion(rotate)
		* Math::Matrix::CreateTranslation(translate);
}

void KdAnimator::AdvanceTime(std::vector<KdModelWork::Node>& rNodes, float speed)
{
	if (!m_spAnimation) { return; }

	// 【2026-08-04 追加】混ぜ元の採取。
	// この時点のノードには、まだ切り替え前のアニメの結果が残っている
	if (m_needsBlendSnapshot)
	{
		TakeBlendSnapshot(rNodes);
		m_needsBlendSnapshot = false;
	}

	const bool isBlending = IsBlending();

	// m_blendTimeが0のときは isBlending が false になるので、ここで0除算は起きない
	const float blendRate = isBlending ? (m_blendElapsed / m_blendTime) : 1.0f;

	// 全てのアニメーションノード（モデルの行列を補間する情報）の行列補間を実行する
	for (size_t i = 0; i < m_spAnimation->m_nodes.size(); ++i)
	{
		auto& rAnimNode = m_spAnimation->m_nodes[i];

		// 対応するモデルノードのインデックス
		UINT idx = rAnimNode.m_nodeOffset;

		// ※ ここにあった prev(m_localTransformの控え)は、代入するだけで一度も読んでいない
		//   変数だったため、クロスフェードを足すときにコメント化した。
		//   「前のポーズ」が必要なのは混ぜる処理のほうで、それは m_blendPoses が持っている
		// auto prev = rNodes[idx].m_localTransform;

		// アニメーションデータによる行列補間
		rAnimNode.Interpolate(rNodes[idx].m_localTransform, m_time);

		// 切り替えた直後は、前のポーズから徐々に移す
		if (isBlending && i < m_blendPoses.size())
		{
			BlendPoseInto(rNodes[idx].m_localTransform, m_blendPoses[i], blendRate);
		}
	}

	// アニメーションのフレームを進める
	m_time += speed;

	// アニメーションデータの最後のフレームを超えたら
	if (m_time >= m_spAnimation->m_maxLength)
	{
		if (m_isLoop)
		{
			// アニメーションの最初に戻る（ループさせる
			m_time = 0.0f;
		}
		else
		{
			m_time = m_spAnimation->m_maxLength;
		}
	}

	// 【2026-08-04 追加】クロスフェードを進める。
	// speedと同じ歩幅で進めるので、再生を遅くすると混ぜる時間も一緒に伸びる
	// (集中スロー中に切り替えだけ一瞬で終わると浮くため、そろえてある)。
	// 逆再生(speedが負)でも混ぜ終わるよう、絶対値で足す
	if (isBlending)
	{
		m_blendElapsed += (speed < 0.0f) ? -speed : speed;

		if (m_blendElapsed >= m_blendTime)
		{
			m_blendElapsed = m_blendTime;

			// 混ぜ終わったら控えは不要
			m_blendPoses.clear();
			m_blendPoses.shrink_to_fit();
		}
	}
}
