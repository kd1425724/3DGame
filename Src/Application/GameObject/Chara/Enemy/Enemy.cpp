#include "Enemy.h"

#include "../../../main.h"
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugDraw/DebugDraw.h"
#include "../../../Debug/DebugFlags/DebugFlags.h"
#include "../../Debris/DebrisSystem.h"   // 関節が壊れたとき、その部位を落とすため
#include "../../../Utility/JsonManager.h"   // 全身破砕の破片の表(fragments.json)を読むため
#include "../Player/Player.h"                // 攻撃が当たったとき反撃/ノックバックを通知するため
#include "../../../Debug/DebugWatch/DebugWatch.h"   // すり抜けの検知(手の1フレーム移動量)

// 本体(部位を切り出した残り)のメッシュノード名。全身破砕でここを消す。
// 分割ツールの設定(Cloude\Project\3DGame\GltfPartExtract\StoneGolem.parts.json の
// bodyNodeName)と揃えること
static constexpr const char* kBodyNodeName = "Body";

// 腰の骨。破片を外向きに散らすときの「体の中心」に使う
static constexpr const char* kHipsBoneName = "mixamorig:Hips";

// 攻撃に使う腕の骨。肩から手へ向かって並べる。
// 【なぜ複数か】手だけに判定を付けると、腕の中ほどに当たっても素通りする。
//   骨と骨の間も補間して球を置くので、腕全体が一本の当たり判定になる
static constexpr const char* kAttackArmBonesRight[] =
{
	"mixamorig:RightArm",       // 上腕
	"mixamorig:RightForeArm",   // 前腕
	"mixamorig:RightHand",      // 手
};
static constexpr const char* kAttackArmBonesLeft[] =
{
	"mixamorig:LeftArm",
	"mixamorig:LeftForeArm",
	"mixamorig:LeftHand",
};
static constexpr int kAttackArmBoneCount = _countof(kAttackArmBonesRight);

// 骨と骨の間に追加で置く球の数。0なら骨の位置だけ
static constexpr int kAttackSphereBetween = 2;

// 攻撃アニメの名前。左右で素材が分かれている(Mixamoで両方落としてある)
static constexpr const char* kSlamRightAnimName = "slamR";
static constexpr const char* kSlamLeftAnimName  = "slamL";

// 待機アニメ。無いと止まったとき歩行を遅回しすることになり「歩いて見える」
static constexpr const char* kIdleAnimName = "idle";

// 被弾リアクション。素材は4.58秒あるが、頭から数フレームだけ見せて切り上げる
static constexpr const char* kHitAnimName = "hit";

// 掃引で刻む球の上限。増やすほど確実だが重くなる
static constexpr int kMaxSweepSteps = 16;

// 全身破砕の破片の表と置き場所。破砕ツール(Cloude\GltfFracture)の出力。
// 【コードに焼かない理由】破片の数も骨の割り当てもツールの出力で決まるので、
// 表を読む形にしておかないと、流し直すたびに手で書き換えることになる
static constexpr const char* kFragmentDir       = "Asset/Models/Character/StoneGolem/";
static constexpr const char* kFragmentTablePath = "Asset/Models/Character/StoneGolem/fragments.json";

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

