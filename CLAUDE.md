# BaseFramework 3D — 開発ルール

C++/DirectX11製の3Dゲームフレームワーク。TPS視点のアクションゲームを制作中。

**このファイルの運用**：コーディング規約・設計判断・ワークフローなど、今後のセッションにも影響する重要なルールが会話中に決まった場合は、その場でこのファイルに追加してよいか確認すること。

## アーキテクチャ

- `Src/Framework/` — エンジン非依存の汎用機能（Direct3D, Shader, Math, Audio, Input, Font, Effekseer, Window, Utility, `KdGameObject`基底クラスなど）
  - **Framework配下を変更する場合は必ず先に確認を取ること**
- `Src/Application/` — ゲーム固有のコード（Scene, GameObject/Camera派生, LevelEditor, Debugツールなど）
  - 新機能はここに実装する

## アセット

- **テクスチャは用途でフォルダを分ける**：
  - `Asset/Textures/System/` … エンジン汎用・下地用のダミー（`WhiteNoise.png`など）
  - `Asset/Textures/UI/` … ゲームのUI画像（照準`Reticle.png`、今後のHPバー・アイコン・ボタン等）。コード側の`Src/Application/UI/`と対応させる
- 新しいUI画像はまず`Asset/Textures/UI/`に置く。読み込みパスは`"Asset/Textures/UI/○○.png"`

## Git運用

- コードの編集は必ず**git worktree（コピー側）**で行い、本体（`main`ブランチの作業ディレクトリ）を直接編集しない
- worktree側で意味のある単位ごとにコミットしてから、内容を提示してユーザーに確認を取る
- **本体へマージする前に、毎回`git status`で本体側の状態を確認する**（ユーザーがVisual Studioで本体を直接並行編集していることがあるため。何か変更があれば内容を提示し、取り込む/破棄するか確認してから進める）
- push・PR作成は別途都度確認する。`gh` CLIはこの環境に無いため、PRは手動作成用のURLを案内する

## C++コーディング規約

1. **新規`.h`/`.cpp`ファイルには必ずUTF-8 BOM（`EF BB BF`）を付ける**
   Writeツールで作成したファイルはBOM無しになるため、作成直後に手動で付与すること。無いとMSVCがソースをUTF-8と認識できず、`U8("...")`経由の日本語文字列が文字化けする。
2. **`.h`ファイルでの`#include`は継承時（基底クラスの完全な定義が必要な場合）のみ許可。** それ以外（ポインタ/参照/`shared_ptr`/`weak_ptr`で使うだけの型）は前方宣言し、実際のincludeは`.cpp`側に書く。
   - `KdGameObject`・`KdCollider`などFramework側の型はPch.hの強制インクルード経由で全ファイルから既に見えているため、明示的なincludeは不要（継承時も含めて不要）。
3. **シングルトンのコンストラクタから`Init()`を呼ばない。** `Foo() { Init(); }` + `static Foo& Instance()`という形は、`Init()`の中で（間接的にでも）同じ`Instance()`を再度呼ぶと、static変数の初期化中に自己再入してデッドロックする。`Init()`はpublicにして、`Instance()`の外側（`Application::Init()`など）から明示的に1回呼ぶこと。
4. **移動などの「意思決定」は`Update()`、当たり判定・接地判定などの「world状態に対する解決」は`PostUpdate()`で行う。** オブジェクトリストの並び順に依存しない安定した処理にするため。
5. **命名規則**：メンバ変数には`m_`、引数には`_`を先頭に付ける（例：`m_moveSpeed`、`void SetPos(const Math::Vector3& _pos)`）。この規則は`Src/Application/`配下の新規コードに適用する。
   - **ポインタ種別の接頭辞**：`m_`/`_`の後に、生ポインタは`p`、`weak_ptr`は`wp`、`shared_ptr`は`sp`を付ける（例：`m_pCollider`、`m_wpCamera`、`m_spEditorCamera`、`std::shared_ptr<Enemy>& _spEnemy`）
   - **複数単語はキャメルケース**：2語目以降の頭を大文字にして単語を見分けられるようにする（例：`m_jumpPower`、`m_moveSpeed`、`m_verticalVelocity`）
   - **例外**：`Src/Framework/`配下は既存コードのままとし、この命名規則を適用しない
   - **例外**：`Src/Application/`内でも`CameraBase`・`TPSCamera`・`FPSCamera`はこの命名規則を適用しない（既存のコードのまま）

## 入力

- キー/マウス入力は`GetAsyncKeyState`を直接呼ばず、**`KdInputManager`経由**で扱う
- ボタン/軸は`main.cpp`の`Application::Init()`内で名前を付けて一括登録済み（`Move`軸、`RightClick`、`Ctrl`など）。新しい入力が必要な場合もここに追加する
- 判定は`KdInputManager::Instance().IsPress/IsHold/IsRelease("名前")`、軸は`GetAxisState("名前")`

## Effekseerエフェクト

