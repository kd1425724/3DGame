#=====================================================================
#
#  TextureGen ── 手続き的テクスチャ生成の共通ライブラリ
#
#  ・「計算でテクスチャを描く」ための土台。AIによる画像生成ではない
#    （→ 何が作れて何が作れないかは README.md）
#  ・使い方：レシピ（Recipes/*.ps1）から dot-source して呼ぶ
#        . "$PSScriptRoot\..\TextureGen.ps1"
#  ・全レシピをまとめて実行するのは Build-Textures.ps1
#
#  【なぜ計算の核をC#にしているのか】
#    PowerShellのループは1ピクセルずつ回すと極端に遅い。
#    32x128（4千px）なら一瞬だが、1024x1024（100万px）だと数分かかり実用にならない。
#    Add-Type でC#を1回コンパイルしてしまえばネイティブ速度で回るので、
#    大きなテクスチャでも一瞬で済む。PowerShell側は「レシピを読みやすく書く」係に徹する。
#
#=====================================================================

Add-Type -AssemblyName System.Drawing

# C#の核は1セッションに1回だけコンパイルする（2回目以降は型が既にあるので飛ばす）
if (-not ([System.Management.Automation.PSTypeName]'Tex').Type)
{
    # ※ Windows PowerShell 5.1 の Add-Type は C#5 相当なので、
    #    文字列補間（$"..."）・式形式メンバ（=>）・out var は使えない
    Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

// 1枚のテクスチャ。RGBAをfloatで持つ（合成の途中で桁が潰れないように）
public class Tex
{
    public int W;
    public int H;
    public float[] P;   // RGBA RGBA ... 長さ = W*H*4

    public Tex(int w, int h)
    {
        W = w; H = h; P = new float[w * h * 4];
    }

    public Tex Clone()
    {
        Tex t = new Tex(W, H);
        Array.Copy(P, t.P, P.Length);
        return t;
    }

    static float Cl(float v) { if (v < 0f) return 0f; if (v > 1f) return 1f; return v; }
    static float Smooth(float t) { return t * t * (3f - 2f * t); }

    //--------------------------------------------------------------
    // 生成（いずれも「グレースケール1枚」を返す。RGBに同じ値、Aは1）
    // 色や透明度はこの後の合成（Tint / SetAlpha）で与える。
    // こう分けておくと「明るさの分布」と「切り抜きの形」を別々に設計できる
    //--------------------------------------------------------------

    public static Tex Const(int w, int h, float v)
    {
        Tex t = new Tex(w, h);
        for (int i = 0; i < t.P.Length; i += 4) { t.P[i] = v; t.P[i+1] = v; t.P[i+2] = v; t.P[i+3] = 1f; }
        return t;
    }

    // 中心線で1、両端で0。horizontal=true なら左右方向に減衰（縦のケーブル向き）
    public static Tex AxisFalloff(int w, int h, bool horizontal, float exponent)
    {
        Tex t = new Tex(w, h);
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float half, pos;
                if (horizontal) { half = (w - 1) / 2f; pos = x; }
                else            { half = (h - 1) / 2f; pos = y; }

                float d = half > 0f ? Math.Abs(pos - half) / half : 0f;
                float v = (float)Math.Pow(1.0 - Cl(d), exponent);
                int i = (y * w + x) * 4;
                t.P[i] = v; t.P[i+1] = v; t.P[i+2] = v; t.P[i+3] = 1f;
            }
        }
        return t;
    }

    // 中心で1、半径radiusで0。粒（パーティクル）の減衰に使う
    public static Tex Radial(int w, int h, float cx, float cy, float radius, float exponent)
    {
        Tex t = new Tex(w, h);
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float dx = x - cx, dy = y - cy;
                float d = (float)Math.Sqrt(dx * dx + dy * dy) / (radius > 0f ? radius : 1f);
                float v = (float)Math.Pow(1.0 - Cl(d), exponent);
                int i = (y * w + x) * 4;
                t.P[i] = v; t.P[i+1] = v; t.P[i+2] = v; t.P[i+3] = 1f;
            }
        }
        return t;
    }

    // 一方向のなだらかな傾斜（0→1）。angleDeg=0で左→右、90で上→下
    public static Tex Linear(int w, int h, float angleDeg)
    {
        Tex t = new Tex(w, h);
        double a = angleDeg * Math.PI / 180.0;
        float ax = (float)Math.Cos(a), ay = (float)Math.Sin(a);
        // 投影値の範囲で正規化する（角度によらず 0..1 に収める）
        float lo = 0f, hi = 0f;
        float[] cs = new float[] { 0f, 0f, w - 1f, 0f, 0f, h - 1f, w - 1f, h - 1f };
        for (int k = 0; k < 4; k++)
        {
            float p = cs[k*2] * ax + cs[k*2+1] * ay;
            if (k == 0) { lo = p; hi = p; }
            if (p < lo) lo = p;
            if (p > hi) hi = p;
        }
        float span = (hi - lo) != 0f ? (hi - lo) : 1f;

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float v = Cl((x * ax + y * ay - lo) / span);
                int i = (y * w + x) * 4;
                t.P[i] = v; t.P[i+1] = v; t.P[i+2] = v; t.P[i+3] = 1f;
            }
        }
        return t;
    }

    // 螺旋（撚り）。turns はテクスチャの縦方向に何回ねじれるか。
    // 【重要】turns が整数なら上下の端で位相が一致するので、縦に継ぎ目なく繰り返せる。
    //   skew は横方向の位相ずれで、これがあると「縞」ではなく「撚り」に見える
    public static Tex Helix(int w, int h, int turns, float skew, float depth)
    {
        Tex t = new Tex(w, h);
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                double phase = ((double)y / h) * 2.0 * Math.PI * turns
                             + ((double)x / w) * 2.0 * Math.PI * skew;
                float s = (float)(0.5 + 0.5 * Math.Sin(phase));
                float v = Cl((1f - depth) + depth * s);
                int i = (y * w + x) * 4;
                t.P[i] = v; t.P[i+1] = v; t.P[i+2] = v; t.P[i+3] = 1f;
            }
        }
        return t;
    }

    // 縞。periodX/periodY は「何回繰り返すか」（整数なら継ぎ目なし）。
    // duty=0.5 で線と隙間が半々、soft で縁のぼけ幅（ピクセルではなく比率）
    public static Tex Stripes(int w, int h, int repeatX, int repeatY, float duty, float soft)
    {
        Tex t = new Tex(w, h);
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float u = ((float)x / w) * repeatX + ((float)y / h) * repeatY;
                float f = u - (float)Math.Floor(u);            // 0..1 の位相
                float d = Math.Abs(f - 0.5f) * 2f;             // 中心0 端1
                float e = duty > 0f ? (1f - d / duty) : 0f;    // duty内で1→0
                float v;
                if (soft <= 0f) { v = e > 0f ? 1f : 0f; }
                else            { v = Cl(e / soft); }
                int i = (y * w + x) * 4;
                t.P[i] = v; t.P[i+1] = v; t.P[i+2] = v; t.P[i+3] = 1f;
            }
        }
        return t;
    }

    public static Tex Checker(int w, int h, int cell)
    {
        Tex t = new Tex(w, h);
        if (cell < 1) cell = 1;
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float v = (((x / cell) + (y / cell)) % 2 == 0) ? 1f : 0f;
                int i = (y * w + x) * 4;
                t.P[i] = v; t.P[i+1] = v; t.P[i+2] = v; t.P[i+3] = 1f;
            }
        }
        return t;
    }

    // 値ノイズ。cells は格子の数で、整数なので格子ごと巻き戻る＝【継ぎ目なくタイルできる】。
    // octaves で細かい成分を重ねる（各段で格子を倍にするので、倍でも整数のまま＝タイル性は保たれる）
    public static Tex ValueNoise(int w, int h, int seed, int cellsX, int cellsY, int octaves, float persistence)
    {
        Tex t = new Tex(w, h);
        if (cellsX < 1) cellsX = 1;
        if (cellsY < 1) cellsY = 1;
        if (octaves < 1) octaves = 1;

        float total = 0f, amp = 1f;
        for (int o = 0; o < octaves; o++) { total += amp; amp *= persistence; }
        if (total <= 0f) total = 1f;

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float sum = 0f;
                amp = 1f;
                int cx = cellsX, cy = cellsY;

                for (int o = 0; o < octaves; o++)
                {
                    float fx = ((float)x / w) * cx;
                    float fy = ((float)y / h) * cy;
                    int x0 = (int)Math.Floor(fx), y0 = (int)Math.Floor(fy);
                    float tx = Smooth(fx - x0), ty = Smooth(fy - y0);

                    float v00 = Hash(x0,     y0,     cx, cy, seed + o);
                    float v10 = Hash(x0 + 1, y0,     cx, cy, seed + o);
                    float v01 = Hash(x0,     y0 + 1, cx, cy, seed + o);
                    float v11 = Hash(x0 + 1, y0 + 1, cx, cy, seed + o);

                    float a = v00 + (v10 - v00) * tx;
                    float b = v01 + (v11 - v01) * tx;
                    sum += (a + (b - a) * ty) * amp;

                    amp *= persistence;
                    cx *= 2; cy *= 2;
                }

                float v = Cl(sum / total);
                int i = (y * w + x) * 4;
                t.P[i] = v; t.P[i+1] = v; t.P[i+2] = v; t.P[i+3] = 1f;
            }
        }
        return t;
    }

    // 格子点のハッシュ。cx/cyで剰余を取ることが「タイルできる」の正体
    static float Hash(int x, int y, int cx, int cy, int seed)
    {
        x = ((x % cx) + cx) % cx;
        y = ((y % cy) + cy) % cy;
        unchecked
        {
            int n = x * 374761393 + y * 668265263 + seed * 1442695040;
            n = (n ^ (n >> 13)) * 1274126177;
            n = n ^ (n >> 16);
            return (float)((uint)n % 100000u) / 99999f;
        }
    }

    //--------------------------------------------------------------
    // 合成・加工（自分を書き換えて自分を返す＝繋げて書ける）
    //--------------------------------------------------------------

    public Tex Mul(Tex o)    { for (int i = 0; i < P.Length; i++) P[i] *= o.P[i];                 return this; }
    public Tex Add(Tex o)    { for (int i = 0; i < P.Length; i++) P[i] += o.P[i];                 return this; }
    public Tex Screen(Tex o) { for (int i = 0; i < P.Length; i++) P[i] = 1f - (1f - P[i]) * (1f - o.P[i]); return this; }
    public Tex Max(Tex o)    { for (int i = 0; i < P.Length; i++) if (o.P[i] > P[i]) P[i] = o.P[i]; return this; }
    public Tex Min(Tex o)    { for (int i = 0; i < P.Length; i++) if (o.P[i] < P[i]) P[i] = o.P[i]; return this; }

    public Tex Lerp(Tex o, float k)
    {
        for (int i = 0; i < P.Length; i++) P[i] = P[i] + (o.P[i] - P[i]) * k;
        return this;
    }

    public Tex MulS(float s) { for (int i = 0; i < P.Length; i++) P[i] *= s; return this; }
    public Tex AddS(float s) { for (int i = 0; i < P.Length; i++) P[i] += s; return this; }

    public Tex Pow(float e)
    {
        for (int i = 0; i < P.Length; i += 4)
            for (int c = 0; c < 3; c++) P[i+c] = (float)Math.Pow(Cl(P[i+c]), e);
        return this;
    }

    // lo以下を0、hi以上を1にして間を伸ばす（コントラスト調整）
    public Tex Levels(float lo, float hi)
    {
        float span = (hi - lo) != 0f ? (hi - lo) : 1f;
        for (int i = 0; i < P.Length; i += 4)
            for (int c = 0; c < 3; c++) P[i+c] = Cl((P[i+c] - lo) / span);
        return this;
    }

    public Tex Invert()
    {
        for (int i = 0; i < P.Length; i += 4)
            for (int c = 0; c < 3; c++) P[i+c] = 1f - P[i+c];
        return this;
    }

    // RGBに色を掛ける（アルファは触らない）
    public Tex Tint(float r, float g, float b)
    {
        for (int i = 0; i < P.Length; i += 4) { P[i] *= r; P[i+1] *= g; P[i+2] *= b; }
        return this;
    }

    // 別のテクスチャの明るさ（R成分）を自分のアルファにする＝切り抜きの形を与える
    public Tex SetAlpha(Tex mask)
    {
        for (int i = 0; i < P.Length; i += 4) P[i+3] = Cl(mask.P[i]);
        return this;
    }

    public Tex SetAlphaS(float a)
    {
        for (int i = 0; i < P.Length; i += 4) P[i+3] = a;
        return this;
    }

    public Tex Clamp()
    {
        for (int i = 0; i < P.Length; i++) P[i] = Cl(P[i]);
        return this;
    }

    //--------------------------------------------------------------
    // 検査：継ぎ目なくタイルできているか
    //
    // 【単純に「両端が一致するか」を見てはいけない】
    //   模様が端で途中の値を取っていると、両端は当然違う値になる。
    //   例：撚りが14回巻く128pxのテクスチャは、隣の行との差が毎行0.687ラジアンぶんある。
    //   端どうしが違うのは異常ではなく、それが模様の正常な進み方。
    //   タイルしたとき最終行の次に先頭行が来るので、見るべきは
    //   「巻き戻る部分の差」が「内部の隣接行の差」と同程度かどうか（＝比）。
    //   同程度なら模様は連続していて継ぎ目は見えない。
    //--------------------------------------------------------------

    // 巻き戻り部分（最終列と先頭列 / 最終行と先頭行）の平均差
    public float WrapU()
    {
        double sum = 0; int n = 0;
        for (int y = 0; y < H; y++)
        {
            int a = (y * W + (W - 1)) * 4, b = (y * W + 0) * 4;
            for (int c = 0; c < 4; c++) { sum += Math.Abs(P[a+c] - P[b+c]); n++; }
        }
        return n > 0 ? (float)(sum / n) : 0f;
    }

    public float WrapV()
    {
        double sum = 0; int n = 0;
        for (int x = 0; x < W; x++)
        {
            int a = ((H - 1) * W + x) * 4, b = (0 * W + x) * 4;
            for (int c = 0; c < 4; c++) { sum += Math.Abs(P[a+c] - P[b+c]); n++; }
        }
        return n > 0 ? (float)(sum / n) : 0f;
    }

    // 内部の隣接列 / 隣接行の平均差（＝模様の正常な進み方の大きさ）
    public float StepU()
    {
        double sum = 0; int n = 0;
        for (int y = 0; y < H; y++)
        {
            for (int x = 0; x < W - 1; x++)
            {
                int a = (y * W + x) * 4, b = (y * W + x + 1) * 4;
                for (int c = 0; c < 4; c++) { sum += Math.Abs(P[a+c] - P[b+c]); n++; }
            }
        }
        return n > 0 ? (float)(sum / n) : 0f;
    }

    public float StepV()
    {
        double sum = 0; int n = 0;
        for (int y = 0; y < H - 1; y++)
        {
            for (int x = 0; x < W; x++)
            {
                int a = (y * W + x) * 4, b = ((y + 1) * W + x) * 4;
                for (int c = 0; c < 4; c++) { sum += Math.Abs(P[a+c] - P[b+c]); n++; }
            }
        }
        return n > 0 ? (float)(sum / n) : 0f;
    }

    //--------------------------------------------------------------
    // 入出力
    //--------------------------------------------------------------

    public void Save(string path)
    {
        Bitmap bmp = new Bitmap(W, H, PixelFormat.Format32bppArgb);
        BitmapData bd = bmp.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        byte[] row = new byte[Math.Abs(bd.Stride)];
        for (int y = 0; y < H; y++)
        {
            for (int x = 0; x < W; x++)
            {
                int i = (y * W + x) * 4;
                // GDI+ の 32bppArgb はメモリ上 BGRA の順
                row[x*4+0] = (byte)(Cl(P[i+2]) * 255f + 0.5f);
                row[x*4+1] = (byte)(Cl(P[i+1]) * 255f + 0.5f);
                row[x*4+2] = (byte)(Cl(P[i+0]) * 255f + 0.5f);
                row[x*4+3] = (byte)(Cl(P[i+3]) * 255f + 0.5f);
            }
            Marshal.Copy(row, 0, (IntPtr)(bd.Scan0.ToInt64() + (long)y * bd.Stride), W * 4);
        }
        bmp.UnlockBits(bd);
        bmp.Save(path, ImageFormat.Png);
        bmp.Dispose();
    }

    public static Tex FromFile(string path)
    {
        Bitmap bmp = new Bitmap(path);
        Tex t = new Tex(bmp.Width, bmp.Height);
        BitmapData bd = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        byte[] row = new byte[Math.Abs(bd.Stride)];
        for (int y = 0; y < bmp.Height; y++)
        {
            Marshal.Copy((IntPtr)(bd.Scan0.ToInt64() + (long)y * bd.Stride), row, 0, bmp.Width * 4);
            for (int x = 0; x < bmp.Width; x++)
            {
                int i = (y * bmp.Width + x) * 4;
                t.P[i+0] = row[x*4+2] / 255f;
                t.P[i+1] = row[x*4+1] / 255f;
                t.P[i+2] = row[x*4+0] / 255f;
                t.P[i+3] = row[x*4+3] / 255f;
            }
        }
        bmp.UnlockBits(bd);
        bmp.Dispose();
        return t;
    }

    // System.Drawing で描いた図形を取り込む（多角形や曲線はGDI+に任せるほうが安く、
    // アンチエイリアスも自前で書かずに済む）。Bitmap は呼び出し側が破棄する
    public static Tex FromBitmap(Bitmap bmp)
    {
        Tex t = new Tex(bmp.Width, bmp.Height);
        BitmapData bd = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        byte[] row = new byte[Math.Abs(bd.Stride)];
        for (int y = 0; y < bmp.Height; y++)
        {
            Marshal.Copy((IntPtr)(bd.Scan0.ToInt64() + (long)y * bd.Stride), row, 0, bmp.Width * 4);
            for (int x = 0; x < bmp.Width; x++)
            {
                int i = (y * bmp.Width + x) * 4;
                t.P[i+0] = row[x*4+2] / 255f;
                t.P[i+1] = row[x*4+1] / 255f;
                t.P[i+2] = row[x*4+0] / 255f;
                t.P[i+3] = row[x*4+3] / 255f;
            }
        }
        bmp.UnlockBits(bd);
        return t;
    }
}
'@
}

