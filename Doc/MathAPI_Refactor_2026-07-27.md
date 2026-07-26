# MathAPI集約リファクタ 変更一覧（2026-07-27）

`Src/Application/` 全88ファイルを精査し、重複していた数学計算を `Src/Application/API/MathAPI/` へ集約した記録。
**関数名は実在するエンジンの公式名に合わせている**（出典は `MathAPI.h` の各関数コメント）。

- コミット: `ec9d9c9`(段階1) → `df36178`(2) → `d69aedd`(3) → `b75a063`(4) → `9361874`(5) → `59c6dce`(しきい値統一)
- Debug / Release ともビルド確認済み・`Src/Application` 配下の警告ゼロ
- **実機での動作確認はまだ**

---

## 1. 追加した関数

| 関数 | 公式名の出典 | 式が完全一致 |
|---|---|---|
| `TryNormalize` | Unreal `FVector::Normalize(Tolerance)` ※`Vector3::Normalize()`と紛らわしいので`Try`を付けた | ✅ |
| `GetSafeNormal` | Unreal `FVector::GetSafeNormal` ※失敗時の戻り値を選べるようにした | ✅ |
| `ToDirectionAndLength` | Unreal `FVector::ToDirectionAndLength` | ✅ |
| `GetSafeNormalXZ` | Unreal `FVector::GetSafeNormal2D` ※あちらはZ-upなので`XZ`に改名 | ✅ |
| `FlattenY` | 独自（公式名なし） | — |
| `InterpTo` (float / Vector2 / Vector3) | Unreal `FMath::FInterpTo` / `VInterpTo` | ✅ |
| `ClampAngleDeg` | Unreal `FRotator::ClampAxis` | ✅ |
| `DeltaAngleDeg` | Unity `Mathf.DeltaAngle` | ✅ |
| `LerpAngleDeg` | Unity `Mathf.LerpAngle` | ✅ |
| `MoveTowardsAngleDeg` | Unity `Mathf.MoveTowardsAngle` | ✅ |
| `InterpAngleTo` | 独自（`LerpAngleDeg` + dtクランプ） | — |
| `DirToYawDeg` | 独自（公式名なし） | — |
| `ProjectOnPlane` | Unity `Vector3.ProjectOnPlane`（数学用語では「ベクトル棄却」） | ✅ |
| `ClipVelocity` | Quake / Source `PM_ClipVelocity` | ✅ |
| `ClampMagnitude` | Unity `Vector3.ClampMagnitude` | ✅ |
| `FromToRotation` | Unity `Quaternion.FromToRotation` ※上限と重みの引数を足した | ⚠️ |
| `kSmallNumber` | Unreal `KINDA_SMALL_NUMBER`（同値 1e-4） | ✅ |

---

## 2. 元のコード → 新コード

### 2-1. 安全な正規化（29箇所）

