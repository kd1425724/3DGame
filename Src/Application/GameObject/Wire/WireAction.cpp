#include "WireAction.h"

#include "../../Scene/SceneManager.h"           // Shootのレイ判定で全オブジェクトを走査する
#include "../../Debug/DebugParams/DebugParams.h"// リール速度・引き込み力などの調整値
#include "../../Debug/DebugFlags/DebugFlags.h"  // 漕ぎ(ポンプ)のON/OFF
#include "../Chara/CharaBase.h"                 // スイングで動かすキャラ(速度・位置・当たり解決)
#include "../../Collision/CollisionGrid.h"      // IsWallBetween(手元〜アンカー間の遮蔽判定)

// 見た目の板ポリ(KdSquarePolygon)はPch経由で見える。unique_ptr(前方宣言)の生成/破棄を
// ここ(完全な型が見える.cpp)で行うため、ctor/dtorを定義する
WireAction::WireAction()
{
	// 白テクスチャを土台に、描画時に水色＋発光を乗せる。軸(ワイヤー方向)固定ビルボード。
	// テクスチャはKdAssetsのキャッシュから取り、板ポリに渡す
	std::shared_ptr<KdTexture> spTex = KdAssets::Instance().m_textures.GetData("Asset/Textures/System/WhiteNoise.png");
	m_upPoly = std::make_unique<KdSquarePolygon>(spTex);
	m_upPoly->Set2DObject(false);
	m_upPoly->SetBillboardMode(KdPolygon::BillboardMode::eAxis);
}

WireAction::~WireAction() = default;

