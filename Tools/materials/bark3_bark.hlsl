// M_TreeBark3, Custom node "Bark" (14 Nov night, branch seams). M_TreeBark2's bark with the junction skirts' seam
// cross-faded. Source of truth for the node's Code; Tools/materials/build_bark3_mcp.py builds M_TreeBark3 as a duplicate
// of M_TreeBark2 with a new Custom node "Bark3" holding this code. Inputs as M_TreeBark2's Bark node: UV1 (R cos, R sin in cm), UV2 (along split
// hi/lo), UV3 (x: bark radius; y: buried distance on the tubes, the other side's along + 512 on skirt vertices near a
// seam), VC (vertex colour RGB: level/3 or the other side's per-branch random, per-branch random, height on tree), WP,
// N, T, Cam; and two more: UV0 (on skirt vertices near a seam: the other side's radial direction times its bark radius)
// and VA (vertex colour alpha: 1 - the seam weight; 1 on the tubes). TreeSkirts.inl writes them with leaf.SkirtBlend 1.
// Returns the base colour; additional outputs Rough, NormalW (world-space normal) and AO, as M_TreeBark2.
// Where the weight is 0 (every tube pixel) this is exactly M_TreeBark2's bark. Near a seam the bark is evaluated twice,
// once with this vertex's own coordinates and once with the other branch's continued across the seam, and the two are
// height-blended: whichever plate is higher shows, biased by the weight, so plates of both barks interleave at the seam
// instead of ghosting. The blend is symmetric, so both halves of a skirt give the same colour on the seam row.
// At the trunk's foot the bark grades into the soil colour over its last 30 to 50 cm above the ground.
struct FBark3
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


	// One evaluation of M_TreeBark2's bark. uv1: (R cos, R sin) for the radius and direction round the branch; uvq: the
	// same in "true proportions" space (qw, colour, moss, grain); along (cm); barkR: the ridge-space radius; vcg: the
	// per-branch random; vcb: height on tree; Tt, Aa: unit directions of increasing angle and along on the surface.
	// Returns the colour; Rough, NormalW, AO; Plate: 1 on a plate, 0 in a fissure (for the height blend).
	float3 Eval(float2 uv1, float2 uvq, float along, float barkR, float vcg, float vcb, float3 Nn, float3 Tt, float3 Aa,
		float fp, out float Rough, out float3 NormalW, out float AO, out float Plate)
	{
		// M_TreeBark2's constants (bark2_bark.hlsl has the reasons for each value).
		const float PLATE = 14.0;
		const float ALONG = 0.13;
		const float FIS_W = 0.3;
		const float FIS_DEPTH = 1.8;
		const float DOME = 1.5;
		const float GRAIN_DEPTH = 0.12;
		const float MOSS_AMOUNT = 1.0;
		const float FISSURE_SHADE = 0.42;
		const float3 PLATE_DARK = float3(0.14, 0.095, 0.062);
		const float3 PLATE_LIGHT = float3(0.32, 0.23, 0.145);
		const float3 YOUNG = float3(0.22, 0.15, 0.10);
		const float3 MOSS_A = float3(0.05, 0.062, 0.03);
		const float3 MOSS_B = float3(0.075, 0.085, 0.042);

		float R = max(length(uv1), 0.001);
		float2 radial = uv1 / R;
		float2 aroundQ = float2(-radial.y, radial.x);
		float Rq = barkR > 0.5 ? barkR : R;
		float aroundScale = Rq / R;
		float3 q = float3(radial * Rq, along * ALONG + vcg * 2000.0);
		float3 qw = float3(uvq, along + vcg * 5000.0);
		float smallness = 1.0 - smoothstep(6.0, 30.0, R);

		float3 p = q / PLATE;
		float3 pwq = p * float3(0.35, 0.35, 0.6);
		float wa = VN(pwq + 11.0).x;
		float wb = VN(pwq + 47.0).x;
		float wc = VN(p * float3(1.6, 1.6, 0.5) + 23.0).x;
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
			float3 r = g + H3(ip + g) - fq;
			float dd = dot(r, r);
			if (dd < f1) { f2 = f1; r2 = r1; f1 = dd; r1 = r; id = H1(ip + g + 17.0); }
			else if (dd < f2) { f2 = dd; r2 = r; }
		}
		f1 = sqrt(f1);
		f2 = sqrt(f2);
		float e = f2 - f1;
		float W = max(FIS_W * (0.6 + 0.8 * wc), fp * 1.5 / PLATE);
		float s = saturate(e / W);
		float fis = 1.0 - s * s * (3.0 - 2.0 * s);
		float dfis = -6.0 * s * (1.0 - s) / W;
		float3 de = r1 / max(f1, 1e-4) - r2 / max(f2, 1e-4);
		float depth = FIS_DEPTH * (1.0 - 0.75 * smallness);
		float reliefFade = 1.0 - smoothstep(1.0, 3.0, fp);
		float3 gq = (-depth * dfis * de + 2.0 * DOME * r1) / PLATE * reliefFade;

		float3 gw = 0;
		float grainAmt = 1.0 - smoothstep(0.06, 0.2, fp);
		float grain = 0.5;
		[branch] if (grainAmt > 0.001)
		{
			float3 sc = float3(1.0 / 0.45, 1.0 / 0.45, 1.0 / 2.2);
			float4 n1 = VN(qw * sc);
			float4 n2 = VN(qw * sc * 2.03 + 19.0);
			grain = n1.x * 0.65 + n2.x * 0.35;
			gw += (n1.yzw * sc * 0.65 + n2.yzw * sc * (2.03 * 0.35)) * (2.0 * GRAIN_DEPTH * grainAmt);
		}
		grain = lerp(0.5, grain, grainAmt);

		float mottle = VN(qw / float3(5.0, 5.0, 9.0) + 3.0).x;
		float macro = VN(qw / 300.0 + 7.0).x;

		float m1 = VN(qw / float3(60.0, 60.0, 110.0) + 5.0).x;
		float m2 = VN(qw / float3(20.0, 20.0, 35.0) + 9.0).x;
		float up = saturate(Nn.z);
		float mossField = m1 * 0.7 + m2 * 0.3 + 0.12 * fis + 0.15 * up + 0.08 * (1.0 - vcb);
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
		float3 G = (Tt * (dot(gq.xy, aroundQ) * aroundScale + dot(gw.xy, aroundQ)) + Aa * (gq.z * ALONG + gw.z)) * (1.0 - 0.6 * moss);
		NormalW = normalize(Nn - G);
		Plate = 1.0 - fis;
		return col;
	}

	// Unit surface direction in which f increases, from screen-space derivatives (for the other branch's coordinates,
	// which have no vertex tangent). Falls back to Fallback where the derivatives vanish.
	float3 GradDir(float fx, float fy, float3 dpx, float3 dpy, float3 Nn, float3 Fallback)
	{
		float3 g = cross(dpy, Nn) * fx + cross(Nn, dpx) * fy;
		g *= dot(Nn, cross(dpx, dpy)) < 0.0 ? -1.0 : 1.0; // divide by the sign of the screen-to-surface determinant
		g -= Nn * dot(g, Nn);
		float l = length(g);
		return l > 1e-8 ? g / l : Fallback;
	}
};
FBark3 B;

