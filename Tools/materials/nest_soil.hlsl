// M_NestSoil, Custom node "NestSoil" (look pass 2, 13 Nov). Source of truth for the node's Code; the main session pastes
// it in over unreal-mcp. Material: opaque, default lit, used on slot 0 of SM_Nest (Tools/blender/make_nest.py).
// Inputs: VC (vertex colour, float4), WP (absolute world position, cm), N (vertex normal WS).
// Returns the base colour; additional outputs Rough, AO and Spec (float each). Wire Spec to Specular: it drops to 0 in
// the entrance, so the black throat has no sheen under the low day-start camera.
// Vertex colour (Art/Blender/README.md):
//   R soil cover: 1 inside a wandering radius of about 105..122 cm, linear to 0 at the skirt's irregular outline
//     (1 on pellets and clumps). Its bays reach onto the raised mound, so GROUND_TONE shows on the mound's foot as well
//     as on the skirt: match it to M_Ground or the bays read as a different soil.
//   G entrance: 1 inside 0.45 x the entrance radius, linear to 0 at 1.05 x (pellets carry the value at their centre)
//   B 0 on the mound surface, 1 on pellets and clumps
//   A on the surface: loose spoil near pellets and clumps, 0..1; on pellets and clumps: a random value per clump
// Gradients are stored linear and shaped here; masks are hard 0/1. If the ramps look too wide or too narrow in the
// editor, the import converted the colours (sRGB); adjust the smoothstep edges rather than the mesh.
// Nothing here may change the silhouette: the mesh follows the walking field, so the look is colour, AO and roughness.
struct FNestSoil
{
	float H1(float3 p)
	{
		p = frac(p * 0.1031);
		p += dot(p, p.zyx + 31.32);
		return frac((p.x + p.y) * p.z);
	}
	// Value noise in [0,1].
	float VN(float3 x)
	{
		float3 i = floor(x);
		float3 f = x - i;
		float3 u = f * f * (3.0 - 2.0 * f);
		float a = H1(i);
		float b = H1(i + float3(1, 0, 0));
		float c = H1(i + float3(0, 1, 0));
		float d = H1(i + float3(1, 1, 0));
		float e = H1(i + float3(0, 0, 1));
		float g = H1(i + float3(1, 0, 1));
		float h = H1(i + float3(0, 1, 1));
		float k = H1(i + float3(1, 1, 1));
		return lerp(lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y), lerp(lerp(e, g, u.x), lerp(h, k, u.x), u.y), u.z);
	}
};
FNestSoil S;

// Scale (cm)
const float GRAIN = 0.8;        // finest grain wavelength; raise it if the soil shimmers from far away
const float PATCH = 18.0;       // damp and dry patches
// Colour (linear). Fresh soil is darker and grainier than the ground; pellets are drier and vary per pellet.
const float3 SOIL_DARK = float3(0.045, 0.030, 0.019);
const float3 SOIL_LIGHT = float3(0.105, 0.070, 0.044);
const float3 CRUMB = float3(0.16, 0.11, 0.068);
const float3 PELLET_A = float3(0.07, 0.045, 0.028);
const float3 PELLET_B = float3(0.135, 0.088, 0.054);
const float3 GROUND_TONE = float3(0.062, 0.071, 0.03); // where the soil thins out: M_Ground's mean base colour (its two tones' mean)
const float3 THROAT = float3(0.004, 0.003, 0.002);

float cover = VC.r;
float ent = VC.g;
float isPellet = step(0.5, VC.b);
float spoil = VC.a * (1.0 - isPellet);
float rnd = VC.a * isPellet;
float3 Nn = normalize(N.xyz);
float3 P = WP.xyz;

// Grain: two octaves near a centimetre, sparse lighter crumbs, slow damp/dry patches.
float g1 = S.VN(P / GRAIN);
float g2 = S.VN(P / (GRAIN * 2.3) + 17.0);
float grain = g1 * 0.55 + g2 * 0.45;
float crumbs = smoothstep(0.70, 0.80, S.VN(P / (GRAIN * 1.4) + 41.0));
float patch = S.VN(P / PATCH + 3.0) * 0.6 + S.VN(P / (PATCH * 3.0) + 9.0) * 0.4;

float3 soil = lerp(SOIL_DARK, SOIL_LIGHT, saturate(grain * 0.9 + (patch - 0.5) * 0.7));
// Fresh spoil round the lip and the edge piles: drier, lighter, more crumbs.
soil *= 1.0 + 0.25 * spoil;
soil = lerp(soil, CRUMB * (0.8 + 0.4 * g2), crumbs * (0.3 + 0.5 * spoil));

// Pellets: their own tone each, dry tops a little lighter.
float3 pel = lerp(PELLET_A, PELLET_B, rnd) * (0.8 + 0.35 * g1) * (0.8 + 0.3 * saturate(Nn.z));
float3 col = lerp(soil, pel, isPellet);

// The edge: soil thins out over the ground in crumbs, not along a line. Exactly 1 wherever R is 1.
float edgeN = S.VN(P / 7.0 + 31.0) * 0.6 + S.VN(P / 2.2 + 57.0) * 0.4;
float t = 0.1 + 0.8 * edgeN;
float soilAmt = max(smoothstep(t - 0.1, t + 0.1, cover), isPellet);
col = lerp(GROUND_TONE * (0.85 + 0.3 * grain), col, soilAmt);

// The entrance: damp, darker soil going in, then the throat to black with a ragged edge. With the mesh's 16 cm mean
// entrance radius the throat is black inside about 11 cm and half black at about 13 cm (the ant's feet reach about 9 cm
// from its centre), the damp ring reaches about 17 cm, and the pellets start at 13 cm.
float entN = S.VN(P / 2.5 + 71.0);
float damp = smoothstep(0.0, 0.4, ent);
float throat = smoothstep(0.15, 0.6, ent + (entN - 0.5) * 0.3);
col *= 1.0 - 0.45 * damp;
col = lerp(col, THROAT, throat);

float ao = lerp(0.85, 1.0, grain);
ao *= 1.0 - 0.25 * spoil;                                  // crevices between pellets on the surface
ao *= lerp(1.0, lerp(0.6, 1.0, saturate(Nn.z)), isPellet); // pellet undersides
ao *= 1.0 - 0.9 * throat;
AO = ao;

Rough = lerp(lerp(0.92 + 0.06 * grain, 0.96, isPellet), 0.78, damp * 0.6 * (1.0 - throat));
Spec = 0.5 * (1.0 - throat) * (1.0 - 0.3 * damp);
return col;