void WireAction::UpdateSwing(CharaBase& _body, float _dt, const Math::Vector2& _moveInput)
{
	if (!m_isAttached) { return; }

	// 手元とアンカーの間に壁(塔など)が入ったら、線が壁を突き抜けるのでワイヤーを外す(貫通させない)。
	// ただし一瞬のかすりで外れないよう、一定時間(自動リリース猶予)遮蔽が続いた時だけ外す(デバウンス)。
	// 外した瞬間の速度はそのまま残るので、スイングの勢いで飛んでいける(フリング)
	Math::Vector3 hand = _body.GetPos() + Math::Vector3(0.0f, 1.0f, 0.0f);
	float occMargin = DebugParams::Instance().Float(U8("ワイヤー/遮蔽の余白"),     1.0f, 0.0f, 5.0f);
	if (CollisionGrid::IsWallBetween(hand, m_anchor, occMargin))
	{
		m_occludedTime += _dt;
	}
	else
	{
		m_occludedTime = 0.0f;
	}

	float releaseDelay = DebugParams::Instance().Float(U8("ワイヤー/自動リリース猶予"), 0.2f, 0.0f, 1.0f);
	if (m_occludedTime >= releaseDelay)
	{
		Release();
		return;
	}

	// 重力を3D速度に加える
	float gravity = DebugParams::Instance().Float(U8("キャラ/重力"), 20.0f, 0.0f, 100.0f);
	_body.m_velocity.y -= gravity * _dt;

	// 速度で進めてから、ワイヤーの距離拘束を解く
	Math::Vector3 pos = _body.GetPos();
	Math::Vector3 startPos = pos;   // 移動前の位置(スイープの始点)
	pos += _body.m_velocity * _dt;

	const bool winchMode = DebugFlags::Instance().Get(U8("ワイヤー/引き寄せモード"), true);

	// === 巻き取り(ウインチ) ── 立体機動の推進力の本体 ===
	// 【重要】ここは拘束を解く"前"に行う。ワイヤー長を縮めてから拘束を解くことで、
	// 「ピンと張ったまま巻き取られる」状態が作れる。
	//
	// ※ 2026/07/20 に作り直し。
	//   以前は「アンカー方向へ加速」で実装していたが、半径方向にだけ力を掛けると
	//   アンカーへ一直線に飛ぶだけで弧を描かず、回転しないので上にも飛ばなかった。
	//   張った紐を短くすると、接線方向の速度は保たれたまま半径だけ縮むので、
	//   角速度が上がって振り回されながら上へ抜ける(紐に繋いだ球を手繰る動き)。
	//   これが立体機動の「巻き取られて振られる」感触の正体。
	if (winchMode)
	{
		float reelSpeed = DebugParams::Instance().Float(U8("ワイヤー/巻き取り速度"), 14.0f, 0.0f, 60.0f);
		float minLen    = DebugParams::Instance().Float(U8("ワイヤー/最短の長さ"),   3.0f, 0.5f, 30.0f);

		m_length -= reelSpeed * _dt;
		if (m_length < minLen)
		{
			m_length = minLen;
		}
	}

	// 距離拘束を解く(球の外へは出られない。内側は自由＝紐であって棒ではない)。
	// ※ 手動リールは 2026/07/19 に廃止したので入力は 0。巻き取りは上の自動ウインチが担当
	Update(pos, _body.m_velocity, _dt, 0.0f);

	// === 補助的な半径方向の引き ===
	// DebugFlags「ワイヤー/引き寄せモード」で挙動が2種類に分かれる。
	//   ON (立体機動)   … 上の巻き取りでワイヤーが縮み、張ったまま振り回される
	//   OFF (Spider-Man)… 巻き取りなし。重力で落ちて振れる純粋な振り子
	if (winchMode)
	{
		Math::Vector3 toAnchor = m_anchor - pos;
		float distToAnchor = toAnchor.Length();
		if (distToAnchor > 0.001f)
		{
			toAnchor /= distToAnchor;

			float approach = _body.m_velocity.Dot(toAnchor);

			// 半径方向への加速。※ 既定を 20 → 0 にした(2026/07/20)。
			// 半径方向に力を掛けるとアンカーへ一直線に向かってしまい、弧を描かなくなる
			// (＝回転しないので上に飛ばない)。推進は上の「巻き取り」に任せるのが正しい。
			// 真下にぶら下がった時など、どうしても寄せたい場合用にパラメータは残す(0で無効)
			float reelAcc = DebugParams::Instance().Float(U8("ワイヤー/引き寄せ加速"),    0.0f, 0.0f, 120.0f);
			float reelMax = DebugParams::Instance().Float(U8("ワイヤー/引き寄せ上限速度"), 30.0f, 0.0f, 120.0f);
			if (reelAcc > 0.0f && approach < reelMax)
			{
				_body.m_velocity += toAnchor * (reelAcc * _dt);
			}
		}

		// ※ ここにあった自動離脱を 2026/07/20 に撤去した(ユーザー指示)。
		//    撤去したのは (A)アンカーの追い越しで離す ＋ それを「上を向いてから」に
		//    遅らせる待ち、(B)アンカーに近づきすぎたら離す(壁への激突防止の安全網)、の2つ。
		//    自動で外れる条件は「手元〜アンカーが壁で遮られた時」(UpdateSwing冒頭)だけになり、
		//    それ以外はプレイヤーが左クリックを離すまで繋がったままになる。
		//    → (B)を消したので、正面の壁に水平に刺すとアンカーまで引かれて激突する。
		//      切り離しは自分で行う前提。戻すならこのコミットをrevertする
	}

	// === 操舵と漕ぎ(進行方向の水平ベクトルを基準にする) ===
	// 進行方向(水平)と、それに直交する右向きを作る。
	// 半径方向(アンカーから外向き)の成分は距離拘束で打ち消されるだけなので、
	// 接線成分だけに力を入れる = 弧に沿ってのみ加速・旋回する
	Math::Vector3 horiz(_body.m_velocity.x, 0.0f, _body.m_velocity.z);
	float sp = horiz.Length();
	if (sp > 0.0001f)
	{
		Math::Vector3 tdir = horiz / sp;                       // 進行方向(水平)
		Math::Vector3 side = Math::Vector3::Up.Cross(tdir);    // 進行方向の右手側
		if (side.LengthSquared() > 0.0001f)
		{
			side.Normalize();
		}

		// アンカーから外向きの水平成分を落として、接線方向だけ残す
		Math::Vector3 radial = pos - m_anchor;
		radial.y = 0.0f;
		if (radial.LengthSquared() > 0.0001f)
		{
			radial.Normalize();
			tdir -= radial * tdir.Dot(radial);
			if (tdir.LengthSquared() > 0.0001f)
			{
				tdir.Normalize();
			}
			else
			{
				tdir = Math::Vector3::Zero;
			}
		}

		// --- 前方への漕ぎ(W/S) ---
		// ブランコを漕ぐイメージで接線方向に加速する。上限速度までしか足さないので
		// 押しっぱなしでも際限なく速くならない
		float pumpMax = DebugParams::Instance().Float(U8("ワイヤー/ポンプ上限速度"), 20.0f, 0.0f, 60.0f);
		float pumpAcc = DebugParams::Instance().Float(U8("ワイヤー/ポンプ加速"),     15.0f, 0.0f, 100.0f);
		float add = pumpMax - sp;
		if (_moveInput.y != 0.0f && add > 0.0f && tdir.LengthSquared() > 0.0f)
		{
			float a = pumpAcc * _moveInput.y * _dt;
			if (a > add)
			{
				a = add;
			}
			_body.m_velocity.x += tdir.x * a;
			_body.m_velocity.z += tdir.z * a;
		}

		// --- 操舵(A/D) ---
		// 進行方向の横へ加速して、振り子の面ごと向きを変える。
		// 曲がりたい方向へ velocity を寄せるので、次の弧が違う方向へ向く
		//
		// ※ 既定を 14 → 26 に引き上げた(2026/07/19)。
		//   引き寄せ加速に対して操舵が弱すぎると「ワイヤーに乗せられて建物へ運ばれる」感覚になる。
		//   立体機動が自由に感じるのは、ワイヤーが大まかな方向を決めるだけで
		//   細かい軌道は自分の推進力で作れるから。引きと操舵の比率が手触りを決める
		float steerAcc = DebugParams::Instance().Float(U8("ワイヤー/操舵加速"), 26.0f, 0.0f, 80.0f);
		if (_moveInput.x != 0.0f)
		{
			_body.m_velocity += side * (steerAcc * _moveInput.x * _dt);
		}
	}

	// ※ ここにあった「Space/Ctrl単独の上下噴射」は 2026/07/20 に廃止した(ユーザー指示)。
	//    上への推進は Player 側の加速(右クリック＋Space)へ一本化している。
	//    噴射の入口が複数あると同時押しで二重に効いてしまい、強さが読めなくなるため。
	//    調整値「ワイヤー/上下噴射加速」「ワイヤー/上昇の上限速度」も未使用になった

	// ※ ここにあった「弧の底で自動リリース＋離脱ブースト」も 2026/07/20 に撤去した(同上)。
	//    振り子(Spider-Man)モード専用＝`!winchMode`の時だけ効く経路だったので、
	//    既定の引き寄せモードでは元々動いていない。撤去に伴い調整値
	//    「ワイヤー/自動リリース最短時間」「ワイヤー/離脱ブースト前方」「〜上」も未使用になった

// 地面に潜らないよう押し上げる(ワイヤー中もすり抜け防止)。
	// ※ ワイヤー中は原則「着地しない」= 地面スレスレを飛べるようにする。
	//    着地扱いにすると地面に触れるたび止まって勢いが死に、低空を飛べないため。
	//    ただしアンカーを地面そのものに刺した場合は別で、そこへ引かれて降りていくのが
	//    正しい動きなので通常どおり着地させる
	bool allowLanding = m_anchorIsGround || !DebugFlags::Instance().Get(U8("ワイヤー/地面スレスレ飛行"), true);
	_body.ResolveGround(pos, allowLanding);

	// 上昇スイング中に頭上の天井へ潜り込むのを止める(高速上昇のトンネリングも掃引で拾う)
	_body.ResolveCeiling(startPos, pos);

	// スイングは高速なので、まず壁を飛び越えるトンネリングを止める(startPos→posを掃引)
	_body.ResolveBumpSweep(startPos, pos);

	// 塔(Block)にめり込まないよう水平に押し出す
	_body.ResolveBump(pos);

	_body.SetPos(pos);
}

