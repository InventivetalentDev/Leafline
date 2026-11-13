// M_TreeBark2, Custom node "Bark" (look pass, 13 Nov). Source of truth for the node's Code; Tools/materials/build_bark2.py
// pastes it in over unreal-mcp. Inputs: UV1 (R cos, R sin in cm), UV2 (distance along the branch split hi/lo, see
// TreeMeshBuilder.h), VC (vertex colour: level/3, per-branch random, height on tree, buried), WP (world position),
// N (vertex normal WS), T (vertex tangent WS, the direction of increasing angle round the branch), Cam (MPC_View CamPos).
// Returns the base colour; additional outputs Rough, NormalW (world-space normal) and AO.
// Bark lives on a cylinder per branch: q = (UV1, along * ALONG). Plates are Voronoi cells of q (longer along the
// branch); fissures are the soft cell borders, darker wood, never outlines. Relief comes from the analytic gradient of
// the same height (normal only, no displacement). Fine grain fades with the pixel footprint, fissures widen with it.
struct FBark2
{
	float H1(float3 p)
	{
		p = frac(p * 0.1031);
		p += dot(p, p.zyx + 31.32);
		return frac((p.x + p.y) * p.z);
	}
	float3 H3(float3 p)
	{
		p = frac(p * float3(0.1031, 0.1030, 0.0973));
		p += dot(p, p.yxz + 33.33);
		return frac((p.xxy + p.yxx) * p.zyx);
	}
	// Value noise in [0,1] with its analytic gradient (yzw).
	float4 VN(float3 x)
	{
		float3 i = floor(x);
		float3 f = x - i;
		float3 u = f * f * (3.0 - 2.0 * f);
		float3 du = 6.0 * f * (1.0 - f);
		float a = H1(i);
		float b = H1(i + float3(1, 0, 0));
		float c = H1(i + float3(0, 1, 0));
		float d = H1(i + float3(1, 1, 0));
		float e = H1(i + float3(0, 0, 1));
		float g = H1(i + float3(1, 0, 1));
		float h = H1(i + float3(0, 1, 1));
		float k = H1(i + float3(1, 1, 1));
		float k1 = b - a, k2 = c - a, k3 = e - a, k4 = a - b - c + d, k5 = a - c - e + h, k6 = a - b - e + g;
		float k7 = -a + b + c - d + e - g - h + k;
		float v = a + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z;
		float3 dv = du * float3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z,
		                        k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
		                        k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);
		return float4(v, dv);
	}
};
FBark2 B;

// Shape (cm unless noted)
const float PLATE = 14.0;       // ridge width across the branch
const float ALONG = 0.085;      // along compression: ridges about twelve times longer along the branch than across
const float FIS_W = 0.5;        // soft width of a furrow (cell units): wide valleys, rounded ridges, no outlines
const float FIS_DEPTH = 2.6;    // furrow depth
const float DOME = 1.5;         // ridge rounding
const float GRAIN_DEPTH = 0.12; // fine grain depth
const float MOSS_AMOUNT = 1.0;
// Colour (linear)
// Warmer than first tried (13 Nov): with red/blue about 1.5 shadowed bark read slate grey-green; 2.2 keeps it brown.
const float3 PLATE_DARK = float3(0.14, 0.095, 0.062);
const float3 PLATE_LIGHT = float3(0.32, 0.23, 0.145);
const float3 YOUNG = float3(0.22, 0.15, 0.10);
const float FISSURE_SHADE = 0.42; // furrow colour as a fraction of the ridge colour beside it
const float3 MOSS_A = float3(0.085, 0.13, 0.035);
const float3 MOSS_B = float3(0.12, 0.16, 0.05);

float R = max(length(UV1.xy), 0.001);
float2 radial = UV1.xy / R;
float2 aroundQ = float2(-radial.y, radial.x);
float along = UV2.x + UV2.y;
float3 q = float3(UV1.xy, along * ALONG + VC.g * 2000.0);   // ridge space (along compressed)
float3 qw = float3(UV1.xy, along + VC.g * 5000.0);           // true proportions, for colour, moss and grain
// Pixel footprint (cm per pixel at 1080p and 75 degrees): fades detail that would shimmer.
float fp = distance(WP.xyz, Cam.xyz) * 0.0008;
float smallness = 1.0 - smoothstep(6.0, 30.0, R); // twigs: smoother, younger bark