#---------------------------------------------------------------------
# PowerShell側の便利関数
#---------------------------------------------------------------------

# 多角形などをGDI+で描いてTexにする。
# $Draw には Graphics を受け取るスクリプトブロックを渡す（アンチエイリアス済み・背景は透明）
function New-TexFromDrawing
{
    param(
        [Parameter(Mandatory)][int]$Width,
        [Parameter(Mandatory)][int]$Height,
        [Parameter(Mandatory)][scriptblock]$Draw
    )

    $bmp = New-Object System.Drawing.Bitmap $Width, $Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))

    & $Draw $g

    $g.Dispose()
    $tex = [Tex]::FromBitmap($bmp)
    $bmp.Dispose()
    return $tex
}

# 多角形を塗る（New-TexFromDrawing の中で使う）。
# $Points は @(@(x,y), @(x,y), ...) の形
function Add-TexPolygon
{
    param(
        [Parameter(Mandatory)]$Graphics,
        [Parameter(Mandatory)][array]$Points,
        [Parameter(Mandatory)][int[]]$Rgb,
        [int]$Alpha = 255
    )

    $pts = @()
    foreach ($p in $Points)
    {
        $pts += (New-Object System.Drawing.PointF ([float]$p[0]), ([float]$p[1]))
    }

    $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb($Alpha, $Rgb[0], $Rgb[1], $Rgb[2]))
    $Graphics.FillPolygon($brush, [System.Drawing.PointF[]]$pts)
    $brush.Dispose()
}

