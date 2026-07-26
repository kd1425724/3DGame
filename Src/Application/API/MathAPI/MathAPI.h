#pragma once

//====================================================
//
// デフォルトでは用意されていない計算をまとめた関数群
//  ・安全な正規化(ゼロ長ガード付き)／水平(xz)成分の取り出し
//  ・一定の割合で目標値に近づける(Lerp)処理(Vector3版/float版)
//  ・Y軸回転で目標方向を向く処理(内積で角度差・外積で回転方向を判定)
//
// 【方針1】1フレームに何百回も呼ばれる極小関数は、ヘッダに inline で置く。
//   Release構成に WholeProgramOptimization(LTCG) が無いため(Project.vcxproj:175〜)、
//   .cpp に定義すると他ファイルからインライン展開されず関数呼び出しのコストが残る。
//   計算量のある関数(RotateToDirection等)だけ .cpp に置く。
//
// 【方針2】関数名は実在するエンジンの公式名に合わせる。出典は各関数のコメントに書く。
//   本作の都合で名前を変えたものは、その理由もコメントに残す。
//
//====================================================
namespace MathAPI
{
	// ベクトルを「ゼロと見なす」長さの二乗のしきい値。
	// ※ 比較対象は Length() ではなく LengthSquared()。0.0001f は長さ0.01に相当する
	// ※ 元のコードでは 0.0001f / 0.00001f / 0.0f が混在していたので、
	//    最も多く使われていた 0.0001f に統一した(Unreal の KINDA_SMALL_NUMBER と同値)
	inline constexpr float kSmallNumber = 0.0001f;

	//------------------------------------------------
	// 正規化(ゼロ長ガード付き)
	//------------------------------------------------

	// 正規化して true を返す。長さがしきい値以下なら何もせず false を返す
	// ＝「短すぎたら処理を打ち切る」書き方(if (!TryNormalize(dir)) { return; })に使う
	// ※ Unreal の FVector::Normalize(Tolerance) と同じ意味。ただし Vector3::Normalize()
	//    (ガード無し・戻り値なし)と紛らわしくならないよう Try を付けている
	inline bool TryNormalize(Math::Vector3& _v, float _tolerance = kSmallNumber)
	{
		if (_v.LengthSquared() <= _tolerance) { return false; }

		_v.Normalize();
		return true;
	}

	// 正規化した値を返す。長さがしきい値以下なら _fallback を返す
	// ＝「短すぎたら代わりの向きを使う」書き方に使う
	// ※ Unreal の FVector::GetSafeNormal(Tolerance)。失敗時の戻り値を選べるようにしてある
	//    (Unreal はゼロベクトル固定。本作には Backward を代用する箇所があるため)
	inline Math::Vector3 GetSafeNormal(const Math::Vector3& _v,
		const Math::Vector3& _fallback = Math::Vector3::Zero,
		float _tolerance = kSmallNumber)
	{
		Math::Vector3 v = _v;
		if (!TryNormalize(v, _tolerance)) { return _fallback; }

		return v;
	}

	// 方向と長さを同時に取り出す。長さがしきい値以下なら false(このとき方向はゼロ・長さは0)
	// ＝長さを後でも使う場所用。Length() と Normalize() で二度計算しないで済む
	// ※ Unreal の FVector::ToDirectionAndLength
	inline bool ToDirectionAndLength(const Math::Vector3& _v, Math::Vector3& _outDir, float& _outLength,
		float _tolerance = kSmallNumber)
	{
		const float lenSq = _v.LengthSquared();
		if (lenSq <= _tolerance)
		{
			_outDir = Math::Vector3::Zero;
			_outLength = 0.0f;
			return false;
		}

		_outLength = std::sqrt(lenSq);
		_outDir = _v / _outLength;
		return true;
	}

	//------------------------------------------------
	// 水平(xz)成分
	//------------------------------------------------

	// y成分を捨てて水平(xz)成分だけにする(公式名は無いので独自)
	// ※ 「速度のyを0にして落下を止める」用途とは別物。あちらは物理的な操作なので置き換えない
	inline Math::Vector3 FlattenY(const Math::Vector3& _v)
	{
		return Math::Vector3(_v.x, 0.0f, _v.z);
	}

	// 水平(xz)成分だけを取り出して正規化する。長さがしきい値以下なら _fallback
	// ※ Unreal の FVector::GetSafeNormal2D にあたる。ただし Unreal は Z-up なので
	//    あちらの 2D は xy 平面。本作は Y-up で xz 平面なので名前を XZ にしている
	inline Math::Vector3 GetSafeNormalXZ(const Math::Vector3& _v,
		const Math::Vector3& _fallback = Math::Vector3::Zero,
		float _tolerance = kSmallNumber)
	{
		return GetSafeNormal(FlattenY(_v), _fallback, _tolerance);
	}