void WireAction::Draw(const Math::Vector3& _from, const Math::Vector3& _to)
{
	if (!m_upPoly) { return; }

	// 軸(=ワイヤー方向)と長さ
	Math::Vector3 axis = _to - _from;
	float length = axis.Length();
	if (length < 0.001f) { return; }   // 長さ0は描けない
	axis /= length;

	// 板の寸法(幅=太さ / 高さ=ワイヤー長)。太さはDebugParamsで調整
	float thickness = DebugParams::Instance().Float(U8("ワイヤー/見た目の太さ"), 0.08f, 0.01f, 1.0f);
	m_upPoly->SetScale(Math::Vector2(thickness, length));

	// 軸(Y=ワイヤー方向)と中点だけ入れる。面をカメラへ向ける計算はeAxisビルボードでDrawPolygonが行う
	Math::Matrix world = Math::Matrix::Identity;
	world.Up(axis);
	world.Translation((_from + _to) * 0.5f);

	// 発光っぽい水色のワイヤーとして描く(DrawPolygonはCullNoneなので裏表は気にしなくてよい)
	Math::Color   col(0.4f, 0.85f, 1.0f, 1.0f);
	Math::Vector3 emissive(0.1f, 0.4f, 0.7f);
	KdShaderManager::Instance().m_StandardShader.DrawPolygon(*m_upPoly, world, col, emissive);
}


