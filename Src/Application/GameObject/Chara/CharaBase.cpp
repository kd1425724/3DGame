#include "CharaBase.h"

#include "../../main.h"
#include "../../Scene/SceneManager.h"
#include "../../Debug/DebugParams/DebugParams.h"
#include "../../Debug/DebugFlags/DebugFlags.h"
#include "../../Collision/CollisionGrid.h"   // 静的コリジョンのbroadphase(近傍の建物だけ問い合わせる)
#include "../../API/MathAPI/MathAPI.h"        // 安全な正規化・水平化などの共通計算
#include "../../Debug/DebugDraw/DebugDraw.h"  // デバッグ表示のカテゴリ判定

DebugDraw::Category CharaBase::GetDebugCategory() const
{
	return DebugDraw::Category::Player;
}

void CharaBase::SetAsset(const std::string& assetName)
{
	m_modelWork.SetModelData(KdAssets::Instance().m_modeldatas.GetData(assetName));
}

Math::Matrix CharaBase::GetDrawMatrix() const
{
	// 原点が足元のモデル(GogglesCharaは頂点のYが0〜1.899)は、そのまま描くと
	// 足が pos の高さに来て半身ぶん浮く。半身下げて体の中心を pos に合わせる。
	// 原点が中心のモデル(Block等)は下げない
	float half = m_modelOriginIsFeet ? GetBodyHalfHeight() : 0.0f;

	return Math::Matrix::CreateTranslation(0.0f, -half, 0.0f)
		* Math::Matrix::CreateRotationZ(DirectX::XMConvertToRadians(m_tilt.y))
		* Math::Matrix::CreateRotationX(DirectX::XMConvertToRadians(m_tilt.x))
		* m_mWorld;
}

void CharaBase::UpdateTilt(float _deltaTime)
{
	//目標の傾きを受け取る
	//派生クラスが今どれだけ傾くべくか返してくれる
	Math::Vector2 target = SelectTilt();

	//上限をかける
	//かけないと、アンカーが真横や真下にあるとキャラが逆さまになる
	float maxDeg = DebugParams::Instance().Float(U8("傾き/最大角度"), 45.0f, 0, 89.0f);
	target.x = std::clamp(target.x, -maxDeg, maxDeg);
	//yはzとして扱う
	target.y = std::clamp(target.y, -maxDeg, maxDeg);

	//現在値を目標に寄せる(ターゲットに少しずつ寄せる)
	float k = DebugParams::Instance().Float(U8("傾き/追従速度"), 8.0f, 1.0f, 60.0f);
	m_tilt = MathAPI::InterpTo(m_tilt, target, _deltaTime, k);
}

void CharaBase::UpdateArmAim(float _deltaTime)
{
	// DebugFlagsでオフにしたときも、いきなりアニメへ戻さず重みを0へ落として自然に抜ける。
	// (早期リターンにすると切り替えた瞬間に腕が飛び、比較用のオンオフとして使えない)
	//
	// 【2026/07/27 既定をfalseへ】ワイヤー中の姿勢を手付けクリップ("20 odm fly")で作ったので、
	// 手続き的に腕を上書きするとそのポーズが壊れる。機能自体は残してあるので、
	// DebugFlagsのチェックを入れれば従来どおりアンカーへ腕を向ける動作に戻せる
	bool enabled = DebugFlags::Instance().Get(U8("腕/アンカーへ向ける"), false);

	// 狙う点は派生クラスが決める。狙っている間だけ目標を更新し、
	// 離した後は「最後に狙っていた点」を向いたまま重みだけ0へ落としてアニメへ戻す
	Math::Vector3 aimTarget = {};
	bool wantAim = enabled && SelectArmAimTarget(aimTarget);
	if (wantAim)
	{
		m_armAimTarget = aimTarget;
	}

	// 重みを目標へ寄せる(UpdateTiltと同じ寄せ方)
	float k = DebugParams::Instance().Float(U8("腕/追従速度"), 8.0f, 1.0f, 60.0f);
	m_armAimWeight = MathAPI::InterpTo(m_armAimWeight, wantAim ? 1.0f : 0.0f, _deltaTime, k);

	// ほぼ0なら何もしない=アニメそのまま。
	// CalcNodeMatricesも走らないので、狙っていないキャラ・時間帯には負荷がかからない
	if (m_armAimWeight < 0.001f) { return; }

	// 回転量の上限。無いと、アンカーが真後ろや真下にあるとき肩が反対側へ折れて破綻する
	float maxRad = DirectX::XMConvertToRadians(
		DebugParams::Instance().Float(U8("腕/最大角度"), 120.0f, 0.0f, 180.0f));

	// 狙いの強さ。1.0で完全に狙い、下げるほどアニメのポーズを残す(見た目の好み用)
	float strength = DebugParams::Instance().Float(U8("腕/向ける強さ"), 1.0f, 0.0f, 1.0f);

	// この骨の軸はローカル+Y(glTFで子=肘への平行移動が(0, 0.1907, 0)だったので実測済み)
	const Math::Vector3 boneAxis = { 0.0f, 1.0f, 0.0f };

	m_modelWork.CalcNodeMatrices();

	//型 : KdModelWork::Node
	for (auto& node : m_modelWork.WorkNodes())
	{
		//左腕じゃなかったら
		if (node.m_name != "mixamorig:LeftArm") { continue; }

		// 骨の現在位置から狙う点への向き(ワールド)。骨が動けば毎フレーム変わる
		Math::Vector3 dirWorld = MathAPI::GetSafeNormal(
			m_armAimTarget - GetBoneWorldMatrix(node).Translation());

		AimBoneToDir(node, dirWorld, boneAxis, m_armAimWeight * strength, maxRad);

		break;
	}

	m_modelWork.CalcNodeMatrices();
}

