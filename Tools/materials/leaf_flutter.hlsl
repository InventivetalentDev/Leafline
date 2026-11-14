// M_Leaf, Custom node "LeafWind" (cues branch, 14 Nov; optional, only if the stronger sway alone does not read as
// flutter). Source of truth for the node's Code; the main session pastes it in over unreal-mcp. It replaces the World
// Position Offset chain dumped in Saved/m_leaf_wpo_dump.txt (VertexNormalWS x 20 x WindStrength x U0^2 x
// sin(Time x 3.1 + dot(WorldPos.xy, (0.006, 0)))) and keeps that sway exactly, adding a quick shiver that only shows in
// a gust.
// Inputs (all float unless noted):
//   N   VertexNormalWS (float3)
//   WP  Absolute World Position (float3, cm)
//   U   TextureCoordinate 0, R (0 at the stalk, 1 at the tip)
//   T   Time
//   S   CollectionParameter MPC_Wind.WindStrength (the cues write up to about 3 in a full gust, 0.12 in calm)
// Output: float3, wire to World Position Offset.
// Nothing else changes: no new MPC parameter, the stalk end stays put (U^2), calm looks as before (the shiver scales
// with saturate(S)^2: 0.1 cm in calm, 8 cm in a gust).
float tip = U * U;
// The dump lists only R = 0.006 for Constant2Vector_0, so G is taken as its default 0; if the editor shows another G,
// add WP.y * G here, or the sway's phase changes.
float sway = 20.0 * S * tip * sin(T * 3.1 + WP.x * 0.006);
// Phase per leaf from its position, so neighbours shiver out of step (the sway's phase changes only every few metres).
float ph = WP.x * 0.031 + WP.y * 0.047 + WP.z * 0.013;
float g = saturate(S);
float shiver = 8.0 * g * g * tip * (0.7 * sin(T * 13.0 + ph) + 0.3 * sin(T * 21.0 + 1.7 * ph));
return N * (sway + shiver);