// Seam blend tuning (MODE 'code' in build_bark3_mcp.py after editing): the other bark's plate wins over this one's where
// HEIGHT_MIX * (its plate - this plate) + WEIGHT_MIX * (2w - 1) > 0, softened over +-EDGE. HEIGHT_MIX < 1 keeps this
// bark's fissures where the other bark has a plate except close to the seam; at w = 0.5 the higher plate shows.
const float HEIGHT_MIX = 0.35;
const float WEIGHT_MIX = 1.3;
const float EDGE = 0.15;
const float W_OTHER = (WEIGHT_MIX - EDGE - HEIGHT_MIX) / (2.0 * WEIGHT_MIX); // below this weight the other bark never shows

float3 Nn = normalize(N.xyz);
float3 Tv = normalize(T.xyz - Nn * dot(T.xyz, Nn));
float fp = distance(WP.xyz, Cam.xyz) * 0.0008;
float along = UV2.x + UV2.y;
float w = saturate(1.0 - VA); // seam weight: 0 on the tubes and away from a seam, 0.5 on the seam row
// Screen derivatives for the frames near a seam, taken outside any branch (they are undefined across a per-pixel branch).
float3 dpx = ddx(WP.xyz), dpy = ddy(WP.xyz);
float2 u1 = UV1.xy, u0 = UV0.xy;
float2 du1x = ddx(u1), du1y = ddy(u1), du0x = ddx(u0), du0y = ddy(u0);

