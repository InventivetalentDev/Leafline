// M_Ground2, Custom node "GroundFar" (floor pass, 14 Nov, docs/briefs/floor-from-above.md steps 2 and 4a). Source of truth
// for the node's Code; Tools/materials/build_ground2_mcp.py builds M_Ground2 as a DUPLICATE of M_Ground and puts this node
// between M_Ground's own colour, roughness and normal and the material outputs, so the near look is M_Ground's exactly.
// Used on the ground rings (ATreeActor::BuildGround, wood mesh section 1: UV0 = world XY, normal up, tangent +X, so
// tangent space is world space on the plane; beyond 160 m the rings follow LeafRelief and carry its normals).
//
// Inputs
//   NearCol   float3  what fed M_Ground's Base Colour
//   NearRough float   what fed its Roughness (unconnected: the script feeds a constant 0.9)
//   NearN     float3  what fed its Normal, tangent space (unconnected: a constant (0,0,1))
//   WP        float3  Absolute World Position (cm)
//   Cam       float3  Camera Position WS (cm)
//   FarStart  scalar parameter, default 1800: distance (cm) from the camera where the far look starts (weight exactly 0
//             nearer, so the nest view keeps M_Ground's look)
//   FarEnd    scalar parameter, default 5000: distance (cm) where it is complete (a wide ramp: no line on a climb)
//   Fleck     scalar parameter, default 110: size (cm) of the leaf flecks' grid (the fallen leaves are about 1 m)
//   FarDark   scalar parameter, default 1.0: brightness of the far look (tuning)
//   Undulate  scalar parameter, default 0.3: steepness of the shading-only undulation (rise per run at its steepest)
//   UndSize   scalar parameter, default 450: width (cm) of the undulation's rises and hollows
// Outputs
//   return    float3  Base Colour
//   Rough     float   -> Roughness
//   Nrm       float3  -> Normal (tangent space)
//
// The far look: a carpet of leaf-shaped flecks (ellipses on a jittered grid, the topmost one wins), each with its own tone
// from the litter palette (Source/AntTreeGame/World/ForestFloor.cpp MatColors: brown, ochre, olive, dark brown), dark soil
// between them, faint variation at 6 and 25 m, no cracks and no cell edges. Its mean is darker and greyer than M_Ground's
// (0.062, 0.071, 0.03, see nest_soil.hlsl GROUND_TONE). Flecks fade to their mean where a pixel covers a quarter of a
// fleck or more (no shimmer from far up or at a grazing angle). The undulation only tilts the normal, fades in with the
// far look and fades out where a pixel covers a fifth of its width. fwidth is 0 outside pixel shaders (Common.ush), which
// gives full detail there.
// ---- BEGIN NODE 1 (paste from the next line) ----
struct FGround2
{
	float H1(float2 p)
	{
		float3 p3 = frac(float3(p.x, p.y, p.x) * 0.1031);
		p3 += dot(p3, p3.yzx + 33.33);
		return frac((p3.x + p3.y) * p3.z);
	}
	float2 H2(float2 p)
	{
		float3 p3 = frac(float3(p.x, p.y, p.x) * float3(0.1031, 0.1030, 0.0973));
		p3 += dot(p3, p3.yzx + 33.33);
		return frac((p3.xx + p3.yz) * p3.zy);
	}
	// Value noise in [0,1] with its gradient: (value, d/dx, d/dy) in cells.
	float3 VND(float2 x)
	{
		float2 i = floor(x);
		float2 f = x - i;
		float2 u = f * f * (3.0 - 2.0 * f);
		float2 du = 6.0 * f * (1.0 - f);
		float a = H1(i);
		float b = H1(i + float2(1.0, 0.0));
		float c = H1(i + float2(0.0, 1.0));
		float d = H1(i + float2(1.0, 1.0));
		float k1 = b - a;
		float k2 = c - a;
		float k4 = a - b - c + d;
		return float3(a + k1 * u.x + k2 * u.y + k4 * u.x * u.y, du.x * (k1 + k4 * u.y), du.y * (k2 + k4 * u.x));
	}
	float3 Tone(float pick)
	{
		// Litter palette, a little darker than the meshes' (the material is lit at full roughness, no curl, no shade).
		if (pick < 0.30) return float3(0.040, 0.029, 0.020);      // brown
		if (pick < 0.55) return float3(0.050, 0.044, 0.024);      // dull tan-ochre
		if (pick < 0.80) return float3(0.037, 0.042, 0.017);      // olive
		return float3(0.033, 0.024, 0.016);                       // dark brown
	}
};

