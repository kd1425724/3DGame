#include "Enemy.h"

#include "../../../main.h"
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugDraw/DebugDraw.h"
#include "../../../Debug/DebugFlags/DebugFlags.h"
#include "../Player/Player.h"   // 突進命中時にPlayerの無敵判定/反撃通知/ノックバックを呼ぶため
#include "../../Debris/DebrisSystem.h"   // 関節が壊れたとき、その部位を落とすため

// 関節表。狙えるのはこの5つだけ。
// 骨の対応はMixamoリグの作りに沿っている(この経路のモデルなら他でもそのまま使える)：
//   肘 = ForeArm の根元／膝 = Leg の根元／首 = Neck。いずれも「その関節から先」を配下に持つ
// 半径はglTFの骨座標の実測から決めた初期値【モデル座標系】(身長1.899m)。
// 実機で見て詰められるよう、半径だけDebugParamsに出してある(左右は同じキーを共有)
// partNode は StoneGolem.gltf の【メッシュノード名】。骨名ではないことに注意
const Enemy::JointDef Enemy::kJointDefs[Enemy::kJointCount] =
{
	// 首は弱点。壊すと頭が落ちるので、小さい判定＋大きい倍率という業界標準の作り方にしてある
	{ U8("首"),   "mixamorig:Neck",         U8("関節/半径_首"), 0.15f, 2.0f, 20.0f,
	  "Asset/Models/Character/StoneGolem/Gib_Head.gltf",         "Part_Head"         },

	{ U8("左肘"), "mixamorig:LeftForeArm",  U8("関節/半径_肘"), 0.13f, 1.0f, 30.0f,
	  "Asset/Models/Character/StoneGolem/Gib_LeftForeArm.gltf",  "Part_LeftForeArm"  },

	{ U8("右肘"), "mixamorig:RightForeArm", U8("関節/半径_肘"), 0.13f, 1.0f, 30.0f,
	  "Asset/Models/Character/StoneGolem/Gib_RightForeArm.gltf", "Part_RightForeArm" },

	{ U8("左膝"), "mixamorig:LeftLeg",      U8("関節/半径_膝"), 0.14f, 1.0f, 40.0f,
	  "Asset/Models/Character/StoneGolem/Gib_LeftLeg.gltf",      "Part_LeftLeg"      },

	{ U8("右膝"), "mixamorig:RightLeg",     U8("関節/半径_膝"), 0.14f, 1.0f, 40.0f,
	  "Asset/Models/Character/StoneGolem/Gib_RightLeg.gltf",     "Part_RightLeg"     },
};

DebugDraw::Category Enemy::GetDebugCategory() const
{
	return DebugDraw::Category::Enemy;
}

const char* Enemy::GetJointName(int _index)
{
	if (_index < 0 || _index >= kJointCount) { return ""; }

	return kJointDefs[_index].name;
}

bool Enemy::IsJointAlive(int _index) const
{
	if (_index < 0 || _index >= kJointCount) { return false; }

	return m_jointHp[_index] > 0.0f;
}

bool Enemy::GetJointSphereAt(int _index, Math::Vector3& _outCenter, float& _outRadius) const
{
	// 壊れた関節は骨を潰してあり、球だけ元の位置に残しても狙う先として意味が無い
	if (!IsJointAlive(_index)) { return false; }

	return GetJointSphere(kJointDefs[_index], _outCenter, _outRadius);
}

void Enemy::ApplyJointDamage(int _index, float _damage)
{
	if (_index < 0 || _index >= kJointCount) { return; }

	const JointDef& joint = kJointDefs[_index];

	// 関節へは倍率を掛けたぶん、本体へは素のダメージが入る。
	// 弱点(首)を狙うと関節は早く壊れるが、本体HPの減りは他と同じ＝
	// 「部位を狙う」と「早く倒す」が別の目的になる(モンハン方式の狙い)
	if (m_jointHp[_index] > 0.0f)
	{
		m_jointHp[_index] -= _damage * joint.damageScale;

		if (m_jointHp[_index] <= 0.0f)
		{
			m_jointHp[_index] = 0.0f;
		}
	}

	ApplyBodyDamage(_damage);
}

