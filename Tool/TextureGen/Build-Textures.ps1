#=====================================================================
#  Build-Textures ── Recipes\*.ps1 を全部走らせてテクスチャを書き出す
#
#  使い方（プロジェクトのどこから実行してもよい）:
#      powershell -ExecutionPolicy Bypass -File Tool\TextureGen\Build-Textures.ps1
#
#  レシピを1つだけ回したいとき:
#      ... -File Tool\TextureGen\Build-Textures.ps1 -Only Wire
#
#  確認用の一覧画像を出したいとき（市松模様の上に並べるので透明部分が見える）:
#      ... -File Tool\TextureGen\Build-Textures.ps1 -Sheet
#
#  【なぜレシピを1つずつのファイルに分けるのか】
#    テクスチャは「数字を少し変えて撃ち直す」作業を何度も繰り返す。
#    1ファイルにまとめると目的の箇所を探すのに時間がかかり、
#    どの数字がどの絵に効くのかも分からなくなる。1枚=1ファイルにしておけば、
#    先頭のつまみだけ見て直せる。
#=====================================================================
[CmdletBinding()]
param(
    [string]$Only,          # レシピ名（拡張子なし）。省略時は全部
    [switch]$Sheet,         # 確認用の一覧画像を出す
    [string]$SheetPath      # 一覧画像の出力先。省略時は Tool\TextureGen\_preview.png
)

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\TextureGen.ps1"

# リポジトリのルート（Tool\TextureGen から2つ上）。レシピはここを基準に出力先を組む
$OutRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Write-Host ("出力先のルート: {0}" -f $OutRoot)

$recipeDir = Join-Path $PSScriptRoot "Recipes"
$recipes = Get-ChildItem -Path $recipeDir -Filter *.ps1 | Sort-Object Name
if ($Only)
{
    $recipes = $recipes | Where-Object { $_.BaseName -eq $Only }
    if (-not $recipes)
    {
        throw ("レシピが見つかりません: {0}（{1} を確認)" -f $Only, $recipeDir)
    }
}

$results = @()
$written = @()
foreach ($r in $recipes)
{
    # レシピは $OutRoot を使って出力先を決め、Save-TexChecked の結果を返す。
    # 出力先はレシピごとに違いうるので、返ってきた Path をそのまま使う（決め打ちしない）
    $res = & $r.FullName
    foreach ($item in $res)
    {
        $results += $item
        $written += $item.Path
    }
}

Write-Host ""
$results | Format-Table -AutoSize Name, Size, Bytes, RatioU, RatioV, Tiles, Warn
Write-Host "RatioU/V = 巻き戻り部分の差 ÷ 内部の隣接どうしの差。1前後なら模様が連続＝継ぎ目なし" -ForegroundColor DarkGray

# 警告があるものは目立たせる（タイルさせる方向に継ぎ目が残っていると線が見える）
$bad = $results | Where-Object { $_.Warn }
if ($bad)
{
    Write-Host "警告あり:" -ForegroundColor Yellow
    foreach ($b in $bad) { Write-Host ("  {0}: {1}" -f $b.Name, $b.Warn) -ForegroundColor Yellow }
}
else
{
    Write-Host "継ぎ目の検査はすべて基準内。" -ForegroundColor Green
}

#---------------------------------------------------------------------
# 確認用の一覧画像
#---------------------------------------------------------------------
if ($Sheet)
{
    if (-not $SheetPath) { $SheetPath = Join-Path $PSScriptRoot "_preview.png" }

    $zoom = 4
    $pad = 24
    $labelH = 26

    # 並べるものを読み直す（保存後のPNGを見る＝実際にゲームが読む中身を確認する）
    $items = @()
    foreach ($p in $written)
    {
        if (Test-Path $p)
        {
            $bmp = New-Object System.Drawing.Bitmap $p
            $items += [PSCustomObject]@{ Path = $p; Bmp = $bmp; Name = (Split-Path -Leaf $p) }
        }
    }

    # ラベルの幅も測って列幅に含める。
    # 【なぜ画像の幅だけで列を組んではいけないか】
    #   32pxのような細いテクスチャだと、拡大しても列がラベルより狭くなり、
    #   隣の列の文字と重なって両方読めなくなる（実際にそうなった）
    $measureBmp = New-Object System.Drawing.Bitmap 1, 1
    $measureG = [System.Drawing.Graphics]::FromImage($measureBmp)
    $labelFont = New-Object System.Drawing.Font "Segoe UI", 12

    foreach ($it in $items)
    {
        # タイル性を見せたいので、縦長のものは2回積んで表示する
        $reps = if ($it.Bmp.Height -ge $it.Bmp.Width * 2) { 2 } else { 1 }
        $caption = ("{0}  {1}x{2}  (x{3})" -f $it.Name, $it.Bmp.Width, $it.Bmp.Height, $zoom)
        if ($reps -gt 1) { $caption += " x2積み" }

        $capW = [Math]::Ceiling($measureG.MeasureString($caption, $labelFont).Width)
        $imgW = $it.Bmp.Width * $zoom

        $it | Add-Member -NotePropertyName Reps -NotePropertyValue $reps -Force
        $it | Add-Member -NotePropertyName Caption -NotePropertyValue $caption -Force
        $it | Add-Member -NotePropertyName ColW -NotePropertyValue ([Math]::Max($imgW, $capW)) -Force
        $it | Add-Member -NotePropertyName ImgW -NotePropertyValue $imgW -Force
    }

    $measureG.Dispose(); $measureBmp.Dispose()

    $totalW = $pad
    $maxH = 0
    foreach ($it in $items)
    {
        $totalW += $it.ColW + $pad
        $h = $it.Bmp.Height * $zoom * $it.Reps
        if ($h -gt $maxH) { $maxH = $h }
    }
    $totalH = $maxH + $labelH + $pad * 2

    $sheetBmp = New-Object System.Drawing.Bitmap $totalW, $totalH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $sg = [System.Drawing.Graphics]::FromImage($sheetBmp)
    $sg.Clear([System.Drawing.Color]::FromArgb(255, 38, 40, 44))

    # 市松模様の下地。これが透けて見える＝そこが透明という意味
    $checkBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 66, 68, 74))
    for ($yy = 0; $yy -lt $totalH; $yy += 16)
    {
        for ($xx = 0; $xx -lt $totalW; $xx += 16)
        {
            if ((($xx / 16) + ($yy / 16)) % 2 -eq 0)
            {
                $sg.FillRectangle($checkBrush, $xx, $yy, 16, 16)
            }
        }
    }
    $checkBrush.Dispose()

    # 拡大は最近傍にする。滑らかに補間すると「実際のピクセル」が見えなくなる
    $sg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $sg.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::Half

    $white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)

    $x = $pad
    foreach ($it in $items)
    {
        $h = $it.Bmp.Height * $zoom

        $y = $labelH + $pad
        for ($k = 0; $k -lt $it.Reps; $k++)
        {
            $sg.DrawImage($it.Bmp, $x, $y, $it.ImgW, $h)
            $y += $h
        }

        $sg.DrawString($it.Caption, $labelFont, $white, $x, $pad / 2)

        $x += $it.ColW + $pad
        $it.Bmp.Dispose()
    }

    $labelFont.Dispose(); $white.Dispose()
    $sheetBmp.Save($SheetPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $sg.Dispose(); $sheetBmp.Dispose()

    Write-Host ""
    Write-Host ("一覧画像: {0}" -f $SheetPath)
}