FGround2 GF;
float dist = length(WP - Cam);
float w = smoothstep(FarStart, max(FarEnd, FarStart + 1.0), dist);
float3 nearN = normalize(NearN);
if (w <= 0.0)
{
	Rough = NearRough;
	Nrm = nearN;
	return NearCol;
}

const float3 SOIL = float3(0.026, 0.022, 0.015);
const float3 MEAN = float3(0.036, 0.031, 0.018);   // about 60% leaf cover over SOIL
float fs = max(Fleck, 10.0);
float2 p = WP.xy;
float fw = max(length(fwidth(p)), 0.01);           // cm per pixel

// Leaf flecks: for each grid cell round this one, a leaf (an ellipse 1 to 1.9 cells long, random heading and layer).
float2 q = p / fs;
float2 ci = floor(q);
float best = -1.0;
float pick = 0.0;
float shade = 1.0;
float edge = 2.0;
float soft = clamp(2.5 * fw / fs, 0.04, 1.0);
for (int j = -1; j <= 1; j++)
{
	for (int i = -1; i <= 1; i++)
	{
		float2 c = ci + float2(i, j);
		float2 h = GF.H2(c);
		float2 ctr = c + 0.1 + 0.8 * h;
		float ang = GF.H1(c + 17.3) * 6.2831853;
		float2 ax = float2(cos(ang), sin(ang));
		float2 d = q - ctr;
		float2 l = float2(dot(d, ax), dot(d, float2(-ax.y, ax.x)));
		float hl = 0.5 + 0.45 * h.y;
		float hw = hl * (0.40 + 0.18 * h.x);
		float e = (l.x * l.x) / (hl * hl) + (l.y * l.y) / (hw * hw);
		float layer = GF.H1(c + 41.7);
		if (e < 1.0 && layer > best)
		{
			best = layer;
			pick = GF.H1(c + 5.1);
			shade = 0.8 + 0.4 * GF.H1(c + 9.7);
			edge = e;
		}
	}
}
float3 fcol = SOIL;
if (best >= 0.0)
{
	float inside = saturate((1.0 - edge) / soft);
	// A faint midrib and a slightly darker rim so a leaf reads as a leaf where it is a few pixels wide.
	float3 leaf = GF.Tone(pick) * shade * lerp(0.85, 1.0, saturate((1.0 - edge) * 3.0));
	fcol = lerp(SOIL, leaf, inside);
}
float detail = 1.0 - smoothstep(0.25 * fs, 0.6 * fs, fw);
fcol = lerp(MEAN, fcol, detail);

// Only faint variation at larger sizes (6 m and 25 m), so the pattern has the size of the leaves, not of blotches.
float big = 0.6 * GF.VND(p / 600.0 + 13.1).x + 0.4 * GF.VND(p / 2500.0 + 71.3).x;
fcol *= (0.9 + 0.2 * big) * FarDark;

// Shading-only undulation: rises and hollows a few metres wide, so the low sun shades the floor. No geometry.
float us = max(UndSize, 50.0);
float3 n1 = GF.VND(p / us);
float3 n2 = GF.VND(p / (us * 0.43) + 5.3);
float2 grad = n1.yz + 0.5 * n2.yz;                  // per cell of us
float ufade = (1.0 - smoothstep(0.2 * us, 0.5 * us, fw)) * w;
float3 farN = normalize(float3(-Undulate * grad * ufade, 1.0));

Rough = lerp(NearRough, 0.92, w);
Nrm = normalize(lerp(nearN, farN, w));
return lerp(NearCol, fcol, w);
// ---- END NODE 1 ----