void Enemy::UpdateBrokenJoints()
{
	for (int i = 0; i < kJointCount; ++i)
	{
		if (m_jointHp[i] > 0.0f) { continue; }

		// 壊れた瞬間の1回だけでよい。ノードの可視はアニメで書き戻されないため、
		// ボーン潰し方式のように毎フレームやり直す必要が無い
		if (m_gibSpawned[i]) { continue; }

		// 【順番が大事】消す【前】に部位を落とす。骨の姿勢から破片の初期姿勢を取るので、
		//   先に消しても問題は無いが、gibとの見た目の連続性のためこの順にしてある
		SpawnGib(i);
		m_gibSpawned[i] = true;

		HidePartNode(kJointDefs[i].partNode);
	}
}

void Enemy::HidePartNode(const char* _nodeName)
{
	if (!_nodeName) { return; }

	// 【ボーン潰しをやめた理由】以前は CollapseBone でその骨の拡縮を潰して部位を消していたが、
	//   あれは「その骨にウェイトを持つ頂点を1点へ集める」操作なので、体に残る切り口が
	//   ウェイト境界のギザギザな穴（複数の岩の殻にまたがった開いた弧）になり、
	//   断面を塞ぐ手立てが無かった。
	//   StoneGolem.gltf を本体＋部位5つの6メッシュノードに分け、ノードごと描かない形にすると、
	//   切り口は切断平面＝閉じた輪になり、蓋をモデル側に作り込める（業界標準のやり方）。
	for (KdModelWork::Node& node : m_modelWork.WorkNodes())
	{
		if (node.m_name == _nodeName)
		{
			node.m_visible = false;
			return;
		}
	}
}

void Enemy::SpawnGib(int _index)
{
	if (_index < 0 || _index >= kJointCount) { return; }

	// --- 破片を出す先を見つける(初回だけ探してキャッシュ) ---
	// ※ Init()ではなくここで探すのは、Playerを探すのと同じ理由。
	//   Init()の時点ではシーンの構築が終わっていない
	std::shared_ptr<DebrisSystem> spDebris = m_wpDebrisSystem.lock();
	if (!spDebris)
	{
		for (const std::shared_ptr<KdGameObject>& spObj : SceneManager::Instance().GetObjList())
		{
			spDebris = std::dynamic_pointer_cast<DebrisSystem>(spObj);
			if (spDebris) { break; }
		}
		if (!spDebris) { return; }

		m_wpDebrisSystem = spDebris;
	}

	if (m_gibModelIds[_index] < 0)
	{
		m_gibModelIds[_index] = spDebris->RegisterModel(kJointDefs[_index].gibModel);
	}

	// --- 姿勢は「その骨のワールド行列」そのもの ---
	// gibは Cloude\GltfPartExtract で【骨のローカル空間】に切り出してあるので、
	// 骨のワールド行列を渡すだけで、消えた部位とぴったり同じ位置・向き・大きさで出る。
	// (拡大も入っているので、身長25mのゴーレムでもそのまま合う)
	const KdModelWork::Node* pNode = m_modelWork.FindWorkNode(kJointDefs[_index].bone);
	if (!pNode) { return; }

	const Math::Matrix boneWorld = GetBoneWorldMatrix(*pNode);

	// --- 落とし方 ---
	// 体の中心から関節へ向かう水平方向へ、少しだけ押し出す。
	// 真下に落とすと体に引っかかって不自然に絡むので、外へ逃がす
	const float pushSpeed = DebugParams::Instance().Float(U8("破片/部位の押し出し"), 4.0f, 0.0f, 30.0f);
	const float tumble    = DebugParams::Instance().Float(U8("破片/部位の回転"),     1.5f, 0.0f, 20.0f);

	Math::Vector3 outward = MathAPI::FlattenY(boneWorld.Translation() - GetPos());
	if (!MathAPI::TryNormalize(outward))
	{
		// 関節が体の中心の真上/真下にある(首など)。その場合は正面へ逃がす
		outward = MathAPI::FlattenY(GetDrawMatrix().Backward());
		if (!MathAPI::TryNormalize(outward)) { outward = Math::Vector3::UnitX; }
	}

	const Math::Vector3 velocity = outward * pushSpeed;

	// 外向きと上向きの外積＝「外へ倒れ込む」向きの回転になる(乱数を使わずに自然に見せる)
	const Math::Vector3 angularVelocity = outward.Cross(Math::Vector3::Up) * tumble;

	spDebris->SpawnPiece(m_gibModelIds[_index], boneWorld, velocity, angularVelocity);
}