float3 col, nrm;
float rough, ao, plateA;
float b = 0.0;
[branch] if (w < 0.002)
{
	col = B.Eval(u1, u1, along, UV3.x, VC.g, VC.b, Nn, Tv, cross(Nn, Tv), fp, rough, nrm, ao, plateA);
}
else
{
	// Both evaluations take their "around" direction from derivatives near the seam (blended in from the vertex tangent),
	// "along" as cross(N, around) as M_TreeBark2 does, and the colour space (qw) of this side eases to the ridge-space
	// radius, as the other side's always is, so the two halves compute the same thing on the seam row.
	float k = saturate(2.0 * w);
	float d1 = max(dot(u1, u1), 1e-4);
	float3 TaD = B.GradDir((u1.x * du1x.y - u1.y * du1x.x) / d1, (u1.x * du1y.y - u1.y * du1y.x) / d1, dpx, dpy, Nn, Tv);
	TaD = dot(TaD, Tv) < 0.0 ? -TaD : TaD; // same sense as the vertex tangent (increasing angle)
	float3 Ta = normalize(lerp(Tv, TaD, k));
	float3 Aa = cross(Nn, Ta);
	float R1 = max(length(u1), 0.001);
	float Rq1 = UV3.x > 0.5 ? UV3.x : R1;
	float2 uq = lerp(u1, u1 / R1 * Rq1, k);
	col = B.Eval(u1, uq, along, UV3.x, VC.g, VC.b, Nn, Ta, Aa, fp, rough, nrm, ao, plateA);
	[branch] if (w > W_OTHER)
	{
		float alongB = UV3.y - 512.0;
		float d0 = max(dot(u0, u0), 1e-4);
		float3 TbD = B.GradDir((u0.x * du0x.y - u0.y * du0x.x) / d0, (u0.x * du0y.y - u0.y * du0y.x) / d0, dpx, dpy, Nn, Ta);
		float3 Ab = cross(Nn, TbD);
		float roughB, aoB, plateB;
		float3 nrmB;
		float3 colB = B.Eval(u0, u0, alongB, length(u0), VC.r, VC.b, Nn, TbD, Ab, fp, roughB, nrmB, aoB, plateB);
		b = smoothstep(-EDGE, EDGE, HEIGHT_MIX * (plateB - plateA) + WEIGHT_MIX * (2.0 * w - 1.0));
		col = lerp(col, colB, b);
		rough = lerp(rough, roughB, b);
		ao = lerp(ao, aoB, b);
		nrm = normalize(lerp(nrm, nrmB, b));
	}
}
// Trunk foot: the bark darkens into the soil over its last 30 to 50 cm above the ground plane (world Z 0 near the tree:
// the relief starts 160 m out), at a height that wanders with a slow noise, so the line where bark meets the ground
// (M_Ground2's soil is about 0.03 linear) is not a hard edge. Only bark within reach of the ground is affected.
[branch] if (WP.z < 60.0)
{
	float soilH = 40.0 + 10.0 * (B.VN(float3(WP.xy / 45.0, 61.0)).x * 2.0 - 1.0); // world XY: no step at root-trunk seams
	float soil = (1.0 - smoothstep(0.0, soilH, WP.z)) * 0.8;
	col = lerp(col, float3(0.05, 0.042, 0.027), soil);
	rough = lerp(rough, 0.95, soil);
	ao = lerp(ao, 0.8, soil);
}
Rough = rough;
AO = ao;
NormalW = nrm;
// Debug views: 0 normal; 1 the seam weight (red, 1 on the seam row) and the blend factor (green: the other branch's bark
// showing); 2 the weight only.
const int DEBUG_VIEW = 0;
if (DEBUG_VIEW == 1) { NormalW = Nn; return float3(w * 2.0, b, 0.0); }
if (DEBUG_VIEW == 2) { NormalW = Nn; return (w * 2.0).xxx; }
return col;