void CharaBase::UpdateLegFlow(float _deltaTime)
{
	// DebugFlagsでオフにしたときも、いきなりアニメへ戻さず重みを0へ落として自然に抜ける
	// (UpdateArmAimと同じ抜け方。切り替えた瞬間に脚が飛ぶと比較用のオンオフとして使えない)
	//
	// 【2026/07/27 既定をfalseへ】手付けクリップ("20 odm fly")が脚の形を決めるので、
	// 手続き的な上書きは切る。左右の腿で捻りが別々に決まって脚が交差する不具合も
	// 未解決のままなので、その回避も兼ねている(原因＝最短回転が軸まわりの捻りを指定しないこと)。
	// 機能は残してあるのでDebugFlagsで戻せる
	bool enabled = DebugFlags::Instance().Get(U8("脚/慣性でなびかせる"), false);
	bool wantFlow = enabled && SelectLegFlow();

	float follow = DebugParams::Instance().Float(U8("脚/追従速度"), 12.0f, 1.0f, 60.0f);
	m_legFlowWeight = MathAPI::InterpTo(m_legFlowWeight, wantFlow ? 1.0f : 0.0f, _deltaTime, follow);

	// --- 脚の向きを進める ---
	// ※ 重みが0(接地中)でも進め続ける。止めると次に浮いた瞬間、前回浮いていた時の
	//    古い向きから動き出すことになり、脚が一度あらぬ方向へ振られる
	float strength = DebugParams::Instance().Float(U8("脚/なびく強さ"),   0.8f,  0.0f,   2.0f);
	float speedRef = DebugParams::Instance().Float(U8("脚/速度の基準"),  20.0f,  1.0f,  60.0f);
	float stiff    = DebugParams::Instance().Float(U8("脚/バネの硬さ"),  60.0f,  1.0f, 200.0f);
	float damp     = DebugParams::Instance().Float(U8("脚/減衰"),        10.0f,  0.0f,  50.0f);

	// 目標＝真下から「進行方向の逆」へ、速さに応じて倒した向き。
	// 真下に落ちるだけなら真下のまま＝脚は垂れる。速く振られるほど後ろへ流れる
	const Math::Vector3 down = { 0.0f, -1.0f, 0.0f };
	Math::Vector3 velDir = MathAPI::GetSafeNormal(m_velocity);
	float speedRatio = std::clamp(m_velocity.Length() / speedRef, 0.0f, 1.0f);
	Math::Vector3 target = MathAPI::GetSafeNormal(down - velDir * (speedRatio * strength), down);

	// バネ＋減衰の2階積分。
	// ※ Lerpでは慣性に見えない。Lerpは目標に着いたら止まるだけで、
	//   「行きすぎて戻る(振り戻し)」が出ないため。速度を持たせて初めて慣性になる
	// 明示的オイラー法なのでdtが大きいと発散する。処理落ちに備えて上限をかける
	float dt = std::min(_deltaTime, 1.0f / 30.0f);
	Math::Vector3 accel = (target - m_legFlowDir) * stiff - m_legFlowVel * damp;
	m_legFlowVel += accel * dt;
	m_legFlowDir += m_legFlowVel * dt;
	MathAPI::TryNormalize(m_legFlowDir);

	// ほぼ0なら何もしない=アニメそのまま。
	// CalcNodeMatricesも走らないので、地上や脚を触らないキャラには負荷がかからない
	if (m_legFlowWeight < 0.001f) { return; }

	// 回転量の上限。無いと脚が真上を向くような破綻したポーズになる
	float maxRad = DirectX::XMConvertToRadians(
		DebugParams::Instance().Float(U8("脚/最大角度"), 70.0f, 0.0f, 120.0f));

	// 腿の骨の軸はローカル+Y(glTFで子=膝への平行移動が左(0,0.4679,0)/右(0,0.4685,0)＝実測済み)。
	// ※ 左右を別々に測った結果どちらも+Y。左を反転して決めつけないこと
	const Math::Vector3 boneAxis = { 0.0f, 1.0f, 0.0f };

	// 直前のUpdateArmAimが末尾で計算済みなら、ここは飛ばす。
	// CalcNodeMatricesは無条件に全ノードを再計算する(ダーティフラグは最後にクリアするだけ)ので、
	// 素直に呼ぶと1フレームに同じ計算が1回ぶん余る。腕が動いていない(重み0で早期リターン)
	// フレームではフラグが立ったままなので、その時はちゃんと計算される
	if (m_modelWork.NeedCalcNodeMatrices())
	{
		m_modelWork.CalcNodeMatrices();
	}

	int done = 0;
	for (auto& node : m_modelWork.WorkNodes())
	{
		if (node.m_name != "mixamorig:LeftUpLeg" && node.m_name != "mixamorig:RightUpLeg") { continue; }

		// 両腿を同じ向きへ向ける(＝脚が揃う)。開きが欲しければ段階3-bの膝と一緒にやる
		AimBoneToDir(node, m_legFlowDir, boneAxis, m_legFlowWeight, maxRad);

		++done;
		if (done >= 2) { break; }
	}

	m_modelWork.CalcNodeMatrices();
}