bool Enemy::GetJointSphere(const JointDef& _joint, Math::Vector3& _outCenter, float& _outRadius) const
{
	if (!GetBoneWorldPos(_joint.bone, _outCenter)) { return false; }

	// 【罠】GetBoneWorldPosはGetDrawMatrix経由なので中心は既にスケール済みのワールド座標。
	//   一方JointDefの半径は【モデル座標系】なので、ここで拡大率を掛けないと
	//   身長25mのゴーレムに0.15m弱の球が並ぶことになる。
	//   逆に中心側でスケールを掛けると先日の「足元補正の二重掛け」と同じ事故になる
	_outRadius = DebugParams::Instance().Float(_joint.radiusKey, _joint.defaultRadius, 0.01f, 1.0f) * GetScale().y;
	return true;
}

void Enemy::DrawJointDebug()
{
	if (!m_pDebugWire) { return; }

	// 狙える的なので目立つ色にする。見たいのは「関節を包めているか」と
	// 「5つが互いに離れていて狙い分けられるか」の2点
	const Math::Color kJointColor = Math::Color(1.0f, 0.85f, 0.2f, 1.0f);

	for (int i = 0; i < kJointCount; ++i)
	{
		Math::Vector3 center{};
		float radius = 0.0f;

		// 壊れた関節は球を出さない(狙えないものを表示すると狙えるように見える)
		if (!GetJointSphereAt(i, center, radius)) { continue; }

		m_pDebugWire->AddDebugSphere(center, radius, kJointColor);

		// 残りHPをその関節の位置に出す。どの関節がどの値かは、別ウィンドウに数値が並んでも
		// 対応が取れない(5つとも似た名前になる)ので、位置の上に直接重ねる
		DebugDraw::AddText3D(center, std::to_string(static_cast<int>(m_jointHp[i])));
	}

	// 本体HPは体の中心に出す(関節と見分けが付くよう位置を分ける)
	DebugDraw::AddText3D(GetPos(), "HP " + std::to_string(static_cast<int>(m_hp)));
}

void Enemy::DrawDebug()
{
	// KdGameObject::DrawDebug は s_showColliderDebug しか見ないので、
	// そのまま呼ぶと登録済みコライダー(緑の球)がカテゴリに関係なく出てしまう。
	// 「敵」がOFFのときは基底の間だけフラグを落とす
	DebugDraw::ScopedGate gate(DebugDraw::Category::Enemy);
	KdGameObject::DrawDebug();
}

void Enemy::Preload()
{
	// モデルとテクスチャを先に読み込んでキャッシュへ載せる。
	//
	// 【なぜ必要か】KdAssetsはパス単位のキャッシュで、最初に要求した時に
	//   glTFの解析・GPUバッファ生成・テクスチャ読み込み(＋ミップ生成)をまとめて行う。
	//   そのため【最初の1体が出現した瞬間に画面がかくつく】。
	//   メカ(W9231)でも同じ症状が出ていた(2026/07/30)。実機で「出た瞬間だけ」と
	//   確認できたので、描画コストではなく読み込みが原因と判明した(2026/07/31)。
	//
	//   シーン開始時に一度読んでおけば、以降の出現は既にキャッシュにあるので無料になる。
	//   ※ モデルを読むとマテリアル経由でテクスチャも一緒に読まれるので、これ1回で足りる
	KdAssets::Instance().m_modeldatas.GetData(kAssetPath);
}