	//------------------------------------------------
	// 面に対する分解
	//------------------------------------------------

	// _vから「_planeNormal方向の成分」を取り除いて、面に沿った成分だけにする
	// (壁に沿って滑らせる／ワイヤーの接線成分だけ残す、等)
	// ※ Unity の Vector3.ProjectOnPlane。数学用語では「ベクトル棄却(rejection)」
	// ※ _planeNormal は単位ベクトルであること。長さが1でないと除去量がずれる
	inline Math::Vector3 ProjectOnPlane(const Math::Vector3& _v, const Math::Vector3& _planeNormal)
	{
		return _v - _planeNormal * _v.Dot(_planeNormal);
	}

	// _velocityのうち「面へ向かって入っていく成分」だけを取り除く。
	// 面から離れていく分はそのまま残すので、壁を擦りながら離れる動きが死なない
	// ＝ ProjectOnPlane との違いは「片側だけ」削ること
	// ※ Quake/Source系エンジンの PM_ClipVelocity と同じ考え方
	inline Math::Vector3 ClipVelocity(const Math::Vector3& _velocity, const Math::Vector3& _planeNormal)
	{
		const float into = _velocity.Dot(_planeNormal);
		if (into >= 0.0f) { return _velocity; }

		return _velocity - _planeNormal * into;
	}

	// ベクトルの長さに上限をかける(向きは変えない)
	// ※ Unity の Vector3.ClampMagnitude
	inline Math::Vector3 ClampMagnitude(const Math::Vector3& _v, float _maxLength)
	{
		const float len = _v.Length();
		if (len <= _maxLength) { return _v; }
		if (len <= 0.0f) { return _v; }

		return _v * (_maxLength / len);
	}

	//------------------------------------------------
	// 目標値への追従(フレームレート非依存)
	//------------------------------------------------

	// 現在値を目標へ「毎秒_speedの勢いで」近づける。dtを掛けてからクランプするので
	// フレームレートが変わっても追従の速さがほぼ変わらない。_speedが大きいほど速く追いつく
	// ※ Unreal の FMath::FInterpTo / VInterpTo。式まで同じ
	// ※ クランプが必須。dtが大きい(処理落ち)ときに1.0を超えると目標を通り越して振動する
	inline float InterpTo(float _current, float _target, float _deltaTime, float _speed)
	{
		const float t = std::clamp(_deltaTime * _speed, 0.0f, 1.0f);
		return _current + (_target - _current) * t;
	}

	inline Math::Vector2 InterpTo(const Math::Vector2& _current, const Math::Vector2& _target,
		float _deltaTime, float _speed)
	{
		const float t = std::clamp(_deltaTime * _speed, 0.0f, 1.0f);
		return Math::Vector2::Lerp(_current, _target, t);
	}

	inline Math::Vector3 InterpTo(const Math::Vector3& _current, const Math::Vector3& _target,
		float _deltaTime, float _speed)
	{
		const float t = std::clamp(_deltaTime * _speed, 0.0f, 1.0f);
		return Math::Vector3::Lerp(_current, _target, t);
	}

	//------------------------------------------------
	// 補間・回転
	//------------------------------------------------

	// 現在値を_targetの方向へ_rate(0~1)の割合で近づける(Lerpによる定数減衰接近)
	// ※ 割合を自分で用意する版。毎フレーム呼ぶなら dt を織り込む InterpTo のほうを使う
	//   (_rate を固定値にするとフレームレートで追従の速さが変わってしまうため)
	Math::Vector3 ApproachByLerp(const Math::Vector3& _current, const Math::Vector3& _target, float _rate);

	// float版
	float ApproachByLerp(float _current, float _target, float _rate);

	// 現在向いている角度(Y軸、度：0~360を想定)を、目標方向(ワールド座標系)へ
	// 最大_maxAngleSpeedDeg度/回まで回転させた新しい角度を返す(敵がプレイヤー方向を向く等に使用)
	// ・内積でなす角(cos)を求め、それをそのまま回転量として使う
	// ・外積のY成分の符号で、右回り/左回りどちらに回すべきかを判定する
	float RotateToDirection(float _nowAngleDeg, const Math::Vector3& _toDir, float _maxAngleSpeedDeg);
}