bool CharaBase::AimBoneToDir(KdModelWork::Node& _node, const Math::Vector3& _dirWorld,
	const Math::Vector3& _boneAxis, float _weight, float _maxRad)
{
	// 狙う方向を骨のローカル空間へ移す(骨の軸と比べるため)。
	// ワールド行列はGetBoneWorldMatrix経由＝GetDrawMatrixの掛け忘れをここで防ぐ
	Math::Vector3 dirLocal = Math::Vector3::TransformNormal(
		_dirWorld, GetBoneWorldMatrix(_node).Invert());
	if (!MathAPI::TryNormalize(dirLocal)) { return false; }

	// 骨の軸をその方向へ向ける回転(外積ゼロ・acosのNaNガードはMathAPI側にある)
	Math::Matrix rot;
	if (!MathAPI::FromToRotation(_boneAxis, dirLocal, rot, _maxRad, _weight)) { return false; }

	// 【罠】前から掛けて初めて「骨の原点まわりで子を回す」＝関節を曲げる意味になる。
	// 逆に掛けると骨が体から外れて飛ぶ(行ベクトル規約なので左が先)
	_node.m_localTransform = rot * _node.m_localTransform;
	return true;
}

bool CharaBase::GetBoneWorldPos(std::string_view _name, Math::Vector3& _outPos) const
{
	const KdModelWork::Node* node = m_modelWork.FindNode(_name);
	if (!node) { return false; }

	_outPos = GetBoneWorldMatrix(*node).Translation();
	return true;
}

void CharaBase::DrawLit()
{
	if (!m_modelWork.IsEnable()) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, GetDrawMatrix(), m_color);
}

void CharaBase::GenerateDepthMapFromLight()
{
	if (!m_modelWork.IsEnable()) { return; }

	// 深度パス用シェーダはBaseScene側のBeginGenerateDepthMapFromLightで既にセット済み。
	// ここは通常のモデル描画を呼ぶだけで、光から見た深度が深度マップに書き込まれる
	// ※ 描画と同じ行列を使う。DrawLitだけ下げると影が体からずれる
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, GetDrawMatrix());
}

void CharaBase::UpdateFacing(float _deltaTime)
{
	Math::Vector3 dir = SelectFacingDir();

	// 止まっている(ほぼ動いていない)ときは今の向きを保つ。
	// ここで0ベクトルからatan2を取ると角度が0°に飛び、停止するたび正面へ向き直ってしまう
	dir = MathAPI::FlattenY(dir);
	if (!MathAPI::TryNormalize(dir)) { return; }

	// m_rot は度で持っているので、向きベクトルを度の角度へ直す
	float targetDeg = MathAPI::DirToYawDeg(dir, m_modelForwardIsMinusZ);

	Math::Vector3 rot = GetRot();

	// 1フレームで回れる上限まで詰める(瞬間で向きが変わるとカクつくのでなめらかに)。
	// MoveTowardsAngleDeg が差を -180〜180 に畳むので、350°→10° で遠回りしない。
	// そのあと 0〜360 に収めて角度が際限なく増減するのを防ぐ
	float maxStep = SelectTurnSpeed() * _deltaTime;
	rot.y = MathAPI::ClampAngleDeg(MathAPI::MoveTowardsAngleDeg(rot.y, targetDeg, maxStep));

	SetRot(rot);
}