void Enemy::Init()
{
	// 【2026/07/31】敵の見た目を石のゴーレムにした。
	// 画像生成 → Meshyで3D化＋テクスチャ → Mixamoで自動リグ、という経路で作った。
	// 制作手順は Desktop\Cloude\Project\3DGame\Doc\Golem_Pipeline.md
	// ライセンスは CC BY 4.0(Meshyのクレジット必須) → THIRD_PARTY_LICENSES.txt
	SetAsset(kAssetPath);

	// 【2026/08/02】歩行アニメを再生するようにしたので false → true へ変えた。
	//   このモデルは【バインドポーズと歩行クリップで足元の高さが違う】。
	//   ・バインドポーズ(アニメを当てない状態)…つま先Y=-0.933／頭頂Y=+0.934＝原点は体の中心
	//   ・歩行クリップを当てた状態         …つま先Y≒0     ／頭頂Y=+1.81 ＝原点は足元
	//   スキン行列は「逆バインド行列×ボーンの現在位置」(KdStandardShader.cpp:310)なので、
	//   アニメを流し始めた瞬間に見た目が骨のほうへ移り、原点の意味が中心から足元へ変わる。
	//   falseのままだと約12m(0.93×拡大率13.2)浮くので、必ずtrueとセットで有効にすること。
	//   ※ glTFを直接読んで前方運動学で測った値(2026/08/02)。頂点のY範囲から推論すると
	//     バインドポーズしか見えず「原点＝中心」と読み違える
	m_modelOriginIsFeet = true;

	// モデルの実寸は高さ1.899m。街の実測から導いた目標は12〜18mで、
	// 「通りを移動できる幅15mが上限、その比率だと高さ17〜18mが限界」。
	// 【2026/07/31】実機で15m→18mと上げたが、まだ「もっとでかく」とのことで25mへ。
	// ※ 街の実測から導いた当初の上限は「幅15m＝通りを通れる限界、比率で高さ17〜18m」だった。
	//   25mだと幅が約16mになり通りを通り抜けられない可能性がある。
	//   実機で通行を確かめること(通れないなら18m前後へ戻す)
	// 実機で回して決められるようDebugParamsに出してある(出現し直すと反映される)
	float targetHeight = DebugParams::Instance().Float(U8("敵/身長"), 25.0f, 1.0f, 60.0f);
	// 直立(バインドポーズ)での実寸。歩行中は膝が曲がるので実際に立つ高さは
	// この95%前後(実測1.58〜1.81)になる。基準を直立のままにしてあるのは、
	// ここを歩行時の高さに変えると今まで詰めた「敵/身長」の見え方が変わってしまうため
	float modelHeight = 1.899f;

	m_bodyHeight = modelHeight;
	SetScale(Math::Vector3::One * (targetHeight / modelHeight));

	// 体の当たり半径。モデルは幅1.221m(腕を広げた全幅)なので、
	// 胴に寄せて身長の1/4程度を初期値にする。実機で見て詰める値
	m_hitRadius = targetHeight * 0.25f;

	// 本体HP。既定100 ＋ プレイヤーの攻撃力34 ＝ 3発で倒せる設定(2026/08/02にユーザーが選択)
	m_hp = DebugParams::Instance().Float(U8("敵/本体HP"), 100.0f, 1.0f, 1000.0f);

	// 関節HPは表の値 × 倍率。倍率を1本外に出してあるのは、
	// 「関節が1発で壊れる/固すぎる」を関節ごとに直さず一括で詰められるようにするため
	float jointHpScale = DebugParams::Instance().Float(U8("関節/HP倍率"), 1.0f, 0.1f, 5.0f);
	for (int i = 0; i < kJointCount; ++i)
	{
		m_jointHp[i] = kJointDefs[i].maxHp * jointHpScale;
	}

	// 正面は -Z。プレイヤー(GogglesChara)と同じ。
	// 骨から測って確定させた: つま先の向きも左肩の向きもプレイヤーと完全に一致していた
	// (foot→toe が両方とも -Y優勢、右肩→左肩が両方とも +X)。
	//
	// 【なぜ実機で2回「逆」に見えたか】フラグの値のせいではなく、
	//   敵の旋回が MathAPI::RotateToDirection にこのフラグを【渡していなかった】ため。
	//   常に「正面＝+Z」で計算されていたので、true/false どちらでも同じ見た目だった。
	//   敵が対称な立方体だった頃は向きが見えないので露見していなかった
	m_modelForwardIsMinusZ = true;

	// ※ 以前ここに「戦闘メカ(W9231)へ差し替える場合の設定」をコメントで残していたが、
	//   2026/08/03にユーザーの判断でモデルごと削除したため、設定も消した。
	//   そのモデルで得た知見(FBX由来はレスト≠バインドで描画されない／生の頂点座標は
	//   スキン変形後を見ないと分からない)は auto-memory に残してある

	// ※ 当たり判定(KdCollider)は登録しない。立方体へ戻す際も復活させていない。
	//   理由は下記①のとおり「登録しても誰も問い合わせていなかった」ため、
	//   戻すと動かないコードが復活するだけになる。
	//   ①以前登録していた "EnemyDamage"(TypeDamage)は Src/Application 全体で
	//     誰も問い合わせていなかった(実際の命中判定はPlayer側の距離チェック)
	//   ②部位破壊の判定は【骨のワールド座標から毎フレーム球を作る自前計算】で行う。
	//     KdColliderに登録した形状はモデルのローカル座標に固定で、
	//     RegisterCollisionShapeがemplace(同名で上書きされない)なので、
	//     アニメーションで動く骨には追従できない
	//   ③モデルを RegisterCollisionShape に渡してはいけない。
	//     このモデルはCOLノードを持たないので、渡すと見た目の70,832三角が
	//     そのまま当たり判定メッシュになる(KdModel.cpp:107-110のフォールバック)
}