bool WireAction::Shoot(const Math::Vector3& _from, const Math::Vector3& _dir, float _maxLength)
{
	// 建物(TypeBump)と地面(TypeGround)を別々に撃って、どちらが手前だったかを覚える。
	// 「地面に刺したか」が分かると、ワイヤー中に着地させるかどうかを切り替えられる
	// (地面スレスレを飛ぶのが基本だが、地面そのものに刺した時は降りていくのが正しいため)
	auto castNearest = [&](UINT _type, Math::Vector3& _outPos) -> float
		{
			KdCollider::RayInfo ray(_type, _from, _dir, _maxLength);
			std::list<KdCollider::CollisionResult> results;
			for (auto& obj : SceneManager::Instance().GetObjList())
			{
				if (!obj) { continue; }
				obj->Intersects(ray, &results);
			}

			float nearest = _maxLength;
			bool found = false;
			for (auto& ret : results)
			{
				float dist = Math::Vector3::Distance(_from, ret.m_hitPos);
				if (dist < nearest)
				{
					nearest = dist;
					_outPos = ret.m_hitPos;
					found = true;
				}
			}
			return found ? nearest : -1.0f;   // 当たらなければ-1
		};

	Math::Vector3 bumpPos;
	Math::Vector3 groundPos;
	float bumpDist   = castNearest(KdCollider::TypeBump,   bumpPos);
	float groundDist = castNearest(KdCollider::TypeGround, groundPos);

	bool hit = false;
	if (bumpDist >= 0.0f && (groundDist < 0.0f || bumpDist <= groundDist))
	{
		m_anchor = bumpPos;
		m_anchorIsGround = false;
		hit = true;
	}
	else if (groundDist >= 0.0f)
	{
		m_anchor = groundPos;
		m_anchorIsGround = true;
		hit = true;
	}

	// ④ 命中したら:
	//      ・m_anchor に交点をセット
	//      ・m_length = _from と m_anchor の距離(＝撃った瞬間のワイヤー長)
	//      ・m_isAttached = true
	//      ・true を返す
	//    命中しなかったら false を返す
	if (hit)
	{
		m_length = Math::Vector3::Distance(_from,m_anchor);
		m_maxLength = m_length;   // 撃った瞬間の長さをリールアウトの上限にする
		m_isAttached = true;
		m_occludedTime = 0.0f;    // 遮蔽デバウンスをリセット
	}

	// TODO: 上の①〜④を実装する
	return hit; 
}