void CharaBase::UpdateAnimation()
{
	if (!m_modelWork.IsEnable()) { return; }

	// 再生すべきアニメ名を派生クラスから受け取る。空ならアニメを持たないキャラ(Enemyなど)
	std::string next = SelectAnimation();
	if (next.empty()) { return; }

	// 名前が変わったときだけ切り替える
	if (next != m_currentAnimName)
	{
		std::shared_ptr<KdAnimationData> spAnim = m_modelWork.GetAnimation(next);

		// 名前が見つからないときは切り替えず、前のアニメを流し続ける。
		// m_currentAnimNameも更新しないので、名前が直れば次のフレームで復帰できる
		if (spAnim)
		{
			m_animator.SetAnimation(spAnim, true);
			m_currentAnimName = next;
		}
	}

	// KdGLTFLoaderがキー時刻を「秒×60」= 60fps基準のフレーム値に変換して読み込んでいる
	// (KdGLTFLoader.cpp:786 「元が60fpsとして変換」)ので、AdvanceTimeへ渡す値は
	// 「1フレームあたり1.0」が等速になる。実時間で進めるため deltaTime*60 を渡す
	// (フレームレートが変動しても再生速度が変わらない)。
	// GetDeltaTimeはtimeScale適用済みなので、集中スロー中はアニメも一緒に遅くなる
	// ※ SelectAnimationSpeed()は切り替え後に呼ぶ(派生クラスが現在のアニメ名を見て倍率を決めるため)
	float animSpeed = Application::Instance().GetDeltaTime() * 60.0f * SelectAnimationSpeed();

	m_animator.AdvanceTime(m_modelWork.WorkNodes(), animSpeed);
}

// ※ IsWallBetween は 2026/07/19 に CollisionGrid へ移動(照準の遮蔽判定からも使うため)

void CharaBase::GroundCheck()
{
	ZoneScoped;	// Tracy計測(2026/07/19)：接地/天井/壁の解決をまとめて呼ぶ入口

	float deltaTime = Application::Instance().GetDeltaTime();

	// 重力を垂直速度に加える(重力はDebugParamsで調整可能)
	// m_gravityScale は壁走り中に0にされる(壁に張り付いている間は落ちない)。
	// 重力を止める側(WallAction)は「意思決定」のUpdateで倍率を書き、ここは解決するだけにしてある
	float gravity = DebugParams::Instance().Float(U8("キャラ/重力"), 20.0f, 0.0f, 100.0f);
	m_velocity.y -= gravity * m_gravityScale * deltaTime;

	// 速度で位置を進める(縦横まとめて)。水平速度は各キャラのUpdateが設定する
	// ※ Enemyは水平をUpdateで直接動かすためm_velocity.xzは0のまま＝縦だけ動く
	Math::Vector3 pos = GetPos();
	Math::Vector3 startPos = pos;   // 移動前の位置(スイープの始点)
	pos += m_velocity * deltaTime;

	// 地面に潜っていたら押し上げて落下を止める(接地状態もここで更新される)
	ResolveGround(pos);

	// 上昇中に頭上の天井へ潜り込むのを止める(接地とは上昇/落下で排他)
	ResolveCeiling(startPos, pos);

	// 高速移動で壁を飛び越える(トンネリング)のを先に止める
	ResolveBumpSweep(startPos, pos);

	// 壁(TypeBump=Block等)にめり込んでいたら水平に押し出す
	ResolveBump(pos);

	SetPos(pos);
}