void Enemy::OnHit(KdGameObject* /*_other*/)
{
	// 部位を指定しない当たり方(反撃・レーザーなど、命中位置を持たない経路)。
	// 関節を壊さず本体HPだけを削る。
	// ※ 以前はここで即 m_isExpired = true にしていた(＝一撃で消滅)。
	//   本体HPを入れたので、部位を狙った攻撃と同じ土俵で減るようにした
	ApplyBodyDamage(GetAttackPower());
}

float Enemy::GetAttackPower()
{
	// プレイヤーの一撃の威力。本体HP100に対して34＝3発で倒せる
	return DebugParams::Instance().Float(U8("プレイヤー/攻撃力"), 34.0f, 1.0f, 200.0f);
}

void Enemy::ApplyBodyDamage(float _damage)
{
	m_hp -= _damage;
	if (m_hp > 0.0f) { return; }

	m_hp = 0.0f;
	m_isExpired = true;
}

void Enemy::Update()
{
	// 追従対象が未設定なら、シーン内のPlayerを自動で探す
	// (レベルエディタ配置など、外部からSetTarget()を呼ばれない経路のため)
	// ※ Init()の時点ではSceneManagerのシングルトン初期化(=GameScene::Init())が
	//    完了していない場合があり、ここでSceneManager::Instance()を呼ぶと
	//    自己再入でフリーズするため、Update()まで遅延させている
	if (m_wpTarget.expired())
	{
		if (std::shared_ptr<KdGameObject> spPlayer = SceneManager::Instance().FindObjectWithTag(ObjectTag::Player))
		{
			m_wpTarget = spPlayer;
		}
	}

	// 【確認用】動いていると関節の球を見比べられないので、その場に固定できるようにする。
	// AIと移動だけを止め、接地(PostUpdateのGroundCheck)は生かして立たせたままにする。
	// ※ アニメはSelectAnimationSpeedが0を返して凍る。呼ぶのをやめてはいけない → そちらのコメント
	if (IsFrozenForDebug()) { return; }

	std::shared_ptr<KdGameObject> spTarget = m_wpTarget.lock();
	if (!spTarget) { return; }

	const float dt = Application::Instance().GetDeltaTime();

	Math::Vector3 pos       = GetPos();
	Math::Vector3 targetPos = spTarget->GetPos();

	// 対象への水平方向と距離
	Math::Vector3 toTarget = MathAPI::FlattenY(targetPos - pos);
	float distXZ = toTarget.Length();
	Math::Vector3 dirToTarget = MathAPI::GetSafeNormal(toTarget, Math::Vector3::Backward);

	// 対象の方を向く小関数(突進中は向きを固定したいので状態側で使い分ける)
	auto faceTarget = [&]()
	{
		float turnSpeedDeg = DebugParams::Instance().Float(U8("敵/旋回速度"), 180.0f, 0.0f, 720.0f);
		Math::Vector3 rot = GetRot();
		// 🔴 m_modelForwardIsMinusZ を必ず渡すこと。渡さないと既定の「正面＝+Z」で
		//   計算され、モデルがずっと逆を向く(2026/07/31にこれで2回外した)
		rot.y = MathAPI::RotateToDirection(rot.y, dirToTarget, turnSpeedDeg * dt, m_modelForwardIsMinusZ);
		SetRot(rot);
	};

	switch (m_state)
	{
	case State::Chase:
	{
		// 攻撃開始距離まで近づいたら予備動作へ(その場で予告)。
		//
		// 【体の表面から測ること】(2026/08/02)
		//   ここを中心からの距離にすると、25mのゴーレム(体の半径6.25m)では
		//   プレイヤーが体の【内側】へ入るまで近づき続ける。誰も敵をTypeBumpに
		//   登録していないので押し戻されず、カメラ(地上2.3m)がゴーレムの股の間に入り、
		//   裏面カリングで【敵が消えたように見える】。
		//   実測: 停止時の水平距離が0.35〜0.8mで、体の半径6.25mの中に完全に埋まっていた。
		//   敵が1辺1mの立方体だった頃の2.5mがそのまま残っていたのが原因
		float atkStart = m_hitRadius
			+ DebugParams::Instance().Float(U8("敵/攻撃開始の間合い"), 2.5f, 0.5f, 20.0f);
		if (distXZ <= atkStart)
		{
			m_state = State::Windup;
			m_stateTimer = DebugParams::Instance().Float(U8("敵/予備動作時間"), 0.45f, 0.05f, 2.0f);
			break;
		}

		// 追従移動(対象へゆっくり近づく)
		float moveSpeed = GetMoveSpeed();
		pos += dirToTarget * moveSpeed * dt;
		SetPos(pos);
		faceTarget();
		break;
	}
	case State::Windup:
	{
		// その場で予告(向きだけ対象へ合わせ続ける=どこへ突っ込むか読める)
		faceTarget();
		m_stateTimer -= dt;
		if (m_stateTimer <= 0.0f)
		{
			// 突進開始。方向をここで固定する(以後は追尾しないので回避で避けられる)
			m_state = State::Strike;
			m_stateTimer = DebugParams::Instance().Float(U8("敵/突進時間"), 0.22f, 0.05f, 1.0f);
			m_lungeDir = dirToTarget;
		}
		break;
	}
	case State::Strike:
	{
		// 固定方向へ高速で突っ込む
		float lungeSpeed = GetLungeSpeed();
		pos += m_lungeDir * lungeSpeed * dt;
		SetPos(pos);
		m_stateTimer -= dt;

		// 命中判定：対象に十分近ければ命中処理(反撃 or ノックバック)して硬直へ。
		//
		// 【必ず水平距離で測ること】(2026/08/02に実測で判明)
		//   ここは3D距離だった。位置(GetPos)は【体の中心】なので、身長25mのゴーレムでは
		//   中心が地上13.1m、プレイヤーは1.55m＝【縦だけで11.55m離れている】。
		//   3D距離は絶対に6.75mを下回れず、突進が永遠に命中しなかった。
		//   命中しない＝硬直→追従→また突進を繰り返して前進し続け、最後はプレイヤーに
		//   完全に重なる(実測: 停止時の水平距離0.03〜2.8m)。カメラが体の内側に入るので
		//   【敵が消えたように見える】という症状になっていた。
		//   追従側(distXZ)は最初からFlattenYで水平を見ており、ここだけ揃っていなかった
		float hitDist = m_hitRadius + 0.5f;   // プレイヤー半径ぶん少し余裕を持たせる
		if (MathAPI::FlattenY(targetPos - GetPos()).Length() <= hitDist)
		{
			ResolveStrikeHit(spTarget);
			EnterRecover();
			break;
		}
		// 当たらずに突進時間が切れたら空振りで硬直へ
		if (m_stateTimer <= 0.0f)
		{
			EnterRecover();
		}
		break;
	}
	case State::Recover:
	{
		// 突進の後隙。硬直が明けたら追従に戻る
		m_stateTimer -= dt;
		if (m_stateTimer <= 0.0f)
		{
			m_state = State::Chase;
		}
		break;
	}
	}

	// 見た目：状態で色を変えて攻撃を予告する(黄=予備動作 / 明るい赤=突進)。
	// 【2026/07/31】通常時は白(＝色味を掛けない)。
	//   ここは基本色テクスチャへの【乗算】なので、赤を入れるとゴーレム全体が赤く染まって
	//   苔も割れ目の光も台無しになる。立方体だった頃は見分けるために赤くしていたが、
	//   テクスチャを持つモデルでは白のままにすること。
	//   予備動作と突進の色は攻撃の予告として機能するので残す
	switch (m_state)
	{
	case State::Windup: m_color = Math::Color(1.0f, 0.9f, 0.2f, 1.0f); break;
	case State::Strike: m_color = Math::Color(1.0f, 0.35f, 0.2f, 1.0f); break;
	default:            m_color = kWhiteColor; break;
	}
}

