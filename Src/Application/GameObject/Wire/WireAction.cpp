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

void WireAction::UpdateSwing(CharaBase& _body, float _dt, float _reel)
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

	// たぐり寄せ/伸ばし(入力はキャラ側が決めて_reelで渡す)
	Update(pos, _body.m_velocity, _dt, _reel);

	// === 漕ぎ(ポンプ) ===
	// DebugFlags「ワイヤー/漕ぎ(ポンプ)」でON/OFF。
	//   OFF … 元の純粋な振り子(下りで加速→上りで減速。地面すれすれは速度が落ちる)
	//   ON  … 振り子に接線方向の加速を足す(ブランコを漕ぐイメージ)。上限速度まで勢いを
	//          維持できるので、Spider-Man風に地面すれすれでも速度が落ちにくくなる
	if (DebugFlags::Instance().Get(U8("ワイヤー/漕ぎ(ポンプ)"), false))
	{
		// 今の進行方向(水平)
		Math::Vector3 horiz(_body.m_velocity.x, 0.0f, _body.m_velocity.z);
		float sp = horiz.Length();
		if (sp > 0.0001f)
		{
			Math::Vector3 tdir = horiz / sp;

			// アンカーから外向き(半径方向)の水平成分を除き、接線方向だけに加速する。
			// (半径方向へ足しても距離拘束で打ち消されるだけ＝弧に沿ってのみ漕ぐ)
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

			float pumpMax = DebugParams::Instance().Float(U8("ワイヤー/ポンプ上限速度"), 20.0f, 0.0f, 60.0f);
			float pumpAcc = DebugParams::Instance().Float(U8("ワイヤー/ポンプ加速"),     15.0f, 0.0f, 100.0f);

			float add = pumpMax - sp;   // 上限速度まであとどれだけ足せるか(それ以上は加速しない)
			if (add > 0.0f && tdir.LengthSquared() > 0.0f)
			{
				float a = pumpAcc * _dt;
				if (a > add)
				{
					a = add;
				}
				_body.m_velocity.x += tdir.x * a;
				_body.m_velocity.z += tdir.z * a;
			}
		}
	}

	// 地面に潜らないよう押し上げる(ワイヤー中もすり抜け防止)。m_isGroundedもここで更新される
	_body.ResolveGround(pos);

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
	// ① _from から _dir 方向へ、長さ _maxLength のレイ(KdCollider::RayInfo)を作る
	//    当てたい種類は地形系。例: KdCollider::TypeGround | KdCollider::TypeBump
	KdCollider::RayInfo ray(KdCollider::TypeGround | KdCollider::TypeBump, _from, _dir, _maxLength);

	// ② SceneManager の全オブジェクトに Intersects(ray, &結果リスト) して、当たりを集める
	//    (CharaBase::GroundCheck が同じ書き方をしているので参考になる)
	std::list<KdCollider::CollisionResult> retRayList;

	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }
		obj->Intersects(ray, &retRayList);
	}

	// ③ 集めた当たりの中から「_from に一番近い交点」を選ぶ
	//    (Math::Vector3::Distance で距離を比べて、一番小さいものを採用)

	bool hit = false;
	float nearest = _maxLength;   // 「今のところ最短」を最大長で初期化
	for (auto& ret : retRayList)
	{
		float dist = Math::Vector3::Distance(_from, ret.m_hitPos);
		if (dist<nearest)
		{
			nearest = dist;
			m_anchor = ret.m_hitPos;
			hit = true;
		}
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

		_vel -= dir * DebugParams::Instance().Float(U8("ワイヤー/引き込み力"), 5, 0.1f, 10) * _dt;
	}


	// ※ 内側(dist <= m_length)は自由に動ける = 不等式拘束。だからワイヤーに向かって寄れる
}

const Math::Vector3& WireAction::GetAnchor() const
{
	// m_anchor を返すだけ
	return m_anchor;
}
