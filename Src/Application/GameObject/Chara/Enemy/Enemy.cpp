#include "Enemy.h"

#include "../../../main.h"
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugDraw/DebugDraw.h"
#include "../../../Debug/DebugFlags/DebugFlags.h"
#include "../../Debris/DebrisSystem.h"   // 関節が壊れたとき、その部位を落とすため

// 本体(部位を切り出した残り)のメッシュノード名。全身破砕でここを消す。
// 分割ツールの設定(Cloude\Project\3DGame\GltfPartExtract\StoneGolem.parts.json の
// bodyNodeName)と揃えること
static constexpr const char* kBodyNodeName = "Body";

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
	  "Asset/Models/Character/StoneGolem/Gib_Head.gltf",         "Part_Head",         false },

	{ U8("左肘"), "mixamorig:LeftForeArm",  U8("関節/半径_肘"), 0.13f, 1.0f, 30.0f,
	  "Asset/Models/Character/StoneGolem/Gib_LeftForeArm.gltf",  "Part_LeftForeArm",  false },

	{ U8("右肘"), "mixamorig:RightForeArm", U8("関節/半径_肘"), 0.13f, 1.0f, 30.0f,
	  "Asset/Models/Character/StoneGolem/Gib_RightForeArm.gltf", "Part_RightForeArm", false },

	// 膝を壊されたら倒れる。片方でも壊れれば立ち上がれない
	{ U8("左膝"), "mixamorig:LeftLeg",      U8("関節/半径_膝"), 0.14f, 1.0f, 40.0f,
	  "Asset/Models/Character/StoneGolem/Gib_LeftLeg.gltf",      "Part_LeftLeg",      true  },

	{ U8("右膝"), "mixamorig:RightLeg",     U8("関節/半径_膝"), 0.14f, 1.0f, 40.0f,
	  "Asset/Models/Character/StoneGolem/Gib_RightLeg.gltf",     "Part_RightLeg",     true  },
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

			// 膝を壊されたら倒れる。以後は立ち上がれない
			if (joint.causesDown)
			{
				EnterDown(false);
			}
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

		HideMeshNode(kJointDefs[i].partNode);
	}
}

void Enemy::HideMeshNode(const char* _nodeName)
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

std::shared_ptr<DebrisSystem> Enemy::FindDebrisSystem()
{
	std::shared_ptr<DebrisSystem> spDebris = m_wpDebrisSystem.lock();
	if (spDebris) { return spDebris; }

	for (const std::shared_ptr<KdGameObject>& spObj : SceneManager::Instance().GetObjList())
	{
		spDebris = std::dynamic_pointer_cast<DebrisSystem>(spObj);
		if (spDebris) { break; }
	}

	m_wpDebrisSystem = spDebris;
	return spDebris;
}

void Enemy::SpawnGib(int _index)
{
	if (_index < 0 || _index >= kJointCount) { return; }

	std::shared_ptr<DebrisSystem> spDebris = FindDebrisSystem();
	if (!spDebris) { return; }

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

	// 【2026-08-04】その場で消えるのをやめ、まず倒れるようにした。
	// 【2026-08-05】倒れ切ったあと Shatter() で砕け、そこで初めて消える
	EnterDown(true);
}

void Enemy::EnterDown(bool _isDead)
{
	// 既に倒れている場合は倒れ直さない。
	// 膝を壊されて倒れたあとに本体HPが尽きる順序があるので、そのときは
	// 「生きたダウン → 死んだダウン」へ変わるだけでよい
	if (m_state == State::Down)
	{
		if (_isDead && !m_isDead)
		{
			m_isDead = true;
			m_downTimer = GetShatterDelayTime();
		}
		return;
	}

	m_state = State::Down;

	m_isDead = _isDead;
	m_downTimer = GetShatterDelayTime();

	// 倒れた体が水平の勢いを持ったまま滑っていかないように消す
	m_velocity.x = 0.0f;
	m_velocity.z = 0.0f;
}