void CharaBase::ResolveGround(Math::Vector3& pos, bool _allowLanding)
{
	ZoneScoped;	// Tracy計測(2026/07/19)：真下レイによる接地解決

	float deltaTime = Application::Instance().GetDeltaTime();

	// この呼び出しの前に接地していたか(着地の"瞬間"だけ手応えを出すためのエッジ検出用)
	bool wasGrounded = m_isGrounded;

	// あたる側の設定＝＝＝＝＝＝＝＝＝＝
	// レイの始点は「足元の少し上」。下向きに撃って接地面を拾う。落下が速いほど始点を上げ(fallThisFrame)、
	// 高速落下でも地面を飛び越さない(=可変レイ判定)。
	// ※ 以前は「体の中心から1.0上」=頭より上から出しており、デバッグ表示で邪魔＆過剰だったため足元基準に変更。
	//    足元より上を見るぶん(rayStepUp)が段差の自動乗り上げ許容高さになる(頭上までは伸ばさない)
	float feetY = pos.y - GetBodyHalfHeight();
	float fallThisFrame = (m_velocity.y < 0.0f) ? (-m_velocity.y * deltaTime) : 0.0f;
	const float rayStepUp = 0.3f;   // 足元より上をどれだけ見るか(=乗り上げできる段差の高さ・接地の許容)

	// 足元より下をどれだけ見るか。下方向の吸着距離ぶんは必ず見ないと、
	// 吸着しようにも床がレイに入らない(＝伸ばすのは吸着の前提条件)
	float snapDown = DebugParams::Instance().Float(U8("キャラ/接地の吸着距離"), 0.45f, 0.0f, 1.5f);
	const float rayBelowMin = 0.2f;
	float rayBelow = (snapDown > rayBelowMin) ? snapDown : rayBelowMin;
	Math::Vector3 rayPos(pos.x, feetY + rayStepUp + fallThisFrame, pos.z);
	float rayRange = rayStepUp + fallThisFrame + rayBelow;

	// 地面(TypeGround)に加えてBlock等(TypeBump)の天面にも乗れるようにする
	KdCollider::RayInfo ray(KdCollider::TypeGround | KdCollider::TypeBump, rayPos, Math::Vector3::Down, rayRange);

	// デバッグ表示：地面判定に使用したレイを可視化
	if (KdGameObject::s_showColliderDebug && DebugDraw::IsOn(GetDebugCategory()))
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugLine(ray.m_pos, ray.m_dir, ray.m_range, Math::Color(1.0f, 1.0f, 0.0f, 1.0f));
	}

	// レイに当たったオブジェクトを格納するリストを作成
	std::list<KdCollider::CollisionResult> retRayList;

	// 近傍の静的コリジョンだけをグリッドから取り出して判定する(大量配置時のCPU削減)
	std::vector<KdGameObject*> candidates;
	CollisionGrid::Instance().QueryRay(rayPos, Math::Vector3::Down, rayRange, candidates);
	for (KdGameObject* obj : candidates)
	{
		obj->Intersects(ray, &retRayList);
	}

	// レイに当たったリストから一番遮った(overlapが最大の)地面を探す
	float maxOverLap = 0;
	Math::Vector3 hitPos;
	bool hit = false;

	for (auto& ret : retRayList)
	{
		if (maxOverLap < ret.m_overlapDistance)
		{
			maxOverLap = ret.m_overlapDistance;
			hitPos = ret.m_hitPos;
			hit = true;
		}
	}

	// 落下中(velocity.y<=0)に地面へ届いていて、立つべき高さより下に来ていたら着地させる
	// ※ 上昇中(ジャンプ直後)は吸着しないので、そのまま上へ飛べる
	if (hit && m_velocity.y <= 0.0f)
	{
		// 体の半分の高さだけ持ち上げて、足が地面に接する位置に立たせる
		float standY = hitPos.y + GetBodyHalfHeight();

		// 【下方向の吸着】(2026/07/28 追加)
		// 以前は pos.y <= standY ＝「地面にめり込んでいるとき」しか接地にしていなかった。
		// そのため屋根の傾斜や段差で床がわずかに下がるだけで空中扱いになり、
		// 10cmの段差でも着地まで約9フレーム(√(2*0.1/9.8)≒0.14秒)かかって、
		// そのたびに着地モーションが再生されていた。
		// → 直前に接地していたなら、吸着距離以内の床までは降ろして接地を維持する。
		// ※ wasGrounded を条件に入れるのが肝。入れるとジャンプ直後や落下中に
		//    床へ吸い寄せられることがなくなる(屋根の端から飛び降りる動作は壊れない)
		bool canSnapDown = wasGrounded && (pos.y - standY) <= snapDown;

		if (pos.y <= standY || canSnapDown)
		{
			// 「着地しない」モード(ワイヤーで地面スレスレを飛ぶ用)。
			// 地面へのめり込みだけ直し、落下は止めるが、接地扱いにはしない。
			// 着地の手応えも記録しない(地面に触れるたびカメラが揺れるのを防ぐ)。
			// 横の勢いはそのままなので、地面を舐めるように飛び続けられる
			if (!_allowLanding)
			{
				pos.y = standY;
				if (m_velocity.y < 0.0f)
				{
					m_velocity.y = 0.0f;
				}
				m_isGrounded = false;
				m_coyoteTimer = 0.0f;   // 意図的に着地させないモードなので猶予も与えない
				return;
			}

			// 空中から着地した"瞬間"だけ、落下の速さを手応え(カメラ揺れ)用に記録する
			if (!wasGrounded)
			{
				m_landingImpact = -m_velocity.y;
			}

			pos.y = standY;
			m_velocity.y = 0.0f;   // 縦の勢いだけ止める(横の勢いはそのまま=着地滑りは各キャラのUpdate側で制御)
			m_isGrounded = true;
			m_coyoteTimer = DebugParams::Instance().Float(U8("キャラ/コヨーテタイム"), 0.1f, 0.0f, 0.5f);
			return;
		}
	}

	// それ以外は空中。猶予(コヨーテタイム)を減らしていく
	m_isGrounded = false;
	m_coyoteTimer -= deltaTime;
	if (m_coyoteTimer < 0.0f)
	{
		m_coyoteTimer = 0.0f;
	}
}

