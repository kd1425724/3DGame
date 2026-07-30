#include "Enemy.h"

#include "../../../main.h"
#include "../../../API/MathAPI/MathAPI.h"
#include "../../../Scene/SceneManager.h"
#include "../../../Debug/DebugParams/DebugParams.h"
#include "../../../Debug/DebugDraw/DebugDraw.h"
#include "../Player/Player.h"   // 突進命中時にPlayerの無敵判定/反撃通知/ノックバックを呼ぶため

// 【部位破壊調査(一時)】DumpModelDiagnostics で一時ファイルへ書き出すため
#include <filesystem>
#include <fstream>

DebugDraw::Category Enemy::GetDebugCategory() const
{
	return DebugDraw::Category::Enemy;
}

void Enemy::DrawDebug()
{
	// KdGameObject::DrawDebug は s_showColliderDebug しか見ないので、
	// そのまま呼ぶと登録済みコライダー(緑の球)がカテゴリに関係なく出てしまう。
	// 「敵」がOFFのときは基底の間だけフラグを落とす
	DebugDraw::ScopedGate gate(DebugDraw::Category::Enemy);
	KdGameObject::DrawDebug();
}

void Enemy::Init()
{
	// 【2026/07/29】立方体(Block.gltf)のテスト実装から、リグ付きの戦闘メカへ差し替えた。
	// 部位破壊の題材として選んだモデル。骨が Arm_L / Minigun_L / Camera / Top_Leg_L … と
	// 最初から機械の部位で分かれており、部位ごとの当たり判定を骨に追従させやすい。
	// ライセンスは CC BY 4.0(作者クレジット必須) → THIRD_PARTY_LICENSES.txt
	SetAsset("Asset/Models/Character/W9231Mech/W9231Mech.gltf");

	// 実寸モデルなので等倍。原点は足元にある(実測 足元Z=0.018)
	SetScale(Math::Vector3::One);
	m_modelOriginIsFeet = true;

	// 高さの実測値。スキン変形を計算して求めた値(幅3.764 × 高さ4.368 × 奥行3.000)。
	// 【罠】このモデルの生の頂点座標は±19000という巨大な値で、スキン変形で初めて
	//   正しい位置に戻る。頂点座標をそのまま測ると桁が違う値が出るので必ず変形後を見ること。
	// GetBodyHalfHeight()経由で接地・天井・壁の判定が足元と頭の位置を出すのに使う
	m_bodyHeight = 4.37f;

	// 【未確認】正面の向きは実機で見て決めること。
	//   モデルごとに違い、Blenderの軸から推論して外した実績がある(→memory)。
	//   進行方向のちょうど逆を向いていたら false にする
	m_modelForwardIsMinusZ = true;

	// ※ 当たり判定(KdCollider)は登録しない。
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

// ===== 部位破壊調査(一時) =====
// メカが画面に出ない。glTF側はスキン変形後の位置・向き・大きさすべて正常と確認済みなので、
// エンジンから見てモデルがどう見えているかを実際に書き出して切り分ける。
// 原因が判明したら "部位破壊調査" で grep して削除する。
void Enemy::DumpModelDiagnostics() const
{
	static bool s_done = false;
	if (s_done) { return; }
	s_done = true;

	std::filesystem::path out = std::filesystem::temp_directory_path() / "mech_diag.txt";
	std::ofstream f(out);
	if (!f) { return; }

	f << "[部位破壊調査] エンジンから見たメカのモデル\n";
	f << "IsEnable        = " << (m_modelWork.IsEnable() ? "true" : "false") << "\n";

	const std::shared_ptr<KdModelData> data = m_modelWork.GetData();
	f << "GetData()       = " << (data ? "あり" : "null(=読み込み失敗)") << "\n";
	if (!data)
	{
		f << "→ モデルの読み込み自体が失敗している\n";
		return;
	}

	f << "全ノード数      = " << data->GetOriginalNodes().size() << "\n";
	f << "描画ノード数    = " << data->GetDrawMeshNodeIndices().size() << "\n";
	f << "当たりノード数  = " << data->GetCollisionMeshNodeIndices().size() << "\n";
	f << "ボーン数        = " << data->GetBoneNodeIndices().size() << "\n";
	f << "IsSkinMesh      = " << (data->IsSkinMesh() ? "true" : "false") << "\n";
	f << "マテリアル数    = " << data->GetMaterials().size() << "\n";

	// 描画ノードごとに、メッシュがあるか・三角形数・境界球を出す。
	// 境界球の中心と半径が桁違いなら、頂点データの解釈がずれている
	for (int idx : data->GetDrawMeshNodeIndices())
	{
		const KdModelData::Node& n = data->GetOriginalNodes()[idx];
		f << "  node[" << idx << "] name=" << n.m_name
		  << " mesh=" << (n.m_spMesh ? "あり" : "null")
		  << " isSkin=" << (n.m_isSkinMesh ? "true" : "false");
		if (n.m_spMesh)
		{
			const DirectX::BoundingSphere& bs = n.m_spMesh->GetBoundingSphere();
			f << " 面数=" << n.m_spMesh->GetFaces().size()
			  << " 境界球中心=(" << bs.Center.x << ", " << bs.Center.y << ", " << bs.Center.z << ")"
			  << " 半径=" << bs.Radius;
		}
		f << "\n";
	}

	// ボーンのワールド座標(部位判定の前提。体の付近に来ていないと使えない)
	for (const char* boneName : { "Root_01", "Camera_08", "Arm_L_02", "Top_Leg_L_09" })
	{
		const KdModelWork::Node* node = m_modelWork.FindNode(boneName);
		if (!node)
		{
			f << "  bone " << boneName << " = 見つからない\n";
			continue;
		}
		const Math::Vector3 t = node->m_worldTransform.Translation();
		f << "  bone " << boneName << " モデル空間=(" << t.x << ", " << t.y << ", " << t.z << ")\n";
	}

	f << "敵の位置        = (" << GetPos().x << ", " << GetPos().y << ", " << GetPos().z << ")\n";
	f << "m_bodyHeight    = " << m_bodyHeight << "\n";
}

void Enemy::OnHit(KdGameObject* /*_other*/)
{
	// 攻撃に当たったら消滅する
	m_isExpired = true;
}

void Enemy::Update()
{
	// 【部位破壊調査(一時)】初回だけモデルの状態を一時ファイルへ書き出す
	DumpModelDiagnostics();

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
		rot.y = MathAPI::RotateToDirection(rot.y, dirToTarget, turnSpeedDeg * dt);
		SetRot(rot);
	};

	switch (m_state)
	{
	case State::Chase:
	{
		// 攻撃開始距離まで近づいたら予備動作へ(その場で予告)
		float atkStart = DebugParams::Instance().Float(U8("敵/攻撃開始距離"), 2.5f, 0.5f, 20.0f);
		if (distXZ <= atkStart)
		{
			m_state = State::Windup;
			m_stateTimer = DebugParams::Instance().Float(U8("敵/予備動作時間"), 0.45f, 0.05f, 2.0f);
			break;
		}

		// 追従移動(対象へゆっくり近づく)
		float moveSpeed = DebugParams::Instance().Float(U8("敵/移動速度"), 1.5f, 0.0f, 20.0f);
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
		float lungeSpeed = DebugParams::Instance().Float(U8("敵/突進速度"), 14.0f, 1.0f, 40.0f);
		pos += m_lungeDir * lungeSpeed * dt;
		SetPos(pos);
		m_stateTimer -= dt;

		// 命中判定：対象に十分近ければ命中処理(反撃 or ノックバック)して硬直へ
		float hitDist = m_hitRadius + 0.5f;   // プレイヤー半径ぶん少し余裕を持たせる
		if (Math::Vector3::Distance(GetPos(), targetPos) <= hitDist)
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
	// 【2026/07/29】通常時は赤ではなく白(=色味を掛けない)にした。
	//   立方体だった頃は「他と見分けるため」赤くしていたが、実物のメカは
	//   暗い金属＋発光部で見た目が成立しているので、赤く染めると台無しになる。
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

void Enemy::EnterRecover()
{
	m_state = State::Recover;
	m_stateTimer = DebugParams::Instance().Float(U8("敵/硬直時間"), 0.7f, 0.0f, 3.0f);
}

void Enemy::PostUpdate()
{
	// デバッグ表示：接触判定(m_hitRadius)を可視化
	if (KdGameObject::s_showColliderDebug && DebugDraw::IsOn(DebugDraw::Category::Enemy))
	{
		if (!m_pDebugWire)
		{
			m_pDebugWire = std::make_unique<KdDebugWireFrame>();
		}
		m_pDebugWire->AddDebugSphere(GetPos(), m_hitRadius, kRedColor);
	}

	// ※ 以前は「対象に接触したら敵が消滅」する仮処理だったが、攻撃(突進)に一本化したため撤去。
	//    敵はプレイヤーの攻撃(OnHit)か反撃でのみ消滅する

	// 地面(KdCollider::TypeGround)に立つ
	GroundCheck();
}