| ファイル | 元のコード | 新コード |
|---|---|---|
| `CharaBase.cpp` `UpdateArmAim` | `Math::Vector3 axis = a.Cross(dL);`<br>`if (axis.LengthSquared() < 0.0001f) { break; }`<br>`axis.Normalize();` | `if (!MathAPI::TryNormalize(axis)) { break; }`<br>※後に `FromToRotation` へ吸収（2-5参照） |
| `CharaBase.cpp` `UpdateFacing` | `dir.y = 0.0f;`<br>`if (dir.LengthSquared() < 0.0001f) { return; }`<br>`dir.Normalize();` | `dir = MathAPI::FlattenY(dir);`<br>`if (!MathAPI::TryNormalize(dir)) { return; }` |
| `CharaBase.cpp` `ResolveBump` | `push.y = 0.0f;`<br>`if (push.LengthSquared() > 0.0f)`<br>`{ Math::Vector3 n = push; n.Normalize();` | `push = MathAPI::FlattenY(push);`<br>`Math::Vector3 n = push;`<br>`if (MathAPI::TryNormalize(n))` |
| `CharaBase.cpp` `ResolveBumpSweep` | `Math::Vector3 delta(pos.x - fromPos.x, 0.0f, pos.z - fromPos.z);` | `Math::Vector3 delta = MathAPI::FlattenY(pos - fromPos);` |
| `Enemy.cpp` | `toTarget.y = 0.0f;`<br>`float distXZ = toTarget.Length();`<br>`Math::Vector3 dirToTarget = (distXZ > 0.0001f) ? (toTarget / distXZ) : Math::Vector3::Backward;` | `Math::Vector3 toTarget = MathAPI::FlattenY(targetPos - pos);`<br>`float distXZ = toTarget.Length();`<br>`Math::Vector3 dirToTarget = MathAPI::GetSafeNormal(toTarget, Math::Vector3::Backward);` |
| `Targeting.cpp` ×2 | `if (camFwd.LengthSquared() < 0.0001f) { return; }`<br>`camFwd.Normalize();` | `if (!MathAPI::TryNormalize(camFwd)) { return; }` |
| `WallAction.cpp` `SpawnFx` | `runDir = Math::Vector3(_body.m_velocity.x, 0.0f, _body.m_velocity.z);`<br>`if (runDir.LengthSquared() < 0.0001f) { return; }`<br>`runDir.Normalize();` | `runDir = MathAPI::FlattenY(_body.m_velocity);`<br>`if (!MathAPI::TryNormalize(runDir)) { return; }` |
| `WireAction.cpp` `Swing` | `Math::Vector3 horiz(...x, 0.0f, ...z);`<br>`float sp = horiz.Length();`<br>`if (sp > 0.0001f)`<br>`{ Math::Vector3 tdir = horiz / sp;` | `Math::Vector3 horiz = MathAPI::FlattenY(_body.m_velocity);`<br>`Math::Vector3 tdir; float sp = 0.0f;`<br>`if (MathAPI::ToDirectionAndLength(horiz, tdir, sp))` |
| `WireAction.cpp` 同関数 | `Math::Vector3 radial = pos - m_anchor;`<br>`radial.y = 0.0f;`<br>`if (radial.LengthSquared() > 0.0001f)`<br>`{ radial.Normalize();` | `Math::Vector3 radial = MathAPI::FlattenY(pos - m_anchor);`<br>`if (MathAPI::TryNormalize(radial))` |
| `WireAction.cpp` `ApplyConstraint` | `Math::Vector3 toPos = _pos - m_anchor;`<br>`float dist = toPos.Length();`<br>`if (dist < 0.00001f) { return; }`<br>`Math::Vector3 dir = toPos / dist;` | `Math::Vector3 dir; float dist = 0.0f;`<br>`if (!MathAPI::ToDirectionAndLength(_pos - m_anchor, dir, dist)) { return; }` |
| `Player.cpp` `Update`(ワイヤー中の噴射) | `if (horiz.LengthSquared() > 0.0001f)`<br>`{ horiz.Normalize(); SpawnBoostFx(...); }` | `if (MathAPI::TryNormalize(horiz))`<br>`{ SpawnBoostFx(...); }` |
| `Player.cpp` ワイヤー発射 | `if (dir.LengthSquared() > 0.0001f) { dir.Normalize(); }`<br>`else { dir = Math::Vector3::Backward; }` | `dir = MathAPI::GetSafeNormal(dir, Math::Vector3::Backward);` |
| `Player.cpp` `GetWishDir` | `if (wishDir.LengthSquared() <= 0.0f) { return Math::Vector3::Zero; }`<br>`wishDir.Normalize();` | `if (!MathAPI::TryNormalize(wishDir)) { return Math::Vector3::Zero; }` |
| `Player.cpp` `GetAccelDir` ×3 | `Math::Vector3 horiz(m_velocity.x, 0.0f, m_velocity.z);`<br>`if (horiz.LengthSquared() > 0.0001f) { horiz.Normalize(); dir = horiz; }`<br>`fwd.y = 0.0f;`<br>`if (fwd.LengthSquared() > 0.0001f) { fwd.Normalize(); dir = fwd; }`<br>`dir.y += upRate;`<br>`if (dir.LengthSquared() > 0.0001f) { dir.Normalize(); }` | `Math::Vector3 horiz = MathAPI::FlattenY(m_velocity);`<br>`if (MathAPI::TryNormalize(horiz)) { dir = horiz; }`<br>`Math::Vector3 fwd = MathAPI::FlattenY(TransformNormal(...));`<br>`if (MathAPI::TryNormalize(fwd)) { dir = fwd; }`<br>`dir.y += upRate;`<br>`MathAPI::TryNormalize(dir);` |
| `Player.cpp` 回避ステップの向き | `dir.y = 0.0f;`<br>`if (dir.LengthSquared() < 0.0001f) { dir = Math::Vector3::Backward; }`<br>`dir.Normalize();` | `dir = MathAPI::GetSafeNormalXZ(dir, Math::Vector3::Backward);` |
| `Player.cpp` `ApplyKnockback` | `Math::Vector3 dir = _dir;`<br>`dir.y = 0.0f;`<br>`if (dir.LengthSquared() > 0.0001f) { dir.Normalize(); }`<br>`else { dir = Math::Vector3::Backward; }` | `Math::Vector3 dir = MathAPI::GetSafeNormalXZ(_dir, Math::Vector3::Backward);` |
| `EditorCamera.cpp` | `if (move.LengthSquared() > 0.0f)`<br>`{ move.Normalize();` | `if (MathAPI::TryNormalize(move))` |
| `TPSCamera.cpp` ロックオン | `Math::Vector3 dir = targetPos - GetPos();`<br>`dir.Normalize();` | `Math::Vector3 dir = MathAPI::GetSafeNormal(targetPos - GetPos());` |