void Enemy::ResolveStrikeHit(const std::shared_ptr<KdGameObject>& _target)
{
	Player* pPlayer = dynamic_cast<Player*>(_target.get());
	if (!pPlayer) { return; }

	if (pPlayer->IsInvincible())
	{
		// ジャスト回避成立 → Player側に反撃(スロー窓)を通知する。
		// 自分は消滅させず空振り扱い(呼び出し側でEnterRecoverする)。プレイヤーはこの後の突撃で狩れる
		pPlayer->NotifyCounter();
	}
	else
	{
		// 無防備で被弾 → プレイヤーを外向きにノックバック(HPは無い=勢いを崩すだけ)
		Math::Vector3 knockDir = _target->GetPos() - GetPos();
		float power = DebugParams::Instance().Float(U8("敵/ノックバック力"), 8.0f, 0.0f, 40.0f);
		pPlayer->ApplyKnockback(knockDir, power);
	}
}

float Enemy::GetMoveSpeed() const
{
	return DebugParams::Instance().Float(U8("敵/移動速度"), 1.5f, 0.0f, 40.0f);
}

float Enemy::GetLungeSpeed() const
{
	return DebugParams::Instance().Float(U8("敵/突進速度"), 14.0f, 1.0f, 40.0f);
}

std::string Enemy::SelectAnimation() const
{
	// 持っているアニメが歩行1本だけなので、どの状態でもこれを流し、違いは再生速度で付ける。
	// 攻撃・被弾・死亡が揃ったら、ここを状態で分岐させる(Player::SelectAnimationと同じ形)
	return kWalkAnimName;
}