- `.efk`は`Asset/Data/Effect/`配下に置き、`KdEffekseerManager::GetInstance().Play("相対パス", ...)`で再生する（`EffekseerPath`基準の相対パス。サブフォルダ可）
- **`.efk`は必ず書式Version17(1710)でエクスポートすること。** `../Library/Effekseer`のランタイムはVersion17までしか対応しておらず、Effekseer Editor 1.80.5が標準で吐くVersion18(1810)を読むと`Effekseer::Effect::Create()`が`nullptr`を返し、DEBUG構成では`KdEffekseerManager::Play()`のassertで停止する。バージョンは`.efk`先頭のマジック`SKFE`直後のu32(リトルエンディアン)で確認できる（`ae 06`=1710 / `12 07`=1810）
- 「作成失敗」assertが出たら、まず①`.efk`のバージョンタグ、②`Play()`に渡すパスのスペル、の順で疑う
- 編集用の`.efkproj`はリポジトリに含めない（実行用の`.efk`のみ管理する）。1810を恒久的に使いたい場合は`C:/3DGame/EffekseerForCpp1.80.5`(SDKソース)をビルドして`Library/Effekseer`のInc/Libを差し替える必要がある

### 再生時のトラブルと対処（BlueLaser調査 2026-07-07で判明）

- **エフェクトが消えずに残り続ける**：素材側のノードに①生成数の「無限」、または②生存時間が極端に長い値（例:100000フレーム）が残っているのが原因。フレームワークは全パーティクル消滅(`GetInstanceCount==0`)まで再生リストから外さないので、無限/長寿命ノードがあると永久に残る。対処は(a)Editorで該当ノードの生成数・生存時間を有限にする、または(b)ゲーム側で一定時間後に`KdEffekseerObject::StopEffect()`で強制停止する（`Player`の`FiredLaser`/`UpdateFiredLasers`が実装例。素材の状態に関係なく打ち切れる安全弁）
- **エフェクトの一部が原点(0,0,0)に出る**：そのノードの「位置への影響」が「生成時のみ(親に追従)」になっていると、生成の瞬間の親位置しか受け取らず、遅延生成ノードは位置が渡らず原点に固定される。対処はEditorで該当ノードを「常時(親に追従)」に変更する。あわせて、発射位置に置いたエフェクトは`SetWorldMatrix`を毎フレーム適用し直すこと（1回だけだと後半の遅延生成ノードに反映されない。`Player::UpdateFiredLasers`参照）

## DebugParams（調整値の外部化）

- 調整する数値は`DebugParams::Instance().Float(U8("カテゴリ/名前"), 既定値, min, max)`経由にして、実行中にImGuiで調整・JSON保存できるようにする（`Int`/`Bool`/`Vector3Param`も同様）
- **日本語キーは必ず`U8()`で包む。** プロジェクトは`/utf-8`無しで実行文字セットがShift-JISのため、素の文字列リテラルの日本語キーはShift-JISに変換され、UTF-8前提のImGuiで文字化けする
- **値の「真実の源」はコードの`Float()`第2引数（既定値）。** `DebugParams.json`は開発者ローカルのチューニング用スクラッチとして扱い、`/Asset/Data/Debug/`で**gitignore**する。起動時に`Application::Init()`が`DebugParams::Instance().Load()`を呼ぶのでローカルではJSONが既定値を上書きするが、JSONが無い環境（クリーン/製品ビルド）では既定値にフォールバックする（製品をJSONに依存させない意図）
- **運用フロー**：実行中に調整 → 値が確定したら、その数値を**コードの`Float()`既定値に手で書き写す** → JSONは使い捨て。JSONをコミットして真実の源にはしない（日本語キーでdiffが読みにくく、マージ衝突もしやすいため）
- 対比：レベルデータ`Asset/Data/LevelEditor/*.json`は**ゲームの中身なので追跡する**。開発者専用のチューニング=gitignore＋コード焼き込み、ゲーム内容=追跡、が使い分けの目安

## 使用ライブラリ

プロジェクトルートから見て`../Library/`（`C:/3DGame/Library/`）に配置。`Project.vcxproj`の`AdditionalIncludeDirectories`から参照している。

| ライブラリ | バージョン | 用途 |
|---|---|---|
| **DirectXTK**（DirectX Tool Kit） | - | `Math`名前空間（`DirectX::SimpleMath`）、`Audio`、`SpriteBatch`等の基盤。`Inc`/`Lib`/`Bin`をWin32/x64別に格納 |
| **DirectXTex** | - | テクスチャの読み込み・変換 |
| **Effekseer** / **Effekseer(Win32)** | - | パーティクル/視覚エフェクト再生エンジン（`KdEffekseerManager`が使用）。`.efk`ファイルを読み込んで再生する。x64用と Win32用でフォルダが分かれている |
| **imgui**（Dear ImGui） | 1.90.6 WIP | デバッグ用UI（DebugFlags/DebugWatch/DebugParams/DebugEffect、LevelEditor等）。日本語文字列は`U8()`マクロ経由で渡すこと |
| **nlohmann/json** | 3.12.0 | JSON読み書き（`LevelFileIO`、`DebugParams`の保存/読込で使用）。`#include "nlohmann/json.hpp"`はPch.hで読み込み済み |
| **tinygltf** | - | glTFモデルの読み込み（`KdGLTFLoader`が使用）。`stb_image.h`/`stb_image_write.h`も同梱 |
| **strconv.h** | 1.0.0 (2019/02/27) | 文字コード変換（Shift-JIS⇔UTF-8⇔wide文字列）の単一ヘッダー |

※ バージョン番号は各ライブラリのヘッダー内マクロ等から確認できたもののみ記載。空欄のものはヘッダー内に明確なバージョン表記が見つからなかった。