**ゼロ判定のしきい値**が `0.0001f` / `0.00001f` / `0.0f` と混在していたので `kSmallNumber`(=`0.0001f`) に統一した。
正規化を伴わない「向きがゼロでないか」の判定3箇所（`Player::UpdateAccel`×2、`WallAction::Update`）も定数を使う形にした。

### 2-2. 目標値への追従（7箇所）

| ファイル | 元のコード | 新コード |
|---|---|---|
| `TPSCamera.cpp` 注視点 | `float followT = std::clamp(followK * dt, 0.0f, 1.0f);`<br>`m_smoothFollowPos = Math::Vector3::Lerp(m_smoothFollowPos, targetPos, followT);` | `m_smoothFollowPos = MathAPI::InterpTo(m_smoothFollowPos, targetPos, dt, followK);` |
| `TPSCamera.cpp` カメラ引き | `m_smoothPullback += (pullTarget - m_smoothPullback) * followT;` | `m_smoothPullback = MathAPI::InterpTo(m_smoothPullback, pullTarget, dt, followK);` |
| `TPSCamera.cpp` FOV | `m_smoothFov += (fovTarget - m_smoothFov) * followT;` | `m_smoothFov = MathAPI::InterpTo(m_smoothFov, fovTarget, dt, followK);` |
| `CharaBase.cpp` 体の傾き | `float t = std::clamp(k * _deltaTime, 0.0f, 1.0f);`<br>`m_tilt = Math::Vector2::Lerp(m_tilt, target, t);` | `m_tilt = MathAPI::InterpTo(m_tilt, target, _deltaTime, k);` |
| `CharaBase.cpp` 腕の重み | `float t = std::clamp(k * _deltaTime, 0.0f, 1.0f);`<br>`m_armAimWeight = std::lerp(m_armAimWeight, wantAim ? 1.0f : 0.0f, t);` | `m_armAimWeight = MathAPI::InterpTo(m_armAimWeight, wantAim ? 1.0f : 0.0f, _deltaTime, k);` |
| `FocusPostFx.cpp` | `float t = follow * dt;`<br>`if (t > 1.0f) { t = 1.0f; }`<br>`m_blend += (target - m_blend) * t;` | `m_blend = MathAPI::InterpTo(m_blend, target, dt, follow);` |

`TPSCamera` の中間変数 `followT` は不要になったので削除（3箇所とも同じ `followK` を使う）。

### 2-3. 角度（4ブロック）