void Enemy::UpdateDown(float _dt)
{
	// 倒れ切るまでは何もしない。
	// 【2026-08-04に実機で出た不具合2つを、ここ1箇所で塞いでいる】
	//   ・倒れている最中に向き直りが始まり、体が滑るように回っていた
	//   ・消滅までの時間が倒れ始めから数えられ、モーションの途中で消えていた
	//     (倒れるモーションは3.92秒あるのに、消えるまでが3.0秒だった)
	if (!IsFallFinished()) { return; }

	// --- 死んで倒れた場合 ---
	// 倒れ伏した姿を少しだけ見せてから砕く(0にすれば倒れ終わった瞬間に砕ける)
	if (m_isDead)
	{
		m_downTimer -= _dt;

		if (m_downTimer <= 0.0f)
		{
			Shatter();
		}
		return;
	}

	// --- 膝を壊されて倒れた(まだ生きている)場合 ---
	// 【案1：その場でゆっくり向き直る】(2026-08-04にユーザーが選択)
	//   倒れたまま向きを変えられないと、プレイヤーが背後へ回るだけで完全な安全地帯になり、
	//   ダウンが「危険な状態」でなく「ただの休憩」になってしまう。
	//   這って向き直るモーション(Low Crawl)を使う案もあるが、まずは回転だけで様子を見て、
	//   見た目が足りなければそちらへ差し替える
	std::shared_ptr<KdGameObject> spTarget = m_wpTarget.lock();
	if (!spTarget) { return; }

	Math::Vector3 toTarget    = MathAPI::FlattenY(spTarget->GetPos() - GetPos());
	Math::Vector3 dirToTarget = MathAPI::GetSafeNormal(toTarget, Math::Vector3::Backward);

	// 立っているときより明確に遅くする(這って向きを変えている重さを出すため)
	float turnSpeedDeg = DebugParams::Instance().Float(U8("敵/ダウン中の旋回速度"), 30.0f, 0.0f, 180.0f);

	Math::Vector3 rot = GetRot();
	// 🔴 m_modelForwardIsMinusZ を必ず渡すこと(渡さないとモデルがずっと逆を向く)
	rot.y = MathAPI::RotateToDirection(rot.y, dirToTarget, turnSpeedDeg * _dt, m_modelForwardIsMinusZ);
	SetRot(rot);
}

bool Enemy::IsFallFinished() const
{
	// 倒れるモーションを持たないモデルでは、待っても終わらない。
	// 「終わった」扱いにしないと死体が永久に消えなくなるので、無い場合は即trueを返す
	if (!m_modelWork.GetAnimation(kFallAnimName)) { return true; }

	// アニメの切り替えはPostUpdate(UpdateAnimation)で起きるので、倒れた直後の数フレームは
	// まだ歩行が流れている。名前で確かめてからでないと、ループしている歩行が末尾に来た
	// 瞬間を「倒れ終わった」と誤認する
	if (m_currentAnimName != kFallAnimName) { return false; }

	return m_animator.IsAnimationEnd();
}

float Enemy::GetShatterDelayTime() const
{
	// 倒れ切ってから数え始める(倒れ始めからではない)
	return DebugParams::Instance().Float(U8("敵/倒れてから砕けるまでの秒数"), 1.0f, 0.0f, 10.0f);
}