void CharaBase::ResolveCeiling(const Math::Vector3& fromPos, Math::Vector3& pos)
{
	ZoneScoped;	// Tracy計測(2026/07/19)：上昇中の天井判定

	// 上昇中だけ天井を見る。落下・水平移動時はResolveGround/ResolveBumpが担当
	if (m_velocity.y <= 0.0f) { return; }

	// このフレームで頭がどれだけ上がったか(=掃引区間の長さ)
	float rise = pos.y - fromPos.y;
	if (rise <= 0.0f) { return; }

	float halfH = GetBodyHalfHeight();

	// レイの始点は「移動前の頭の高さ」。ここは前フレームに天井へ潜っていない安全な位置なので、
	// 下から上へ飛ばしても立っている床の天面を裏から拾わない(体中心より下から飛ばすと誤検知する)。
	// x,z は現在位置(頭が最終的に来る場所)を使う。射程=上昇量+余裕で高速上昇のトンネリングも拾う
	Math::Vector3 rayStart(pos.x, fromPos.y + halfH, pos.z);
	float rayRange = rise + 0.1f;

	KdCollider::RayInfo ray(KdCollider::TypeBump, rayStart, Math::Vector3::Up, rayRange);

	// デバッグ表示：天井判定に使用したレイを可視化(下向きの地面レイと色を変える=マゼンタ)
	if (KdGameObject::s_showColliderDebug && DebugDraw::IsOn(GetDebugCategory()))
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugLine(ray.m_pos, ray.m_dir, ray.m_range, Math::Color(1.0f, 0.0f, 1.0f, 1.0f));
	}

	std::list<KdCollider::CollisionResult> retRayList;

	// 近傍の静的コリジョンだけをグリッドから取り出して判定する(大量配置時のCPU削減)
	std::vector<KdGameObject*> candidates;
	CollisionGrid::Instance().QueryRay(rayStart, Math::Vector3::Up, rayRange, candidates);
	for (KdGameObject* obj : candidates)
	{
		obj->Intersects(ray, &retRayList);
	}

	// 一番手前(下)の天井=overlapが最大のヒットを採用(ResolveGroundと同じ選び方)
	float maxOverLap = 0;
	Math::Vector3 hitPos;
	bool hit = false;

	for (auto& ret : retRayList)
	{
		if (maxOverLap < ret.m_overlapDistance)
		{
			maxOverLap = ret.m_overlapDistance;
			hitPos = ret.m_hitPos;
			hit = true;
		}
	}

	// 天井に届いていたら、頭の天面がその直下に収まる高さへ下げて上向きの勢いを止める
	if (hit)
	{
		float ceilCenterY = hitPos.y - halfH - 0.02f;   // 体中心の上限(頭が天井の少し下に来る)
		if (pos.y > ceilCenterY)
		{
			pos.y = ceilCenterY;
			m_velocity.y = 0.0f;   // 上向きの勢いだけ止める(次フレームから重力で落下)
		}
	}
}