bool Enemy::IsFrozenForDebug() const
{
	return DebugFlags::Instance().Get(U8("敵/動きを止める"), false);
}

float Enemy::SelectAnimationSpeed() const
{
	// 【確認用】止めているときは再生速度0で姿勢を凍らせる。
	// 【罠】「UpdateAnimationを呼ばない」で止めてはいけない。このモデルはバインドポーズと
	//   歩行クリップで足元の高さが0.93違い、m_modelOriginIsFeet=true はクリップを当てている
	//   前提の設定なので、一度も姿勢を当てないと約12m浮く。
	//   KdAnimator::AdvanceTime は【先に姿勢を当ててから時間を進める】(KdAnimation.cpp:171→177)
	//   ので、速度0で呼び続ければ姿勢はそのまま保たれる
	if (IsFrozenForDebug()) { return 0.0f; }

	// 【足を滑らせないための計算】
	// この歩行はその場歩き(腰の水平移動が±0.14mしかない=ルートモーション無し)なので、
	// 接地している足は体に対して「実際の歩行速度」ぶんだけ後ろへ流れる。
	// その速さをglTFから前方運動学で実測すると、等倍スケールで 1.544 m/s だった
	// (接地している間につま先が後退する量 1.028m ÷ 0.666秒。2026/08/02計測)。
	// モデルは身長25mへ拡大しているので、滑らない速さも同じ倍率だけ上がる
	constexpr float kWalkGroundSpeedAtScaleOne = 1.544f;

	float noSlideSpeed = kWalkGroundSpeedAtScaleOne * GetScale().y;
	if (noSlideSpeed <= 0.0f) { return 1.0f; }

	// いま実際に出している水平の速さ。この敵は状態で速度が決まっている
	float currentSpeed = 0.0f;
	switch (m_state)
	{
	case State::Strike:
		currentSpeed = GetLungeSpeed();
		break;
	case State::Chase:
		currentSpeed = GetMoveSpeed();
		break;
	default:
		// 予備動作(Windup)と硬直(Recover)はその場に止まっている
		break;
	}

	// 見た目の重さは好みなので、計算どおり(=1.0)から外せるようにしておく。
	// 大きくすると同じ移動速度でも足の回転が速くなり、軽い生き物に見える
	float weight = DebugParams::Instance().Float(U8("敵/歩行アニメ倍率"), 1.0f, 0.1f, 5.0f);

	// 止まっている間に完全な0にすると歩幅の途中で固まり「バグで止まった」ように見える。
	// ごく遅く動かし続けて、重心を移し替えているように見せる
	constexpr float kMinAnimSpeed = 0.05f;

	float speed = currentSpeed / noSlideSpeed * weight;
	if (speed < kMinAnimSpeed)
	{
		speed = kMinAnimSpeed;
	}
	return speed;
}