void Enemy::Shatter()
{
	if (m_shattered) { return; }
	m_shattered = true;

	// --- ① まだ体に付いている部位を、部位破壊とまったく同じやり方で落とす ---
	// 既に壊れている部位は m_gibSpawned が立っているので二重には落ちない。
	// 【なぜ本体と分けるか】部位はモデルが既に「閉じたメッシュ」に分かれていて、
	//   落とす仕組みも実機で動いている。砕く対象を本体だけに絞れるので、
	//   壊した脚が破片として復活する心配も原理的に無くなる
	for (int i = 0; i < kJointCount; ++i)
	{
		if (m_gibSpawned[i]) { continue; }

		SpawnGib(i);
		m_gibSpawned[i] = true;

		HideMeshNode(kJointDefs[i].partNode);
	}

	// --- ② 本体を消す ---
	HideMeshNode(kBodyNodeName);

	// --- ③ 本体の破片を出す ---
	std::shared_ptr<DebrisSystem> spDebris = FindDebrisSystem();
	if (spDebris)
	{
		// 【5-Aの仮実装】本物の破片モデルがまだ無いので、既存のgibを数だけ揃えて撒く。
		//   狙いは見た目ではなく【生成コストの実測】。本番と同じ SpawnDebrisConvex を
		//   同じ個数だけ通るので、ここが詰まるかどうかをアセットを作る前に判定できる。
		//   破片モデルができたら、この1呼び出しが Frag_00〜29 の生成に置き換わる
		const int count = DebugParams::Instance().Int(U8("破片/全身破砕の数"), 30, 1, 120);

		const int modelId = spDebris->RegisterModel(kJointDefs[0].gibModel);

		// 撒く中心は腰の骨。倒れた姿勢では原点(足元)から体が離れているので、
		// GetPos()を使うと足元にだけ湧いて不自然になる
		Math::Vector3 center;
		if (!GetBoneWorldPos("mixamorig:Hips", center)) { center = GetPos(); }

		// 身長25mに対して既定の散らばり(1.5m)では1点に固まって押し合うので、別のキーにする
		const float spread = DebugParams::Instance().Float(U8("破片/全身破砕の散らばり"), 6.0f, 0.0f, 30.0f);

		spDebris->SpawnBurstOfModel(modelId, center, GetScale(), spread, count);
	}

	// 破片へ引き継いだので本体は退場する
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

	const float dt = Application::Instance().GetDeltaTime();

	// 倒れている間は追従も攻撃もしない。向き直りと消滅の管理だけ行う。
	// ※ 対象を見失っていても消滅までは進めたいので、targetの取得より【前】に置く
	if (m_state == State::Down)
	{
		UpdateDown(dt);
		return;
	}

	std::shared_ptr<KdGameObject> spTarget = m_wpTarget.lock();
	if (!spTarget) { return; }

	Math::Vector3 pos       = GetPos();
	Math::Vector3 targetPos = spTarget->GetPos();

	// 対象への水平方向と距離
	Math::Vector3 toTarget = MathAPI::FlattenY(targetPos - pos);
	float distXZ = toTarget.Length();
	Math::Vector3 dirToTarget = MathAPI::GetSafeNormal(toTarget, Math::Vector3::Backward);

	// 対象の方を向く
	// 🔴 m_modelForwardIsMinusZ を必ず渡すこと。渡さないと既定の「正面＝+Z」で
	//   計算され、モデルがずっと逆を向く(2026/07/31にこれで2回外した)
	float turnSpeedDeg = DebugParams::Instance().Float(U8("敵/旋回速度"), 180.0f, 0.0f, 720.0f);
	Math::Vector3 rot = GetRot();
	rot.y = MathAPI::RotateToDirection(rot.y, dirToTarget, turnSpeedDeg * dt, m_modelForwardIsMinusZ);
	SetRot(rot);

	// 追従して、間合いまで来たら止まる。
	//
	// 【必ず体の表面から測ること】(2026/08/02に実測で判明)
	//   中心からの距離で測ると、25mのゴーレム(体の半径6.25m)ではプレイヤーが体の
	//   【内側】へ入るまで近づき続ける。誰も敵をTypeBumpに登録していないので押し戻されず、
	//   カメラ(地上2.3m)がゴーレムの股の間に入り、裏面カリングで
	//   【敵が消えたように見える】という症状になる
	float stopDist = m_hitRadius
		+ DebugParams::Instance().Float(U8("敵/止まる間合い"), 2.5f, 0.5f, 20.0f);

	// 実際に動いた速さを覚えておく(SelectAnimationSpeedが足を滑らせない倍率に使う)。
	// 状態から逆算すると「止まっているのに歩いている」ズレが出るので、結果を持たせる
	m_currentMoveSpeed = 0.0f;

	if (distXZ > stopDist)
	{
		m_currentMoveSpeed = GetMoveSpeed();
		pos += dirToTarget * m_currentMoveSpeed * dt;
		SetPos(pos);
	}
}

float Enemy::GetMoveSpeed() const
{
	return DebugParams::Instance().Float(U8("敵/移動速度"), 1.5f, 0.0f, 40.0f);
}

