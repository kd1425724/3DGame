#=====================================================================
#  Wire.png ── 立体機動装置のワイヤー（板ポリに貼る鋼のケーブル）
#
#  縦(V)がケーブルの長さ方向。長いワイヤーには縦に繰り返して貼るので、
#  【縦方向に継ぎ目が出ないこと】が必須条件。Helixのturnsを整数にすると成立する。
#=====================================================================
. "$PSScriptRoot\..\TextureGen.ps1"
$OutRoot = Get-TexOutRoot -RecipeDir $PSScriptRoot

# --- つまみ（見た目を詰めるのはここだけ） ---
$W            = 32     # 幅  = ケーブルの断面方向
$H            = 128    # 高さ = ケーブルの長さ方向
$EdgeBright   = 0.22   # 縁の暗さ。小さいほど「丸い」と感じる
$EdgeSoft     = 0.55   # 縁のぼかし。小さいほどシャープな輪郭
$Strands      = 14     # 撚りの回数（整数＝タイルできる）
$StrandSkew   = 1.0    # 横方向の位相ずれ。0にすると「縞」になり撚りに見えない
$StrandDepth  = 0.55   # 撚りの濃さ
$StrandSharp  = 1.6    # 撚りの締まり。上げると真珠状の丸みが取れて筋になる

# 断面の陰影：中心が明るく縁が暗い＝平らな板を丸い線に見せる
$shade = [Tex]::AxisFalloff($W, $H, $true, 2.0).MulS(1.0 - $EdgeBright).AddS($EdgeBright)

# 撚り：位相が縦にも横にも進むので、斜めに巻き付いた筋になる。
# Powで締めると、単純なサインの「丸い玉が並ぶ」感じが筋状に変わる
$strand = [Tex]::Helix($W, $H, $Strands, $StrandSkew, $StrandDepth).Pow($StrandSharp)

# 輪郭：端でアルファを0にする。板ポリの四角い縁を消してケーブルの形にする
$alpha = [Tex]::AxisFalloff($W, $H, $true, $EdgeSoft)

# 陰影 × 撚り に、やや青い金属色を乗せる。
# 【なぜ白のままにしないか】描画側でも色を掛けるので、ここで色を付け過ぎると二重に乗る。
#   ほぼ無彩色にしておき、最終的な色味は描画側のDebugParamsで決められるようにする
$wire = $shade.Mul($strand).Tint(0.92, 0.96, 1.0).SetAlpha($alpha)

Save-TexChecked -Tex $wire -Path (Join-Path $OutRoot "Asset\Textures\Effect\Wire.png") -Tiles 'v'