void CharaBase::ResolveBump(Math::Vector3& pos)
{
	ZoneScoped;	// Tracy計測(2026/07/19)：球の押し出しによる壁解決

	// 体を球で近似し、Block等(TypeBump)にめり込んでいたら水平方向へ押し出して壁にする
	// ※ 縦の着地はResolveGroundが担当するので、ここは水平方向だけ押す
	float radius = DebugParams::Instance().Float(U8("キャラ/壁当たり半径"), 0.4f, 0.1f, 2.0f);

	// 球の中心の高さ。球の底が足元のほんの少し上に来るように持ち上げる。
	// こうしないと塔の天面に立ったとき球が天面に潜り込み、水平へ押し出されて引っかかる。
	// (縦の乗り上げ・着地はResolveGroundが担当。ここは壁=体の高さだけを見る)
	float centerY = pos.y - GetBodyHalfHeight() + radius + 0.02f;

	// デバッグ表示：壁当たり用の球を可視化(DebugFlags「当たり判定/AABB表示」でON/OFF)
	if (KdGameObject::s_showColliderDebug && DebugDraw::IsOn(GetDebugCategory()))
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugSphere(Math::Vector3(pos.x, centerY, pos.z), radius, Math::Color(0.3f, 0.6f, 1.0f, 1.0f));
	}

	// 壁の接触状態は毎回ここで取り直す(この解決の時点で壁に触れているか)。
	// 壁走り(WallAction)はこの結果を見るだけで、自前の当たり判定を撃たない
	m_isTouchingWall = false;

	// 複数の壁に挟まれても安定するよう数回反復する
	for (int iter = 0; iter < 3; ++iter)
	{
		const Math::Vector3 sphereCenter(pos.x, centerY, pos.z);
		KdCollider::SphereInfo sphere(KdCollider::TypeBump, sphereCenter, radius);

		std::list<KdCollider::CollisionResult> results;
		// 近傍の静的コリジョンだけをグリッドから取り出して判定する(大量配置時のCPU削減)
		std::vector<KdGameObject*> candidates;
		CollisionGrid::Instance().QuerySphere(sphereCenter, radius, candidates);
		for (KdGameObject* obj : candidates)
		{
			obj->Intersects(sphere, &results);
		}

		// 一番深くめり込んでいる相手から押し出す。
		// ただし押し出し方向(≒面の法線)が上向きの接触は「床/歩ける斜面」なので水平押し出しの対象外にする。
		// (StagePropの凸包などの斜め面を壁扱いして水平に押すと、斜面を下へ滑ってしまうため。
		//  斜面での上下位置合わせは縦の接地ResolveGroundが担当する)。
		// しきい値: 法線Yがこれを超えたら床扱い。0=全部壁, 1=真上の面だけ床。0.5≒水平から約60°より緩い面。
		float walkableNormalY = DebugParams::Instance().Float(U8("キャラ/歩ける斜面のしきい値(法線Y)"), 0.5f, 0.0f, 1.0f);

		Math::Vector3 push = Math::Vector3::Zero;
		float maxOverlap = 0.0f;
		for (auto& ret : results)
		{
			// 床/歩ける斜面(法線が上向き)は縦の接地ResolveGroundに任せ、ここでは水平に押さない(斜面滑り防止)
			if (ret.m_hitDir.y > walkableNormalY) { continue; }

			if (ret.m_overlapDistance > maxOverlap)
			{
				maxOverlap = ret.m_overlapDistance;
				push = ret.m_hitDir * ret.m_overlapDistance;
			}
		}

		// どこにもめり込んでいなければ終わり
		if (maxOverlap <= 0.0f) { break; }

		// 水平方向だけ押し出す(縦の乗り上げ・着地はResolveGroundに任せる)
		push = MathAPI::FlattenY(push);
		pos += push;

		// 壁へ向かう速度成分を消して、壁に沿って滑るようにする
		Math::Vector3 n = push;
		if (MathAPI::TryNormalize(n))
		{
			// 壁走り用に「今どの壁に触れているか」を記録しておく。
			// pushは既に水平化(push.y=0)されているので、これがそのまま水平な壁の法線になる。
			// 最初の反復＝一番深くめり込んでいる壁を採用する(挟まれた時に主となる壁)
			if (!m_isTouchingWall)
			{
				m_isTouchingWall = true;
				m_wallNormal = n;
			}

			// 壁へ入っていく成分だけ消す(離れる方向の勢いは残すので壁沿いの滑りは死なない)
			m_velocity = MathAPI::ClipVelocity(m_velocity, n);
		}
	}
}