| ファイル | 元のコード | 新コード |
|---|---|---|
| `TPSCamera.cpp` | `float rotT = std::clamp(rotK * dt, 0.0f, 1.0f);`<br>`auto lerpAngle = [](float cur, float tgt, float t) {`<br>`  float d = tgt - cur;`<br>`  while (d > 180.0f) { d -= 360.0f; }`<br>`  while (d < -180.0f) { d += 360.0f; }`<br>`  return cur + d * t; };`<br>`m_smoothDegAng.x = lerpAngle(m_smoothDegAng.x, m_DegAng.x, rotT);`<br>`（.y .z も同様）` | `m_smoothDegAng.x = MathAPI::InterpAngleTo(m_smoothDegAng.x, m_DegAng.x, dt, rotK);`<br>`m_smoothDegAng.y = MathAPI::InterpAngleTo(m_smoothDegAng.y, m_DegAng.y, dt, rotK);`<br>`m_smoothDegAng.z = MathAPI::InterpAngleTo(m_smoothDegAng.z, m_DegAng.z, dt, rotK);` |
| `TPSCamera.cpp` ロックオン角 | `m_DegAng.y = DirectX::XMConvertToDegrees(atan2f(dir.x, dir.z));` | `m_DegAng.y = MathAPI::DirToYawDeg(dir);` |
| `CharaBase.cpp` `UpdateFacing` | `float s = m_modelForwardIsMinusZ ? -1.0f : 1.0f;`<br>`float targetDeg = XMConvertToDegrees(std::atan2(s * dir.x, s * dir.z));`<br>`float diff = targetDeg - rot.y;`<br>`while (diff > 180.0f) { diff -= 360.0f; }`<br>`while (diff < -180.0f) { diff += 360.0f; }`<br>`rot.y += std::clamp(diff, -maxStep, maxStep);`<br>`if (rot.y >= 360.0f) { rot.y -= 360.0f; }`<br>`if (rot.y < 0.0f) { rot.y += 360.0f; }` | `float targetDeg = MathAPI::DirToYawDeg(dir, m_modelForwardIsMinusZ);`<br>`rot.y = MathAPI::ClampAngleDeg(`<br>`    MathAPI::MoveTowardsAngleDeg(rot.y, targetDeg, maxStep));` |

### 2-4. 面に対する分解・長さの上限（7箇所）

| ファイル | 元のコード | 新コード |
|---|---|---|
| `WallAction.cpp` 壁沿い移動 | `Math::Vector3 v = _body.m_velocity;`<br>`v -= m_wallNormal * v.Dot(m_wallNormal);` | `Math::Vector3 v = MathAPI::ProjectOnPlane(_body.m_velocity, m_wallNormal);` |
| `WallAction.cpp` `CanStart` | `Math::Vector3 tangent = hv - n * hv.Dot(n);` | `Math::Vector3 tangent = MathAPI::ProjectOnPlane(hv, n);` |
| `WallAction.cpp` `WallJump` | `Math::Vector3 tangent = hv - m_wallNormal * hv.Dot(m_wallNormal);` | `Math::Vector3 tangent = MathAPI::ProjectOnPlane(hv, m_wallNormal);` |
| `WireAction.cpp` 接線抽出 | `tdir -= radial * tdir.Dot(radial);` | `tdir = MathAPI::ProjectOnPlane(tdir, radial);` |
| `CharaBase.cpp` `ResolveBump` | `float into = m_velocity.Dot(n);`<br>`if (into < 0.0f) { m_velocity -= n * into; }` | `m_velocity = MathAPI::ClipVelocity(m_velocity, n);` |
| `WireAction.cpp` 距離拘束 | `float radialSpeed = _vel.Dot(dir);`<br>`if (radialSpeed > 0) { _vel -= dir * radialSpeed; }` | `_vel = MathAPI::ClipVelocity(_vel, -dir);`<br>※符号が逆なので法線に `-dir` を渡す |
| `Player.cpp` `ClampSpeed` | `float sp = m_velocity.Length();`<br>`if (sp > maxSpeed) { m_velocity *= (maxSpeed / sp); }` | `m_velocity = MathAPI::ClampMagnitude(m_velocity, maxSpeed);` |

> ⚠️ `ProjectOnPlane`（両側を削る）と `ClipVelocity`（面へ入る側だけ削る）は式が似ているが**別物**。
> 混同すると壁から離れられなくなる。

### 2-5. ボーンを向ける回転（1箇所・段階3-aで3箇所になる）