# テクスチャを書き出し、寸法・アルファ・タイル性をまとめて報告する。
# 「保存できた」ではなく「意図どおりの中身か」を毎回見えるようにするのが目的
function Save-TexChecked
{
    param(
        [Parameter(Mandatory)][Tex]$Tex,
        [Parameter(Mandatory)][string]$Path,
        [ValidateSet('none','u','v','both')][string]$Tiles = 'none'
    )

    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path $dir))
    {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }

    $Tex.Save($Path)

    $bytes = (Get-Item $Path).Length

    # 「巻き戻る部分の差 ÷ 内部の隣接どうしの差」。
    #
    # 【閾値1.5の根拠（対照実験で決めた。推測ではない）】
    #   周期が閉じていれば、巻き戻りの一歩は内部の一歩と統計的に同じものになるので
    #   比は【ちょうど1.00】に収束する。撚りの回数を整数で 7/14/15 と変えても全て1.00だった。
    #   逆に非整数（13.3 / 14.5 / 20.7）では 1.80 / 2.69 / 2.04 になった。
    #   最初は閾値を3.0にしていたが、それでは壊れた3例すべてを見逃した。
    #   1.5 なら「整数=1.00前後」を通し「非整数=1.8以上」を捕まえられる。
    #   なお一方向の傾斜のような明確な不連続はもっと大きく出る（縦128pxなら比128）。
    #
    # 内部の差がほぼ0（べた塗り）のときは比が意味を持たないので、
    # その場合は巻き戻りの差をそのまま小さな閾値で見る
    $flat = 1e-4
    $ratioU = if ($Tex.StepU() -gt $flat) { $Tex.WrapU() / $Tex.StepU() } else { $null }
    $ratioV = if ($Tex.StepV() -gt $flat) { $Tex.WrapV() / $Tex.StepV() } else { $null }

    $seamLimit = 1.5

    $warn = @()
    $checkU = ($Tiles -eq 'u' -or $Tiles -eq 'both')
    $checkV = ($Tiles -eq 'v' -or $Tiles -eq 'both')

    if ($checkU)
    {
        if ($null -ne $ratioU)
        {
            if ($ratioU -gt $seamLimit) { $warn += ("横に継ぎ目(比{0:N2})" -f $ratioU) }
        }
        elseif ($Tex.WrapU() -gt 0.02)
        {
            $warn += ("横に継ぎ目(差{0:N3})" -f $Tex.WrapU())
        }
    }
    if ($checkV)
    {
        if ($null -ne $ratioV)
        {
            if ($ratioV -gt $seamLimit) { $warn += ("縦に継ぎ目(比{0:N2})" -f $ratioV) }
        }
        elseif ($Tex.WrapV() -gt 0.02)
        {
            $warn += ("縦に継ぎ目(差{0:N3})" -f $Tex.WrapV())
        }
    }

    $fmt = { param($r) if ($null -eq $r) { "-" } else { "{0:N2}" -f $r } }

    [PSCustomObject]@{
        Name   = Split-Path -Leaf $Path
        # フルパスを返す。呼び出し側が出力先を推測しなくて済むようにするため
        # （UI用など Effect\ 以外へ出すレシピを足したときに壊れないように）
        Path   = (Resolve-Path $Path).Path
        Size   = ("{0}x{1}" -f $Tex.W, $Tex.H)
        Bytes  = $bytes
        # 巻き戻り ÷ 内部（1前後なら継ぎ目なし。"-" は内部差がほぼ0で比が無意味な場合）
        RatioU = (& $fmt $ratioU)
        RatioV = (& $fmt $ratioV)
        Tiles  = $Tiles
        Warn   = ($warn -join ' / ')
    }
}

# リポジトリのルートを求める。レシピを単体で実行したときの保険。
# Build-Textures.ps1 経由なら $OutRoot が既に入っているのでそれを使う
function Get-TexOutRoot
{
    param([string]$RecipeDir)

    if ($script:OutRoot) { return $script:OutRoot }
    if ($global:OutRoot) { return $global:OutRoot }

    # Tool\TextureGen\Recipes から3つ上がリポジトリのルート
    return (Resolve-Path (Join-Path $RecipeDir "..\..\..")).Path
}