void CharaBase::ResolveBumpSweep(const Math::Vector3& fromPos, Math::Vector3& pos)
{
	// Tracy計測(2026/07/19)：高速移動時の掃引判定。経路上に球を刻んで当てるので
	// 速度が上がるほど判定回数が増える＝ここが伸びたら刻み幅を疑う
	ZoneScoped;

	float radius = DebugParams::Instance().Float(U8("キャラ/壁当たり半径"), 0.4f, 0.1f, 2.0f);

	// 水平移動量だけを見る(縦の乗り降り・着地はResolveGroundが担当)
	Math::Vector3 delta = MathAPI::FlattenY(pos - fromPos);
	float dist = delta.Length();

	// 半径以下の移動ならトンネリングは起きない(体の球が必ず壁と重なる)。
	// その場合は静止時のResolveBump(重なり押し出し)に任せて、ここは何もしない。
	if (dist <= radius) { return; }

	Math::Vector3 dir = delta / dist;

	// 【トンネリング対策】以前は中心1本のレイだった。高速移動で壁の角をかすめると中心が角を
	// 通り抜けてすり抜けていた。経路を「壁当たり半径以下」のステップに刻み、各点で"体の球"を当て、
	// 進行方向が壁へ向かっている壁と最初に重なったステップの1つ手前で止める(球の幅で角も拾う/
	// 壁沿いの滑りは止めない)。最終的な密着押し出しは後段のResolveBumpに任せる。縦(y)は触らない=壁専用。
	float walkableNormalY = DebugParams::Instance().Float(U8("キャラ/歩ける斜面のしきい値(法線Y)"), 0.5f, 0.0f, 1.0f);
	float centerYOff = -GetBodyHalfHeight() + radius + 0.02f;   // 球中心の高さ=ResolveBumpと同じ
	float step = radius * 0.8f;
	int steps = (int)(dist / step) + 1;   // 切り上げ(最後のステップはt=distにクランプ)

	// デバッグ表示：スイープ経路を可視化
	if (KdGameObject::s_showColliderDebug && DebugDraw::IsOn(GetDebugCategory()))
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugLine(Math::Vector3(fromPos.x, pos.y + centerYOff, fromPos.z), dir, dist, Math::Color(1.0f, 0.4f, 0.0f, 1.0f));
	}

	for (int i = 1; i <= steps; ++i)
	{
		float t = step * (float)i;
		if (t > dist) { t = dist; }
		float frac = t / dist;
		// サンプル位置(経路上)。yはfromPos→posを補間して斜め移動にも沿わせる
		float sy = fromPos.y + (pos.y - fromPos.y) * frac;
		Math::Vector3 sphereCenter(fromPos.x + dir.x * t, sy + centerYOff, fromPos.z + dir.z * t);

		KdCollider::SphereInfo sphere(KdCollider::TypeBump, sphereCenter, radius);
		std::list<KdCollider::CollisionResult> results;
		std::vector<KdGameObject*> candidates;
		CollisionGrid::Instance().QuerySphere(sphereCenter, radius, candidates);
		for (KdGameObject* obj : candidates)
		{
			obj->Intersects(sphere, &results);
		}

		// 進行方向が壁へ向かっている(=すり抜ける恐れがある)壁を探す。床/歩ける斜面・壁沿いは無視。
		Math::Vector3 wallNormal = Math::Vector3::Zero;   // 壁から体へ向かう押し出し方向(単位)
		bool wallHit = false;
		for (auto& ret : results)
		{
			if (ret.m_hitDir.y > walkableNormalY) { continue; }   // 床/歩ける斜面は縦の接地ResolveGroundに任せる
			if (ret.m_overlapDistance <= 0.0f) { continue; }
			float toward = -(ret.m_hitDir.x * dir.x + ret.m_hitDir.z * dir.z);   // 進行が壁へ向かう成分
			if (toward > 0.1f)
			{
				wallNormal = ret.m_hitDir;
				wallHit = true;
				break;
			}
		}
		if (wallHit)
		{
			// 1つ手前(安全)の水平位置まで戻す。step<radius なので隙間はできず、密着はResolveBumpが押し出す
			float safeT = step * (float)(i - 1);
			pos.x = fromPos.x + dir.x * safeT;
			pos.z = fromPos.z + dir.z * safeT;

			// 壁法線方向へ向かう速度成分だけ消して壁に沿って滑れるようにする(進行方向まるごとは消さない)
			float intoWall = -(m_velocity.x * wallNormal.x + m_velocity.z * wallNormal.z);
			if (intoWall > 0.0f)
			{
				m_velocity.x += wallNormal.x * intoWall;
				m_velocity.z += wallNormal.z * intoWall;

				// 手応え(カメラ揺れ)は壁に当たった瞬間だけ記録(押し付け続けでは毎フレーム揺らさない)
				if (!m_wasHittingWall)
				{
					m_wallImpact = intoWall;
				}
				m_wasHittingWall = true;
			}
			return;
		}
	}

	// 経路上に(向かってくる)壁が無ければ接触フラグを解除
	m_wasHittingWall = false;
}

void CharaBase::Jump()
{
	// 接地しているとき、または地面を離れた直後の猶予中だけジャンプできる。
	// 崖際で入力が数フレーム遅れても跳べるようにするため(2026/07/28)
	if (!IsGroundedOrCoyote()) { return; }
	DoJump();
}

void CharaBase::DoJump()
{
	// ジャンプ初速はDebugParamsで調整可能(垂直速度に初速を与える)。接地判定はしない
	m_velocity.y = DebugParams::Instance().Float(U8("キャラ/ジャンプ力"), 8.0f, 0.0f, 30.0f);
	m_isGrounded = false;

	// 猶予を使い切る。消さないと、コヨーテタイムの間に空中で2回目が跳べてしまう
	m_coyoteTimer = 0.0f;
}
