// M_Grass (the forest floor's grass blades, Saturday 14 Nov). Source of truth for ONE Custom node, "GrassSway". Matte,
// opaque, default lit, ONE-SIDED (the floor emits both windings of every blade as separate vertices, so no two-sided
// shading is needed). Built by Tools/materials/build_grass_mcp.py. Mesh and parameters: Source/AntTreeGame/World/
// ForestFloor.cpp (AddBlade, FBuf), wind: Source/AntTreeGame/World/WindSystem.cpp (MPC_Wind).
//
// Wiring (Code = the lines between BEGIN and END)
//   Inputs   UV0 = TexCoord[0]: x the height fraction along the blade (0 at the foot, 1 at the tip), y a random per-blade
//                  phase 0..1 (so blades do not sway in step)
//            UV1 = TexCoord[1]: x the blade's height in METRES (0.8 to 1.7 near the clearing, up to about 7 for the far
//                  tufts)
//            Wind = MPC_Wind WindDirection (vector: the direction the wind blows TOWARD, a world-space unit vector; only
//                  x and y are used)
//            Strength = MPC_Wind WindStrength (scalar 0..1: 0.12 calm, rising to the gust peak)
//            Time = Time node (seconds)
//            Color = vector parameter 'Color' (the floor sets it on the dynamic instance, like BasicShapeMaterial's)
//   Output   float3 = an offset in WORLD space (cm) -> World Position Offset directly (the floor's meshes sit at the world
//            origin with identity transform, vertices are world positions). Additional output Col (float3) -> Base Colour.
//   Roughness constant 1, Specular constant 0.
// Component bounds: the floor sets BoundsScale 1.6 on its meshes, so swaying blades are not culled at the screen edge.
//
// Motion: the blade bends downwind like a cantilever (offset ~ s^2), a steady lean that grows with the wind plus a flutter
// of two out-of-step sines whose speed and size also grow with it; the tip drops as it leans, so the blade does not
// stretch. In the calm (0.12) the tip moves about 5 to 7 percent of the blade height; at a gust peak (1) it leans about a
// third of the blade height and flutters faster.

// ---- BEGIN NODE 1 (paste from the next line) ----
float s = saturate(UV0.x);
float h = max(UV1.x * 100.0, 1.0);                    // blade height, cm
float w = saturate(Strength);
float2 d = Wind.xy;
float len = length(d);
d = len > 0.001 ? d / len : float2(1.0, 0.0);
float ph = UV0.y * 6.2831853;
float lean = (0.02 + 0.30 * w) * h;
float fl = (0.02 + 0.10 * w) * h
         * (0.6 * sin(Time * (1.3 + 2.2 * w) + ph) + 0.4 * sin(Time * (2.9 + 3.5 * w) + ph * 2.3 + 1.7));
float bend = s * s;
float2 side = float2(-d.y, d.x);
float2 off = (d * (lean + fl) + side * (fl * 0.35)) * bend;
Col = Color.rgb * lerp(0.55, 1.15, s);                // darker at the foot, a little lighter at the tip
return float3(off, -dot(off, off) / max(2.0 * h, 1.0));
// ---- END NODE 1 ----