| 元のコード（`CharaBase::UpdateArmAim`） | 新コード |
|---|---|
| `Math::Vector3 a = { 0, 1, 0 };`<br>`Math::Vector3 axis = a.Cross(dL);`<br>`if (axis.LengthSquared() < 0.0001f) { break; }`<br>`axis.Normalize();`<br>`float angle = std::acos(std::clamp(a.Dot(dL), -1.0f, 1.0f));`<br>`angle = std::min(angle, maxRad);`<br>`angle *= m_armAimWeight * strength;`<br>`Math::Matrix R = Math::Matrix::CreateFromAxisAngle(axis, angle);` | `Math::Vector3 boneAxis = { 0, 1, 0 };`<br>`Math::Matrix R;`<br>`if (!MathAPI::FromToRotation(boneAxis, dL, R, maxRad, m_armAimWeight * strength)) { break; }` |

既知の罠2つ（外積ゼロのガード / `acos`前のクランプ。どちらか欠けるとNaNがボーン行列を通って**メッシュ全体が消える**）が
`FromToRotation` の中1箇所に集まった。「上限を先に掛けてから重みを掛ける」順序の制約も関数側のコメントへ移した。

### 2-6. 【挙動が変わった唯一の箇所】敵の旋回

`MathAPI::RotateToDirection` は、`CharaBase::UpdateFacing` と**同じことを別実装でやっていた**ので1本化した。

| 元の実装 | 新しい実装 |
|---|---|
| ① 現在角から向きベクトルを作る（`CreateRotationY` + `TransformNormal`）<br>② 内積を `acos` に通してなす角を出す<br>③ 外積のY成分の符号で左右を決める<br>④ 0.1度未満は切り捨て | `GetSafeNormalXZ` → `DirToYawDeg` → `MoveTowardsAngleDeg` → `ClampAngleDeg` |

差は2点。**どちらも敵の見た目にはほぼ出ないはずだが未確認**：

1. **0.1度のデッドゾーンが無くなった** — 旧実装はわずかな向きのズレが永久に残っていた。180°/秒・60fpsなら1フレーム3°なので0.1°の残差は見えないはず
2. **`acos` に3Dの内積を渡していたのを水平成分だけに変えた** — 旧実装はy成分を持つベクトルを渡すと角度が過大になっていた。現在の呼び出し元（`Enemy`）は水平ベクトルを渡すので**この点は結果に影響しない**

---

## 3. あえて対象外にしたもの

| 対象 | 理由 |
|---|---|
| 度⇔ラジアン変換（16箇所） | `DirectX::XMConvertToRadians` が既に公式APIで、16箇所すべて統一済み。包むと呼び名が増えるだけ |
| `LevelPicker` のレイ交差計算 | レイ同士の最近点・レイと平面の交差。**1箇所ずつしか無い** |
| `m_velocity.y = 0.0f`（4箇所） | 落下を止める**物理的な操作**。`FlattenY`（向きの水平化）とは別物なので混同しない |
| `Player::SelectTilt` の `atan2(local.z, local.y)` 等 | ヨーではなくピッチ/ロールなので `DirToYawDeg` は使えない |
| `CharaBase.h` の `SelectFacingDir` | `.h` での `#include` は継承時のみという規約（CLAUDE.md）に反するため、ここだけ `Vector3(x, 0, z)` の直書きが残っている |
| 生の `.Normalize()` 4箇所 | 回転行列由来で常に単位ベクトル、または直前の判定でゼロでないことが保証済み。**元々ガードが無い場所にガードを足すと挙動が変わる**ので触っていない |
| 逆補間（`age / life` 等 4箇所） | 単なる除算で、まとめても短くならない |

---

## 4. 設計メモ

- **極小関数はヘッダに `inline` で置いている。** `Project.vcxproj` の Release 構成に
  `WholeProgramOptimization`(LTCG) が無い（`MaxSpeed` + `AnySuitable` のみ）ため、`.cpp` に定義すると
  他ファイルからインライン展開されず関数呼び出しのコストが残る。
  計算量のある `FromToRotation` / `RotateToDirection` だけ `.cpp`。
- **`MathAPI.h` は Pch.h に入れていない。** Pch.h は Framework 配下＝変更に確認が必要なため、
  各 `.cpp` から相対パスで include する方針。
- `kSmallNumber` は **`LengthSquared()` と比較する値**（長さ 0.01 相当）。`Length()` と比べないこと。
- 既存の `ApproachByLerp` は未使用だが削除せず、`InterpTo` との使い分けをコメントで残した。