bool Enemy::GetCameraLockPos(Math::Vector3& _outPos) const
{
	// 胸(Spine1)を第一候補にする。頭だと首を振ったぶん動き、腰だと低すぎて
	// 身長25mのゴーレムでは上半身が画面から切れる
	static const char* const kLockBones[] =
	{
		"mixamorig:Spine1",
		"mixamorig:Hips",     // 胸が無いモデルへの保険
	};

	for (const char* bone : kLockBones)
	{
		if (GetBoneWorldPos(bone, _outPos)) { return true; }
	}

	return false;
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

bool Enemy::ReadFragmentTable(std::vector<FragmentDef>& _out)
{
	nlohmann::json json;
	if (!JsonManager::Instance().Read(kFragmentTablePath, json)) { return false; }

	_out.clear();

	try
	{
		for (const nlohmann::json& item : json)
		{
			if (!item.contains("name")) { continue; }
			if (!item.contains("bone")) { continue; }

			FragmentDef def;
			def.modelPath = std::string(kFragmentDir) + item["name"].get<std::string>() + ".gltf";
			def.bone      = item["bone"].get<std::string>();
			_out.push_back(def);
		}
	}
	catch (const nlohmann::json::exception&)
	{
		// 壊れていたら破砕なしで通す。ここで落とすとゲームが起動しなくなる
		_out.clear();
		return false;
	}

	return !_out.empty();
}

void Enemy::PreloadDebrisAssets(DebrisSystem& _debris)
{
	// gib(部位破壊で落ちる5個)
	for (int i = 0; i < kJointCount; ++i)
	{
		_debris.RegisterModel(kJointDefs[i].gibModel);
	}

	// 全身破砕の破片(30個)
	std::vector<FragmentDef> fragments;
	if (!ReadFragmentTable(fragments)) { return; }

	for (const FragmentDef& frag : fragments)
	{
		_debris.RegisterModel(frag.modelPath);
	}
}

void Enemy::PreloadDebrisModels()
{
	if (m_debrisPreloaded) { return; }

	std::shared_ptr<DebrisSystem> spDebris = FindDebrisSystem();
	if (!spDebris) { return; }   // シーン構築が済むまでは何度でも試す

	m_debrisPreloaded = true;

	// 【ここは重くない】実体の読み込みと凸包の構築は Enemy::PreloadDebrisAssets が
	//   シーン構築時に済ませてある。RegisterModel は同じパスなら登録済みのIDを返すだけ。
	//   ここでやっているのは「自分用にIDを控える」だけ。
	//
	// 🔴 ここで初めて登録する形にしてはいけない。敵はスポナーで後から湧くので、
	//   35モデルの読み込みがゲーム中に走ってFPSが5まで落ちる(2026-08-05に実測)
	for (int i = 0; i < kJointCount; ++i)
	{
		m_gibModelIds[i] = spDebris->RegisterModel(kJointDefs[i].gibModel);
	}

	if (ReadFragmentTable(m_fragments))
	{
		for (FragmentDef& frag : m_fragments)
		{
			frag.modelId = spDebris->RegisterModel(frag.modelPath);
		}
	}
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

void Enemy::NotifyHitReaction()
{
	// 倒れている間は怯まない(倒れるモーションが上書きされてしまう)
	if (m_state == State::Down) { return; }

	// 🔴 攻撃中は怯ませない（ハイパーアーマー）。
	//   振りかぶり〜硬直の間に怯むと、プレイヤーが殴り続けるだけで敵が何もできなくなる
	//   （はめ殺し）。大型の敵が攻撃中に怯まないのは、アクションゲームで広く使われる作り。
	//   避ける・待つという読み合いを残すために要る
	if (m_state != State::Chase) { return; }

	m_hitReactTimer = DebugParams::Instance().Float(U8("敵/被弾リアクションの秒数"), 0.6f, 0.0f, 3.0f);
}

void Enemy::ApplyBodyDamage(float _damage)
{
	m_hp -= _damage;

	NotifyHitReaction();

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
		// 破片は「支配ボーンのローカル空間」へ焼き込んであるので、その骨の姿勢を
		// 渡すだけで、砕ける直前とまったく同じ位置・向き・大きさで出る。
		// gibと同じ仕組みで、粒度が5個から30個に上がっただけ
		const float burst = DebugParams::Instance().Float(U8("破片/破砕の勢い"), 6.0f,  0.0f, 40.0f);
		const float lift  = DebugParams::Instance().Float(U8("破片/破砕の浮き"), 2.0f,  0.0f, 20.0f);
		const float spin  = DebugParams::Instance().Float(U8("破片/破砕の回転"), 3.0f,  0.0f, 20.0f);

		// 体の中心。ここから見て外向きに散らす
		Math::Vector3 bodyCenter;
		if (!GetBoneWorldPos(kHipsBoneName, bodyCenter)) { bodyCenter = GetPos(); }

		for (const FragmentDef& frag : m_fragments)
		{
			if (frag.modelId < 0) { continue; }

			const KdModelWork::Node* pNode = m_modelWork.FindWorkNode(frag.bone);
			if (!pNode) { continue; }

			const Math::Matrix boneWorld = GetBoneWorldMatrix(*pNode);

			// 【骨の位置で代用しない】同じ骨の破片が全部同じ向きへ飛んで束になる。
			//   破片ごとの中心をワールドへ持っていって、そこから外向きを決める
			const Math::Vector3 fragWorld =
				Math::Vector3::Transform(spDebris->GetModelCenter(frag.modelId), boneWorld);

			Math::Vector3 outward = fragWorld - bodyCenter;
			if (!MathAPI::TryNormalize(outward)) { outward = Math::Vector3::Up; }

			const Math::Vector3 velocity = outward * burst + Math::Vector3::Up * lift;

			// 外向きと上向きの外積＝「外へ倒れ込む」向きの回転(乱数を使わずに自然に見せる)
			const Math::Vector3 angularVelocity = outward.Cross(Math::Vector3::Up) * spin;

			spDebris->SpawnPiece(frag.modelId, boneWorld, velocity, angularVelocity);
		}
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

	// 破片のモデルと凸包を先に用意しておく(壊れた瞬間に払わないため)。
	// シーン構築が済むまでDebrisSystemが見つからないので、Init()ではなくここで行う
	PreloadDebrisModels();

	// 【確認用】F4で即死させる。倒れ方と全身破砕を何度も見比べるための入口。
	// 通常の経路(ロックオン→突撃→命中)は手順が長く、見た目の詰めに向かないため
	if (KdInputManager::Instance().IsPress("KillEnemy"))
	{
		ApplyBodyDamage(m_hp);
	}

	const float dt = Application::Instance().GetDeltaTime();

	// 倒れている間は追従も攻撃もしない。向き直りと破砕の管理だけ行う。
	// ※ 対象を見失っていても破砕までは進めたいので、targetの取得より【前】に置く
	// ※ 凍結(下)より【前】に置く。「動きを止める」はAIと移動を止めるための旗であって、
	//   倒れ切る・砕けるまで進まなくなるのは意図ではない(F4で確認するときに詰まる)
	if (m_state == State::Down)
	{
		UpdateDown(dt);
		return;
	}

	// 【確認用】動いていると関節の球を見比べられないので、その場に固定できるようにする。
	// AIと移動だけを止め、接地(PostUpdateのGroundCheck)は生かして立たせたままにする。
	// ※ アニメはSelectAnimationSpeedが0を返して凍る。呼ぶのをやめてはいけない → そちらのコメント
	if (IsFrozenForDebug()) { return; }

	std::shared_ptr<KdGameObject> spTarget = m_wpTarget.lock();
	if (!spTarget) { return; }

	Math::Vector3 pos       = GetPos();
	Math::Vector3 targetPos = spTarget->GetPos();

	// 対象への水平方向と距離
	Math::Vector3 toTarget = MathAPI::FlattenY(targetPos - pos);
	float distXZ = toTarget.Length();
	Math::Vector3 dirToTarget = MathAPI::GetSafeNormal(toTarget, Math::Vector3::Backward);

	// 対象の方を向く。
	// 🔴 m_modelForwardIsMinusZ を必ず渡すこと。渡さないと既定の「正面＝+Z」で
	//   計算され、モデルがずっと逆を向く(2026/07/31にこれで2回外した)
	//
	// 🔴 振り下ろしの間は【向きを固定する】。ここで追尾させると、横へ回避しても
	//   敵が向き直って必ず当たる＝回避が成立しない。回避のiフレームは反撃の唯一の
	//   入口なので、避けられない攻撃を作ると反撃システムごと死ぬ。
	//   振りかぶり(Windup)の間だけは遅く向き直れる＝「狙いを付けている」ように見せる
	// 🔴【旋回速度は歩く速さから決める】独立した値にすると必ず足が滑る。
	//   歩きながら向きを変えるとき、体は半径 r = 速さ ÷ 角速度 の弧を描く。
	//   この r が体の幅より小さいと「その場で回っている」ことになり、
	//   前へ歩かせても足が追いつかない。
	//
	//   実測でそうなっていた：歩く5.5m/s ÷ 180度/秒 = 半径【1.75m】。
	//   身長25m・足幅10m近いゴーレムには小さすぎた（2026-08-05に実機で「滑る」と指摘）。
	//
	//   なので角速度のほうを 速さ÷半径 で求める。大きい敵ほど自然に鈍くなり、
	//   「ワイヤーで背後を取る」ことに意味が出る
	const float turnRadius =
		DebugParams::Instance().Float(U8("敵/旋回半径"), 15.0f, 1.0f, 60.0f);

	const float turnCapDeg =
		DebugParams::Instance().Float(U8("敵/旋回速度の上限"), 60.0f, 0.0f, 720.0f);

	float turnSpeedDeg = DirectX::XMConvertToDegrees(GetMoveSpeed() / std::max(turnRadius, 0.01f));
	turnSpeedDeg = std::min(turnSpeedDeg, turnCapDeg);

	if (m_state == State::Windup)
	{
		turnSpeedDeg *= DebugParams::Instance().Float(U8("敵/攻撃_振りかぶり中の旋回率"), 0.35f, 0.0f, 1.0f);
	}
	else if (m_state == State::Strike || m_state == State::Recover)
	{
		turnSpeedDeg = 0.0f;
	}

	DebugWatch::Instance().Watch(U8("敵/旋回_実効(度毎秒)"), turnSpeedDeg);

	if (turnSpeedDeg > 0.0f)
	{
		Math::Vector3 rot = GetRot();
		rot.y = MathAPI::RotateToDirection(rot.y, dirToTarget, turnSpeedDeg * dt, m_modelForwardIsMinusZ);
		SetRot(rot);
	}

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

	if (m_attackCooldown > 0.0f)
	{
		m_attackCooldown -= dt;
	}

	// --- 怯んでいる間は動かず、攻撃も始めない ---
	if (m_hitReactTimer > 0.0f)
	{
		m_hitReactTimer -= dt;
		return;
	}

	// --- 攻撃中は移動しない(突進ではないため) ---
	switch (m_state)
	{
	case State::Windup:
		m_stateTimer -= dt;
		if (m_stateTimer <= 0.0f)
		{
			EnterStrike();
		}
		return;

	case State::Strike:
		m_stateTimer -= dt;
		ResolveAttackHit(spTarget);
		if (m_stateTimer <= 0.0f)
		{
			EnterRecover();
		}
		return;

	case State::Recover:
		m_stateTimer -= dt;
		if (m_stateTimer <= 0.0f)
		{
			m_state = State::Chase;
		}
		return;

	default:
		break;
	}

	// --- 正面からどれだけズレているか ---
	const float yawToTarget = MathAPI::DirToYawDeg(dirToTarget, m_modelForwardIsMinusZ);
	const float offAngle    = std::abs(MathAPI::DeltaAngleDeg(GetRot().y, yawToTarget));

	// 正面とみなす角度。これより外なら「まだこちらを向いていない」
	const float facingDeg =
		DebugParams::Instance().Float(U8("敵/正面とみなす角度"), 25.0f, 5.0f, 90.0f);

	// 自分がいま向いている方向。
	// 🔴 進む向きは【必ずこれ】。対象の方向へ直接進めると、横を向いたまま真横へ動く
	//   ＝カニ歩きになり、足が思い切り滑る(2026-08-05に実機で指摘された滑りの半分はこれ)
	const Math::Vector3 forward = MathAPI::YawDegToDir(GetRot().y, m_modelForwardIsMinusZ);

	// --- 追う ---
	if (distXZ > stopDist)
	{
		m_currentMoveSpeed = GetMoveSpeed();
		pos += forward * m_currentMoveSpeed * dt;
		SetPos(pos);
		return;
	}

	// --- 🔴 その場で回らず、歩いて回り込む ---
	// 【なぜ】旋回だけで向きを変えると足が動かないので、ターンテーブルに乗って回って
	//   いるように見える(待機アニメを入れたことで目立つようになった)。
	//   前へ歩きながら旋回すると軌跡が弧を描き、足も動くので「重い体を踏み替えて
	//   向き直っている」ように見える。大型の敵らしさにもなる。
	//
	// 【背後を取られることは設計上の正解】25mのゴーレムが素早く振り向けないからこそ、
	//   ワイヤーで回り込む機動力に意味が出る。ここを速くすると立体機動の価値が薄れる
	if (offAngle > facingDeg)
	{
		const float arcRate =
			DebugParams::Instance().Float(U8("敵/回り込みの歩く速さの割合"), 0.55f, 0.0f, 1.0f);

		m_currentMoveSpeed = GetMoveSpeed() * arcRate;
		pos += forward * m_currentMoveSpeed * dt;
		SetPos(pos);
		return;
	}

	// --- 間合いに入っていて、次の攻撃が撃てるなら振りかぶる ---
	// 【間合いはstopDistより少し広く取る】ぴったりだと、プレイヤーが少し下がっただけで
	//   攻撃に入れず、寄っては止まるだけの置物になる
	const float attackRange = stopDist
		+ DebugParams::Instance().Float(U8("敵/攻撃_届く余裕"), 3.0f, 0.0f, 20.0f);

	if (m_attackCooldown <= 0.0f && distXZ <= attackRange)
	{
		EnterWindup();
	}
}

float Enemy::GetMoveSpeed() const
{
	return DebugParams::Instance().Float(U8("敵/移動速度"), 1.5f, 0.0f, 40.0f);
}

//======================================================================
//  攻撃（腕の振り下ろし）
//
//  Chase → Windup(振りかぶる) → Strike(振り下ろす) → Recover(硬直) → Chase
//  突進ではないので、この間【敵は移動しない】。25mのゴーレムが突っ込むのは
//  成立しなかったため(2026-08-04に撤去済み)
//======================================================================

void Enemy::EnterWindup()
{
	m_state      = State::Windup;

	// 【既定値はアニメの実測から決めた】slamR/slamL(1.25秒)の内訳：
	//   0.00〜0.67 振りかぶり / 0.71〜0.96 振り下ろし(0.83で最速) / 0.96〜1.25 戻り
	m_stateTimer = DebugParams::Instance().Float(U8("敵/攻撃_予備動作の秒数"), 0.70f, 0.1f, 3.0f);

	// 左右を交互に振る(素材が両方あるので、同じ動きの繰り返しに見えにくい)
	m_swingRight = !m_swingRight;

	// この振りぶんの当たりを解禁する
	m_hitDoneThisSwing = false;
	m_hasPrevSpheres   = false;
}

void Enemy::EnterStrike()
{
	m_state      = State::Strike;
	m_stateTimer = DebugParams::Instance().Float(U8("敵/攻撃_当たる秒数"), 0.30f, 0.05f, 2.0f);
}

void Enemy::EnterRecover()
{
	m_state      = State::Recover;
	m_stateTimer = DebugParams::Instance().Float(U8("敵/攻撃_硬直の秒数"), 0.55f, 0.0f, 3.0f);

	// 次に殴れるまでの間。硬直とは別に持つ(硬直＝隙、クールダウン＝攻撃の頻度)
	m_attackCooldown = DebugParams::Instance().Float(U8("敵/攻撃_間隔の秒数"), 1.5f, 0.0f, 10.0f);
}

void Enemy::BuildAttackSpheres(std::vector<std::pair<Math::Vector3, float>>& _out) const
{
	_out.clear();

	// 半径は【モデル座標】で持ち、ワールドへ出すときに拡大率を掛ける。
	// 関節の球と同じ流儀(中心側で掛けると足元補正と同じ二重掛け事故になる)
	const float radius =
		DebugParams::Instance().Float(U8("敵/攻撃_判定の半径"), 0.12f, 0.01f, 1.0f) * GetScale().y;

	// 振っている側の腕に付ける。アニメと判定が左右で食い違うと、
	// 「当たっていないのに食らう」という一番たちの悪い状態になる
	const char* const* bones = m_swingRight ? kAttackArmBonesRight : kAttackArmBonesLeft;

	Math::Vector3 prev{};
	bool hasPrev = false;

	for (int i = 0; i < kAttackArmBoneCount; ++i)
	{
		Math::Vector3 p{};
		if (!GetBoneWorldPos(bones[i], p)) { continue; }

		// 骨と骨の間を埋める(隙間があると腕の途中が素通りする)
		if (hasPrev)
		{
			for (int k = 1; k <= kAttackSphereBetween; ++k)
			{
				const float t = static_cast<float>(k) / static_cast<float>(kAttackSphereBetween + 1);
				_out.emplace_back(Math::Vector3::Lerp(prev, p, t), radius);
			}
		}

		_out.emplace_back(p, radius);
		prev    = p;
		hasPrev = true;
	}
}

bool Enemy::ResolveAttackHit(const std::shared_ptr<KdGameObject>& _target)
{
	if (m_hitDoneThisSwing) { return false; }
	if (!_target)           { return false; }

	std::vector<std::pair<Math::Vector3, float>> spheres;
	BuildAttackSpheres(spheres);
	if (spheres.empty()) { return false; }

	// --- プレイヤーを包む球 ---
	// 足元が原点なので、胴のあたりへ持ち上げてから当てる
	const float bodyHeight = DebugParams::Instance().Float(U8("敵/攻撃_プレイヤーの高さ"), 1.0f, 0.0f, 3.0f);
	const float bodyRadius = DebugParams::Instance().Float(U8("敵/攻撃_プレイヤーの半径"), 0.6f, 0.1f, 3.0f);
	const Math::Vector3 targetCenter = _target->GetPos() + Math::Vector3::Up * bodyHeight;

	// --- 前フレームからの掃引 ---
	// 🔴【実測で必要と確定した】振り下ろしの最速時、手は1フレームで【9.48m】動く。
	//   判定球(1.58m)＋プレイヤー(0.6m)＝2.18m しか無いので、その場の位置だけを見ると
	//   間にいたプレイヤーを飛び越す。前フレームとの間に球を刻んで埋める
	//   (CharaBase::ResolveBumpSweep が壁のすり抜けで使っているのと同じ手)。
	//   刻む数は移動量÷半径で決める＝速いときだけ細かくなり、遅いときは1回で済む
	int steps = 1;
	if (m_hasPrevSpheres && m_prevSpheres.size() == spheres.size())
	{
		float maxTravel = 0.0f;
		for (size_t i = 0; i < spheres.size(); ++i)
		{
			maxTravel = std::max(maxTravel, (spheres[i].first - m_prevSpheres[i].first).Length());
		}

		const float radius = spheres.front().second;
		if (radius > 1e-4f)
		{
			steps = std::clamp(static_cast<int>(std::ceil(maxTravel / radius)), 1, kMaxSweepSteps);
		}

		DebugWatch::Instance().Watch(U8("敵/攻撃_手の1フレーム移動量"), maxTravel);
		DebugWatch::Instance().Watch(U8("敵/攻撃_判定球の半径"),       radius);
		DebugWatch::Instance().Watch(U8("敵/攻撃_掃引の刻み数"),       steps);
	}

	for (int s = 0; s < steps; ++s)
	{
		// s=0 が前フレーム寄り、s=steps-1 が今フレーム。1回だけなら今の位置になる
		const float t = (steps <= 1) ? 1.0f
			: static_cast<float>(s + 1) / static_cast<float>(steps);

		for (size_t i = 0; i < spheres.size(); ++i)
		{
			const Math::Vector3 center = (steps <= 1 || !m_hasPrevSpheres)
				? spheres[i].first
				: Math::Vector3::Lerp(m_prevSpheres[i].first, spheres[i].first, t);

			const float reach = spheres[i].second + bodyRadius;
			if ((center - targetCenter).LengthSquared() > reach * reach) { continue; }

			// --- 命中 ---
		// この振りではもう当てない(球は数フレーム重なり続けるので、
		// これが無いと1回の振りで何度もノックバックする)
		m_hitDoneThisSwing = true;

		Player* pPlayer = dynamic_cast<Player*>(_target.get());
		if (!pPlayer) { return true; }

		if (pPlayer->IsInvincible())
		{
			// ジャスト回避成立 → Player側に反撃(スロー窓)を通知する。
			// 🔴 回避の無敵は反撃システムの【唯一の入口】なので、ここを消さないこと
			pPlayer->NotifyCounter();
		}
		else
		{
				// 無防備で被弾 → 外向きにノックバック(HPは無い＝勢いを崩すだけ)
				const Math::Vector3 knockDir = _target->GetPos() - GetPos();
				const float power = DebugParams::Instance().Float(U8("敵/ノックバック力"), 8.0f, 0.0f, 40.0f);
				pPlayer->ApplyKnockback(knockDir, power);
			}
			return true;
		}
	}

	// 次のフレームの掃引に使うので、今の位置を覚えておく
	m_prevSpheres    = spheres;
	m_hasPrevSpheres = true;
	return false;
}

void Enemy::DrawAttackDebug()
{
	if (!m_pDebugWire) { return; }
	if (m_state != State::Windup && m_state != State::Strike) { return; }

	// 予備動作は黄、当たる間は赤。攻撃の予告としてそのまま読める色にする
	const bool striking = (m_state == State::Strike);
	const Math::Color color = striking
		? Math::Color(1.0f, 0.25f, 0.15f, 1.0f)
		: Math::Color(1.0f, 0.85f, 0.2f, 1.0f);

	std::vector<std::pair<Math::Vector3, float>> spheres;
	BuildAttackSpheres(spheres);

	for (const std::pair<Math::Vector3, float>& sphere : spheres)
	{
		m_pDebugWire->AddDebugSphere(sphere.first, sphere.second, color);
	}
}

std::string Enemy::SelectAnimation() const
{
	// 倒れている間は「倒れる」を1回だけ流し、最終フレームのポーズで留まる
	// (SelectAnimationLoopがfalseを返すため)。これがそのままダウン中の待機になる
	if (m_state == State::Down) { return kFallAnimName; }

	// 攻撃の3状態はまとめて振り下ろし1本で流す。
	// 【なぜ状態ごとに分けないか】素材が「振りかぶり→振り下ろし→戻り」で1本に
	//   繋がっているため。状態ごとに切ると、切り替わりのたびに先頭へ巻き戻る
	if (m_state == State::Windup || m_state == State::Strike || m_state == State::Recover)
	{
		return m_swingRight ? kSlamRightAnimName : kSlamLeftAnimName;
	}

	// 被弾リアクション。攻撃中は怯まないので、ここへは Chase のときしか来ない
	if (m_hitReactTimer > 0.0f) { return kHitAnimName; }

	// 止まっているときは待機。
	// 【なぜ要るか】待機が無かった頃は歩行を遅回ししていたので、間合いで止まっても
	//   「その場で歩いている」ように見えていた(再生倍率をどう詰めても直らない)
	if (m_currentMoveSpeed <= 0.0f) { return kIdleAnimName; }

	return kWalkAnimName;
}

bool Enemy::SelectAnimationLoop() const
{
	// 1回きりで最後のポーズに留まるもの：倒れる／振り下ろし
	// (振り下ろしはループさせると、硬直中に2回目の振りが始まってしまう)
	if (m_currentAnimName == kFallAnimName)      { return false; }
	if (m_currentAnimName == kSlamRightAnimName) { return false; }
	if (m_currentAnimName == kSlamLeftAnimName)  { return false; }
	if (m_currentAnimName == kHitAnimName)       { return false; }

	return true;
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
	// 【既定はfalseに戻すこと】DebugFlagsには保存/読込が無いので、ここの既定値が
	// そのまま毎回の値になる。trueのままだと敵が起動のたびに止まった状態で始まり、
	// 倒れる→砕けるまで進まない(Update側でこの判定より後ろにあるため)。
	// 部位破壊の見比べで一時的にtrueにしていたぶんを戻した(2026-08-05)
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

	// 以下の「足を滑らせない倍率」は歩行クリップ前提の計算なので、
	// 歩行以外(倒れる／振り下ろす／待機)には意味が無い。等速で流す
	if (m_currentAnimName != kWalkAnimName) { return 1.0f; }

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

		// 攻撃の判定球(振りかぶり=黄 / 当たる=赤)。
		// 攻撃していない間は何も出ないので、開閉がそのまま目で見える
		DrawAttackDebug();

		// 体全体の接触判定(m_hitRadius)。身長25mだと半径6.25mの球になり、
		// 関節の球(1.7〜2m)を完全に飲み込んで狙い分けが見えなくなるので既定はOFF
		if (DebugFlags::Instance().Get(U8("敵/体の判定も出す"), false))
		{
			m_pDebugWire->AddDebugSphere(GetPos(), m_hitRadius, kRedColor);
		}
	}
}
