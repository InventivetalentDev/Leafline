// The forest floor's grass blades (Saturday 14 Nov, v2 at 18:30). Source of truth for the Custom node "GrassSway" in two
// versions, between BEGIN/END markers (Tools/materials/build_grass_mcp.py picks them by number):
//   NODE 1  the in-place upgrade of M_Grass: same inputs, same outputs (main, Col). Darker foot (brief floor-from-above,
//           step 3), tips a touch yellower. No transmission.
//   NODE 2  M_Grass2 (a new material, build_grass_mcp.py MODE 'build2'): NODE 1's colour plus two more outputs, Sub and
//           Emit, for the backlit near-black blades: the material uses the Two Sided Foliage shading model with Sub as its
//           Subsurface Color (light shining through the blade from a low sun) and a small emissive floor so a blade in
//           shade or seen against the sun is never black. ForestFloor loads M_Grass2, then M_Grass, then M_Litter.
// Mesh and parameters: Source/AntTreeGame/World/ForestFloor.cpp (AddBlade, FBuf), wind: WindSystem.cpp (MPC_Wind).
// Matte, opaque, ONE-SIDED (the floor emits both windings of every blade as separate vertices).
//
// Inputs (both nodes; unchanged since M_Grass)
//   UV0 = TexCoord[0]: x the height fraction along the blade (0 at the foot, 1 at the tip), y a random per-blade phase 0..1
//   UV1 = TexCoord[1]: x the blade's height in METRES
//   Wind = MPC_Wind WindDirection (the direction the wind blows TOWARD, world space; x and y used)
//   Strength = MPC_Wind WindStrength (0..1, 0.12 calm, up to the gust peak)
//   Time = Time node (seconds)
//   Color = vector parameter 'Color' (ForestFloor sets it on the dynamic instance)
// Outputs: main float3 = world-space offset (cm) -> World Position Offset. Additional Col (float3) -> Base Colour.
//   NODE 2 only: Sub (float3) -> Subsurface Color, Emit (float3) -> Emissive Colour.
// Component bounds: the floor sets BoundsScale 1.6 on its meshes, so swaying blades are not culled at the screen edge.
//
// Motion: the blade bends downwind like a cantilever (offset ~ s^2), a steady lean that grows with the wind plus a flutter
// of two out-of-step sines; the tip drops as it leans. Calm (0.12): the tip moves 5 to 7 percent of the blade height; at a
// gust peak (1) it leans about a third of the height and flutters faster.
//
// Colour: Color is a dark olive-green (0.028, 0.070, 0.032). Foot factor 0.22 .. tip 1.2 (smoothstep over the lower 90
// percent), so seen from above or from the side each blade has depth: a dark root, a brighter tip.

// ==================================================================== NODE 1: GrassSway for M_Grass (in place)
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
float ao = lerp(0.22, 1.2, smoothstep(0.0, 0.9, s));  // dark foot, bright tip
Col = Color.rgb * ao * float3(1.0, 1.0 + 0.25 * s, 1.0 - 0.25 * s);   // tips a touch yellower
return float3(off, -dot(off, off) / max(2.0 * h, 1.0));
// ---- END NODE 1 ----

// ==================================================================== NODE 2: GrassSway for M_Grass2
// Additional outputs: Col, Sub, Emit.
// ---- BEGIN NODE 2 (paste from the next line) ----
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
float ao = lerp(0.22, 1.2, smoothstep(0.0, 0.9, s));  // dark foot, bright tip
Col = Color.rgb * ao * float3(1.0, 1.0 + 0.25 * s, 1.0 - 0.25 * s);
// Light shining through the blade (Two Sided Foliage): a bright yellow-green that grows toward the tip, so a blade against a
// low sun glows instead of going black. The foot stays dark.
Sub = float3(0.10, 0.22, 0.04) * (0.25 + 0.75 * s) * ao;
// A small emissive floor (linear 0.004 at the foot, 0.034 at the tip): shade is dark green, never black.
Emit = float3(0.014, 0.034, 0.012) * (0.12 + 0.88 * s);
return float3(off, -dot(off, off) / max(2.0 * h, 1.0));
// ---- END NODE 2 ----
