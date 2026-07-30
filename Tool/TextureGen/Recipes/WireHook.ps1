#=====================================================================
#  WireHook.png ── ワイヤー先端のフック（3本爪）
#
#  ビルボード1枚で描くので、正面から見た形だけを持つ。
#  ゲーム内では小さく高速に動くため、細部より【シルエットが読めること】を優先する。
#  角ばった多角形で塗るのは、街や敵の低ポリ調と語彙を揃えるため。
#=====================================================================
. "$PSScriptRoot\..\TextureGen.ps1"
$OutRoot = Get-TexOutRoot -RecipeDir $PSScriptRoot

$S = 64   # 正方形。ビルボードなので縦横比は1でよい

# 明るい面と暗い面の2色で塗ると、平面的な塗りでも金属の厚みが出る。
# ほぼ無彩色にしておく理由はWire.ps1と同じ（描画側で色を掛けるので二重に乗せない）
$lit   = @(238, 244, 252)
$shade = @(150, 170, 190)

$hook = New-TexFromDrawing -Width $S -Height $S -Draw {
    param($g)

    # 柄：ワイヤー側（下）へ伸びる。線と繋がって見えるよう下端まで届かせる
    Add-TexPolygon -Graphics $g -Rgb $shade -Points @(@(28,29), @(36,29), @(35,64), @(29,64))

    # 中央の爪：まっすぐ上。奥にあるので暗い側で塗り、左右の爪より先に描く
    Add-TexPolygon -Graphics $g -Rgb $shade -Points @(@(32,3), @(38,31), @(26,31))

    # 左右の爪：外へ反り返らせる。左右で角度をわずかに変えて機械的な対称を崩す
    Add-TexPolygon -Graphics $g -Rgb $lit -Points @(@(9,13),  @(17,7),  @(31,27), @(24,33))
    Add-TexPolygon -Graphics $g -Rgb $lit -Points @(@(55,15), @(47,8),  @(33,27), @(40,33))

    # 爪の根元をまとめる塊。3本が1点から生えているように見せる
    Add-TexPolygon -Graphics $g -Rgb $lit -Points @(@(24,26), @(40,26), @(37,34), @(27,34))
}

Save-TexChecked -Tex $hook -Path (Join-Path $OutRoot "Asset\Textures\Effect\WireHook.png") -Tiles 'none'
