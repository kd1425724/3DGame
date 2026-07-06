# BaseFramework 3D — 開発ルール

C++/DirectX11製の3Dゲームフレームワーク。TPS視点のアクションゲームを制作中。

**このファイルの運用**：コーディング規約・設計判断・ワークフローなど、今後のセッションにも影響する重要なルールが会話中に決まった場合は、その場でこのファイルに追加してよいか確認すること。

## アーキテクチャ

- `Src/Framework/` — エンジン非依存の汎用機能（Direct3D, Shader, Math, Audio, Input, Font, Effekseer, Window, Utility, `KdGameObject`基底クラスなど）
  - **Framework配下を変更する場合は必ず先に確認を取ること**
- `Src/Application/` — ゲーム固有のコード（Scene, GameObject/Camera派生, LevelEditor, Debugツールなど）
  - 新機能はここに実装する

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
5. **命名規則**：メンバ変数には`m_`、引数には`_`を先頭に付ける（例：`m_moveSpeed`、`void SetPos(const Math::Vector3& _pos)`）。
   - **例外**：`CameraBase`・`TPSCamera`・`FPSCamera`はこの命名規則を適用しない（既存のコードのまま）。

## 入力

- キー/マウス入力は`GetAsyncKeyState`を直接呼ばず、**`KdInputManager`経由**で扱う
- ボタン/軸は`main.cpp`の`Application::Init()`内で名前を付けて一括登録済み（`Move`軸、`RightClick`、`Ctrl`など）。新しい入力が必要な場合もここに追加する
- 判定は`KdInputManager::Instance().IsPress/IsHold/IsRelease("名前")`、軸は`GetAxisState("名前")`

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
