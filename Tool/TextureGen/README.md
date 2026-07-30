# TextureGen — 手続き的テクスチャ生成

計算でテクスチャ（PNG）を描くための道具。Windows標準の機能だけで動くので、
インストールも外部サービスも要らない。

```
powershell -ExecutionPolicy Bypass -File Tool\TextureGen\Build-Textures.ps1 -Sheet
```

`-Sheet` を付けると確認用の一覧画像 `Tool\TextureGen\_preview.png` も出る
（市松模様の上に並べるので、透明な部分がそこだと分かる）。
1枚だけ作り直すなら `-Only Wire` のようにレシピ名を渡す。

## 🔴 これはAIの画像生成ではない

**プロンプトから絵を作る道具ではありません。** 中身は「各ピクセルの値を計算する式」です。
この区別を忘れると必ず期待外れになるので、最初に書いておきます。

| | 作れる | 作れない |
|---|---|---|
| | グラデーション（直線・放射・軸方向の減衰） | **キャラクター** |
| | 繰り返し模様（縞・撚り・市松・値ノイズ） | **ゴーレムや岩肌などの造形** |
| | 角ばった図形・アイコン・シルエット | 写実的な材質 |
| | 正確なアルファ（透明度） | 「それが何に見えるか」の知識が要るもの |
| | 指定寸法ぴったり・継ぎ目なく繰り返せる模様 | 手描きの絵 |

**造形が必要なものは、この道具では届きません。** 画像生成サービスかBlenderを使うこと。

## 構成

```
Tool/TextureGen/
  TextureGen.ps1       共通ライブラリ（計算の核はC#。下記「なぜC#か」）
  Build-Textures.ps1   Recipes/*.ps1 を全部走らせる入口
  Recipes/
    Wire.ps1           ワイヤー本体（撚った鋼のケーブル）
    WireHook.ps1       ワイヤー先端のフック（3本爪）
```

**1枚のテクスチャ = 1つのレシピファイル。** 先頭につまみ（調整用の変数）を並べてあるので、
見た目を詰めるときはそこだけ触る。1ファイルに全部まとめると、
どの数字がどの絵に効くのか分からなくなるため分けている。

### なぜ計算の核をC#にしているのか

PowerShellのループは1ピクセルずつ回すと極端に遅い。32×128（4千px）なら一瞬だが、
1024×1024（100万px）だと数分かかって実用にならない。
`Add-Type` でC#を1回コンパイルしてしまえばネイティブ速度で回る。
PowerShell側は「レシピを読みやすく書く」係に徹している。

※ Windows PowerShell 5.1 の `Add-Type` は **C#5相当**。文字列補間（`$"..."`）・
式形式メンバ（`=>`）・`out var` は使えないので、C#部分を触るときは注意。

## 新しいテクスチャを足す手順

1. `Recipes/なまえ.ps1` を作る
2. 先頭に `. "$PSScriptRoot\..\TextureGen.ps1"` と `$OutRoot = Get-TexOutRoot -RecipeDir $PSScriptRoot`
3. 生成関数を組み合わせて `[Tex]` を作る
4. 最後に `Save-TexChecked -Tex $t -Path (Join-Path $OutRoot "Asset\Textures\...\なまえ.png") -Tiles 'none'`
5. **UTF-8 BOM を付ける**（下記）
6. `Build-Textures.ps1` を走らせる

### 🔴 新規の .ps1 には必ず UTF-8 BOM を付ける

Windows PowerShell 5.1 は **BOMの無いUTF-8をANSIとして読む**ので、日本語コメントが
文字化けし、**文字列の引用符が壊れてパースエラーになる**（実際にやった）。
プロジェクトの `.h`/`.cpp` と同じ理由・同じ対処。

```powershell
$b = [System.IO.File]::ReadAllBytes($f)
[System.IO.File]::WriteAllBytes($f, ([byte[]](0xEF,0xBB,0xBF) + $b))
```

## 生成関数（すべてグレースケール1枚を返す）

色や透明度は後段の合成で与える。こう分けると「明るさの分布」と「切り抜きの形」を
別々に設計できる。

| 関数 | 用途 |
|---|---|
| `[Tex]::Const(w,h,v)` | べた塗り |
| `[Tex]::AxisFalloff(w,h,horizontal,exp)` | 中心線で1、両端で0。**線を丸く見せる** |
| `[Tex]::Radial(w,h,cx,cy,r,exp)` | 中心で1、半径で0。**粒（パーティクル）の減衰** |
| `[Tex]::Linear(w,h,angleDeg)` | 一方向の傾斜 |
| `[Tex]::Helix(w,h,turns,skew,depth)` | 撚り。**turnsは整数**（→タイル性） |
| `[Tex]::Stripes(w,h,repX,repY,duty,soft)` | 縞。**repは整数** |
| `[Tex]::Checker(w,h,cell)` | 市松 |
| `[Tex]::ValueNoise(w,h,seed,cellsX,cellsY,oct,pers)` | ノイズ。**cellsは整数** |
| `New-TexFromDrawing -Width -Height -Draw {...}` | 多角形や曲線をGDI+で描いて取り込む |

**繰り返し回数がintなのは意図的。** 整数なら模様が端で必ず閉じるので、
型の段階でタイル性が保証される。

## 合成・加工（繋げて書ける）

`Mul` `Add` `Screen` `Max` `Min` `Lerp` `MulS` `AddS` `Pow` `Levels` `Invert`
`Tint(r,g,b)`（RGBだけ） `SetAlpha(mask)`（別テクスチャの明るさをアルファにする）
`SetAlphaS(a)` `Clamp` `Clone`

```powershell
$shade = [Tex]::AxisFalloff($W,$H,$true,2.0).MulS(0.78).AddS(0.22)
$alpha = [Tex]::AxisFalloff($W,$H,$true,0.55)
$tex   = $shade.Mul($strand).Tint(0.92,0.96,1.0).SetAlpha($alpha)
```

## 継ぎ目の検査（RatioU / RatioV）

`Save-TexChecked -Tiles 'v'` のように「どの方向に繰り返して使うか」を宣言すると、
その方向の継ぎ目を検査して警告する。

**「両端の値が一致するか」を見てはいけない。** 模様が端で途中の値を取っていれば
両端は当然違う値になる。見るのは
**「巻き戻る部分の差」÷「内部の隣接どうしの差」**で、これが1前後なら模様は連続している。

| 対照実験の結果 | RatioV |
|---|---|
| 撚り7回・14回・15回（整数＝閉じる） | **1.00**（ぴったり） |
| 撚り13.3回・14.5回・20.7回（非整数＝閉じない） | 1.80 / 2.69 / 2.04 |
| 縦方向の傾斜（明確に閉じない） | 127.0 |

閾値は **1.5**。最初3.0にしていたら壊れた3例すべてを見逃した（実測）ので、
実験して決め直した値。この数字を動かすときは同じ実験をやり直すこと。

## 既存のテクスチャは置き換えていない

`Particle.png` `Slash.png` `Reticle*.png` は既にあるものをそのまま使っている。
この道具で作り直すこともできるが、**動いている見た目を近似で置き換える必要がない**ため
触っていない。