void WireAction::Release()
{
	m_isAttached = false;
	m_occludedTime = 0.0f;   // 遮蔽デバウンスをリセット
}

bool WireAction::IsAttached() const
{
	// m_isAttached を返すだけ
	return m_isAttached;
}

void WireAction::Update(Math::Vector3& _pos, Math::Vector3& _vel, float _dt, float _reelInput)
{
	// 繋がっていない(!m_isAttached)なら何もせず早期return
	if (!m_isAttached) { return; }

	// ① リール: _reelInput でワイヤー長 m_length を伸縮させる
	//    ・縮める量 = _reelInput * リール速度(DebugParams) * _dt を m_length から引く
	//    ・短くなりすぎないよう下限(例:0.5)でクランプ
	m_length -= _reelInput * DebugParams::Instance().Float(U8("ワイヤー/リール速度"), 5.0f, 0.1f, 10.0f) * _dt;
	if (m_length < 0.5f)
	{
		m_length = 0.5f;
	}   // 短すぎ防止
	if (m_length > m_maxLength)
	{
		m_length = m_maxLength;
	}   // 撃った時の長さより伸ばさない

	// ② アンカーから自分へのベクトル toPos = _pos - m_anchor と、その長さ dist を求める
	//    ・dist がほぼ0なら向きが定義できないので早期return
	//    ・dir = toPos / dist (アンカーから外向きの単位ベクトル)
	Math::Vector3 toPos = _pos - m_anchor;
	float dist = toPos.Length();
	if (dist < 0.00001f) { return; }
	Math::Vector3 dir = toPos / dist;

	// ③ 距離拘束: dist が m_length より大きい(=ワイヤーを伸ばしすぎ)ときだけ:
	//    (a) 位置を円周(球面)上へ戻す:  _pos = m_anchor + dir * m_length
	//    (b) 速度の半径方向成分だけ打ち消す:
	//         radialSpeed = _vel.Dot(dir)
	//         radialSpeed > 0 (外向き)のときだけ _vel から dir * radialSpeed を引く
	//         → 残った接線方向の速度で振り子のように振れる
	//    (c) (任意)張力で少し内向きに引く: _vel から dir * 引き込み力(DebugParams) * _dt を引く
	
	//現在位置から固定ポイントまでの距離がワイヤーの長さを超えていたら
	if (dist > m_length)
	{
		_pos = m_anchor + dir * m_length;

		float radialSpeed = _vel.Dot(dir);
		if (radialSpeed > 0)
		{
			_vel -= dir * radialSpeed;
		}

		// 【2026/07/19 既定値を 5 → 0 に変更】
		// 拘束中ずっとアンカー方向へ加速し続けていたため、「前や上へ行きたいのに
		// すぐ内側へ寄ってしまう」原因になっていた。実際のロープに内向きの力は無く、
		// 張力は「外へ出るのを止める」だけでよい。
		// 意図的に内へ寄せたい時のためにパラメータ自体は残す(0で無効)
		float pullIn = DebugParams::Instance().Float(U8("ワイヤー/引き込み力"), 0.0f, 0.0f, 10.0f);
		if (pullIn > 0.0f)
		{
			_vel -= dir * pullIn * _dt;
		}
	}


	// ※ 内側(dist <= m_length)は自由に動ける = 不等式拘束。だからワイヤーに向かって寄れる
}

const Math::Vector3& WireAction::GetAnchor() const
{
	// m_anchor を返すだけ
	return m_anchor;
}