// Plates: Voronoi on a gently warped q.
float3 p = q / PLATE;
// Warp across the branch, slowly along it: the furrows wander and meet like real bark.
float3 pwq = p * float3(0.35, 0.35, 0.6);
float wa = B.VN(pwq + 11.0).x;
float wb = B.VN(pwq + 47.0).x;
float wc = B.VN(p * float3(1.6, 1.6, 0.5) + 23.0).x;
float3 pw = p + float3(wa - 0.5, wb - 0.5, 0.0) * 1.4;
float3 ip = floor(pw);
float3 fq = pw - ip;
float f1 = 9.0, f2 = 9.0, id = 0.0;
float3 r1 = 0, r2 = 0;
[unroll] for (int z = -1; z <= 1; z++)
[unroll] for (int y = -1; y <= 1; y++)
[unroll] for (int x = -1; x <= 1; x++)
{
	float3 g = float3(x, y, z);
	float3 r = g + B.H3(ip + g) - fq;
	float dd = dot(r, r);
	if (dd < f1) { f2 = f1; r2 = r1; f1 = dd; r1 = r; id = B.H1(ip + g + 17.0); }
	else if (dd < f2) { f2 = dd; r2 = r; }
}
f1 = sqrt(f1);
f2 = sqrt(f2);
float e = f2 - f1;
float W = max(FIS_W * (0.6 + 0.8 * wc), fp * 1.5 / PLATE);
float s = saturate(e / W);
float fis = 1.0 - s * s * (3.0 - 2.0 * s);            // 1 in a fissure, 0 on a plate
float dfis = -6.0 * s * (1.0 - s) / W;               // d fis / d e
float3 de = r1 / max(f1, 1e-4) - r2 / max(f2, 1e-4);  // gradient of e in cell units
float depth = FIS_DEPTH * (1.0 - 0.75 * smallness);
float reliefFade = 1.0 - smoothstep(1.0, 3.0, fp);
float3 gq = (-depth * dfis * de + 2.0 * DOME * r1) / PLATE * reliefFade;

// Fine grain, streaks along the branch, only up close. gw is the height gradient in qw. (A layer of small ridged-noise
// cracks was tried on 13 Nov: it drew looping outlines, the look this bark replaces.)
float3 gw = 0;
float grainAmt = 1.0 - smoothstep(0.06, 0.2, fp);
float grain = 0.5;
[branch] if (grainAmt > 0.001)
{
	float3 sc = float3(1.0 / 0.45, 1.0 / 0.45, 1.0 / 2.2);
	float4 n1 = B.VN(qw * sc);
	float4 n2 = B.VN(qw * sc * 2.03 + 19.0);
	grain = n1.x * 0.65 + n2.x * 0.35;
	gw += (n1.yzw * sc * 0.65 + n2.yzw * sc * (2.03 * 0.35)) * (2.0 * GRAIN_DEPTH * grainAmt);
}
grain = lerp(0.5, grain, grainAmt);

// Colour variation: mottling within ridges and a slow tone change across the tree.
float mottle = B.VN(qw / float3(5.0, 5.0, 9.0) + 3.0).x;
float macro = B.VN(qw / 300.0 + 7.0).x;

// Moss: patches (elongated along, like water runs) and creases, a little more on top of limbs and low down.
float m1 = B.VN(qw / float3(60.0, 60.0, 110.0) + 5.0).x;
float m2 = B.VN(qw / float3(20.0, 20.0, 35.0) + 9.0).x;
float3 Nn = normalize(N.xyz);
float up = saturate(Nn.z);
float mossField = m1 * 0.7 + m2 * 0.3 + 0.12 * fis + 0.15 * up + 0.08 * (1.0 - VC.b);
float moss = smoothstep(0.69, 0.83, mossField) * MOSS_AMOUNT * (1.0 - 0.7 * smallness);

float tone = 0.15 + 0.8 * id;
float3 plate = lerp(PLATE_DARK, PLATE_LIGHT, tone);
plate *= 0.8 + 0.4 * mottle;
plate *= 1.0 + 0.12 * saturate(e * 3.0 - 0.3);
plate = lerp(plate, YOUNG, smallness * 0.6);
plate *= 0.85 + 0.3 * macro;
plate *= 0.85 + 0.3 * grain;
float3 col = plate * lerp(1.0, FISSURE_SHADE, fis * (1.0 - 0.6 * smallness));
col = lerp(col, lerp(MOSS_A, MOSS_B, m2), moss);

Rough = saturate(lerp(lerp(0.74 + 0.12 * (1.0 - tone), 0.95, fis), 0.9, moss) + (grain - 0.5) * 0.1);
AO = lerp(1.0, 0.55, fis * reliefFade * (1.0 - 0.5 * moss));

float3 Tt = normalize(T.xyz - Nn * dot(T.xyz, Nn));
float3 Aa = cross(Nn, Tt);
float3 G = (Tt * dot(gq.xy + gw.xy, aroundQ) + Aa * (gq.z * ALONG + gw.z)) * (1.0 - 0.6 * moss);
NormalW = normalize(Nn - G);
return col;