std::string Enemy::SelectAnimation() const
{
	// 倒れている間は「倒れる」を1回だけ流し、最終フレームのポーズで留まる
	// (SelectAnimationLoopがfalseを返すため)。これがそのままダウン中の待機になる。
	// ※ kFallAnimNameはまだモデルに入っていないので、今は見つからず歩行のまま流れ続ける
	if (m_state == State::Down) { return kFallAnimName; }

	// 持っているアニメが歩行1本だけなので、どの状態でもこれを流し、違いは再生速度で付ける。
	// 攻撃・被弾が揃ったら、ここを状態で分岐させる(Player::SelectAnimationと同じ形)
	return kWalkAnimName;
}

bool Enemy::SelectAnimationLoop() const
{
	// 「倒れる」だけはループさせない。最後のフレームで止まり、その姿勢のまま留まる
	return m_currentAnimName != kFallAnimName;
}

float Enemy::SelectAnimationBlendTime() const
{
	// 歩き→倒れるの切り替わりが1フレームで飛ぶのを防ぐ。
	// 倒れるモーションの序盤を切るほど、切り替え前後のポーズ差が大きくなって目立つ。
	// 0にすれば従来どおりの即差し替えに戻せる
	return DebugParams::Instance().Float(U8("敵/アニメ切り替えを混ぜる秒数"), 0.15f, 0.0f, 0.5f);
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

	// 倒れるモーションは等速で流す。
	// 以下の「足を滑らせない倍率」は歩行クリップ前提の計算なので、倒れる動きには意味が無い
	if (m_state == State::Down) { return 1.0f; }

	// 【足を滑らせないための計算】
	// この歩行はその場歩き(腰の水平移動が±0.14mしかない=ルートモーション無し)なので、
	// 接地している足は体に対して「実際の歩行速度」ぶんだけ後ろへ流れる。
	// その速さをglTFから前方運動学で実測すると、等倍スケールで 1.544 m/s だった
	// (接地している間につま先が後退する量 1.028m ÷ 0.666秒。2026/08/02計測)。
	// モデルは身長25mへ拡大しているので、滑らない速さも同じ倍率だけ上がる
	constexpr float kWalkGroundSpeedAtScaleOne = 1.544f;

	float noSlideSpeed = kWalkGroundSpeedAtScaleOne * GetScale().y;
	if (noSlideSpeed <= 0.0f) { return 1.0f; }

	// いま実際に出している水平の速さ(Updateが実際に動かした結果を持っている)
	float currentSpeed = m_currentMoveSpeed;

	// 見た目の重さは好みなので、計算どおり(=1.0)から外せるようにしておく。
	// 大きくすると同じ移動速度でも足の回転が速くなり、軽い生き物に見える
	float weight = DebugParams::Instance().Float(U8("敵/歩行アニメ倍率"), 1.0f, 0.1f, 5.0f);

	// 止まっている間の再生速度。
	//
	// 【2026-08-04】突進を撤去して「間合いで止まったまま」の時間ができたことで、
	//   ここが実際に見えるようになった。既定の0.05だと歩行1周に約57秒かかり、
	//   スローモーションで足踏みしているように見える(実機で指摘された)。
	//   0にすると歩幅の途中で完全に固まり「バグで止まった」ように見えるので、
	//   どこが自然かは目で見て決めるしかない → DebugParamsへ出した。
	//
	// 🔴 本来の解は【待機モーションを持たせること】。この値は待機アニメが無い間のつなぎで、
	//   歩行クリップを遅回しして「止まっている」を表現しているに過ぎない
	float minSpeed = DebugParams::Instance().Float(U8("敵/停止時のアニメ倍率"), 0.25f, 0.0f, 1.0f);

	float speed = currentSpeed / noSlideSpeed * weight;
	if (speed < minSpeed)
	{
		speed = minSpeed;
	}
	return speed;
}

void Enemy::PostUpdate()
{
	// ※ 敵はプレイヤーの攻撃(OnHit / 部位破壊)でのみ倒れる。
	//    突進による接触ダメージは 2026-08-04 に撤去した(下記)

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