void Enemy::EnterRecover()
{
	m_state = State::Recover;
	m_stateTimer = DebugParams::Instance().Float(U8("敵/硬直時間"), 0.7f, 0.0f, 3.0f);
}

void Enemy::PostUpdate()
{
	// ※ 以前は「対象に接触したら敵が消滅」する仮処理だったが、攻撃(突進)に一本化したため撤去。
	//    敵はプレイヤーの攻撃(OnHit)か反撃でのみ消滅する

	// 地面(KdCollider::TypeGround)に立つ
	GroundCheck();

	// アニメーションを進める。接地・位置・状態が全て確定したあとで呼ぶ
	// (Playerと同じくPostUpdateの最後。CLAUDE.mdの「PostUpdate＝world状態の解決」に合わせる)
	UpdateAnimation();

	// 壊れた関節を潰し直す。UpdateAnimationが毎フレーム骨を書き戻すので、必ずその【後】に呼ぶ
	UpdateBrokenJoints();

	// 【部位破壊の方式確認】ボーンを潰すと部位が消えるかを実機で確かめる。
	// UpdateAnimationが毎フレーム骨を書き戻すので、必ずその【後】に呼ぶ(前だと塗り潰される)
	UpdateBoneCollapseTest();

	// デバッグ表示は最後にまとめる。
	// 【なぜ最後か】接地(GroundCheck)で位置が、アニメ(UpdateAnimation)で骨が動くので、
	//   ここより前で描くと1フレーム古い位置に線が出て、モデルからずれて見える
	if (KdGameObject::s_showColliderDebug && DebugDraw::IsOn(DebugDraw::Category::Enemy))
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}

		// 狙える関節の球(半径を目で見て決めるため)
		DrawJointDebug();

		// 体全体の接触判定(m_hitRadius)。身長25mだと半径6.25mの球になり、
		// 関節の球(1.7〜2m)を完全に飲み込んで狙い分けが見えなくなるので既定はOFF
		if (DebugFlags::Instance().Get(U8("敵/体の判定も出す"), false))
		{
			m_pDebugWire->AddDebugSphere(GetPos(), m_hitRadius, kRedColor);
		}
	}
}
