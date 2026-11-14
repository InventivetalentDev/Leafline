// M_Bird (the bird, Saturday 14 Nov). Source of truth for two Custom nodes; the main session pastes each block into
// its own Custom node. Mesh: SM_Bird from Tools/blender/make_bird.py (Art/Blender/README.md, "Bird mesh").
// Material: opaque, default lit, two-sided off (the wings and tail are closed plates).
//
// Wiring (each node's Code is the lines between its BEGIN and END markers)
//   BirdColour: Code = NODE 1.  Inputs UV0 = TexCoord[0], UV1 = TexCoord[1].  Output type float3.
//               Return value -> Base Colour.  Additional output Rough (float) -> Roughness.  Specular default.
//   BirdFlap:   Code = NODE 2.  Inputs LP = Local Position (Pre-Skinned Local Position: the vertex in mesh space,
//               cm), UV0 = TexCoord[0], FlapPhase and FlapAmount = scalar parameters of the same names.
//               Output type float3: an offset in MESH (local) space.
//               BirdFlap -> TransformVector (Local Space -> World Space) -> World Position Offset.
//               The transform applies the actor's rotation and scale (BirdScale), so the flap scales with the bird.
//   Parameters set from C++ through a dynamic material instance, like the ant's GaitPhase / GaitAmount:
//               FlapPhase (radians, any value; one wing beat per 2 pi), FlapAmount (0 gliding .. 1 full flapping).
//               FlapAmount 0 gives exactly the modelled pose (the offset is multiplied by zero).
//
// Mesh data (values as authored; Unreal flips V on import, so both nodes read 1 - V):
//   UV0  U 0 body, head, beak, eyes, tail; 0.25 left wing; 0.75 right wing
//        V spanwise distance from the shoulder line, 0 there (and inside the body) to 1 at the wing tip; 0 off the wings
//   UV1  U part id: 0 body, 0.2 head, 0.4 beak, 0.6 wing, 0.8 tail, 1.0 eye
//        V body and head: 0 back to 1 belly; wings and tail: 0 upper skin, 1 under skin; beak and eyes: 0
// Mesh space (after import, cm): +X forward, +Z up, 100 cm wingspan. The shoulder lines run along X at |Y| = 5 cm,
// Z = 0, and the wings lie flat on Z = 0, so each wing bends about its shoulder line. The left wing (Blender +Y) is
// at -Y in Unreal (the import's handedness flip); NODE 2 takes the side from the sign of LP.y, so that does not matter.
//
// After building: the flapping wings leave the mesh's bounds (the tips rise and fall about 32 cm in mesh space).
// Give SM_Bird a Positive and a Negative Bounds Extension of about Z 35 (or the component a BoundsScale of 2), or the
// bird can be culled or lose its shadow at the edge of the screen while it flaps.

// ==================================================================== NODE 1: BirdColour
// Plain brown-grey songbird: darker back and wings, lighter belly and under-wing, dark primaries, dark beak and eye.
// No species markings.
// ---- BEGIN NODE 1 (paste from the next line) ----
const float3 BACK = float3(0.105, 0.085, 0.065);
const float3 HEAD = float3(0.095, 0.085, 0.075);
const float3 BELLY = float3(0.36, 0.32, 0.27);
const float3 WING_TOP = float3(0.085, 0.070, 0.055);
const float3 WING_UNDER = float3(0.24, 0.22, 0.19);
const float3 TIPS = float3(0.035, 0.030, 0.026);      // primaries, both skins
const float3 TAIL_TOP = float3(0.075, 0.062, 0.050);
const float3 TAIL_UNDER = float3(0.16, 0.145, 0.125);
const float3 BEAK = float3(0.05, 0.045, 0.04);
const float3 EYE = float3(0.008, 0.007, 0.006);

int part = (int)round(UV1.x * 5.0);       // 0 body, 1 head, 2 beak, 3 wing, 4 tail, 5 eye
float under = saturate(1.0 - UV1.y);      // belly (body, head) or under skin (wing, tail)
float span = saturate(1.0 - UV0.y);       // 0 shoulder .. 1 wing tip
float3 col = lerp(BACK, BELLY, under);
Rough = 0.75;
if (part == 1) col = lerp(HEAD, BELLY * 0.9, under);
else if (part == 2) { col = BEAK; Rough = 0.45; }
else if (part == 3) col = lerp(lerp(WING_TOP, WING_UNDER, under), TIPS, smoothstep(0.62, 0.88, span));
else if (part == 4) col = lerp(TAIL_TOP, TAIL_UNDER, under);
else if (part == 5) { col = EYE; Rough = 0.15; }
return col;
// ---- END NODE 1 ----

// ==================================================================== NODE 2: BirdFlap
// Each wing bends about its shoulder line. The angle of the wing's middle plane grows linearly from the shoulder
// (a0) to the tip (a0 + a1), so the wing curves rather than hinging: AMP at the shoulder, AMP * (1 + TIP_GAIN) at the
// tip, the hand a little behind the arm (TIP_LAG). Positions along the curve are the exact integral of that angle;
// points above or below the middle plane keep their distance from it. Same maths as flap_offset() in make_bird.py.
// ---- BEGIN NODE 2 (paste from the next line) ----
const float AMP = 0.61;          // radians: 35 degrees at the shoulder at FlapAmount 1
const float TIP_GAIN = 0.7;      // the tip turns 70% further (about 60 degrees)
const float TIP_LAG = 0.5;       // radians the hand lags behind the arm
const float WING_LEN = 45.0;     // cm in mesh space: shoulder line to wing tip
const float SHOULDER_Z = 0.0;    // cm in mesh space: height of the shoulder line

float wing = step(0.1, UV0.x);
float v = saturate(1.0 - UV0.y);
float side = LP.y >= 0.0 ? 1.0 : -1.0;
float A = FlapAmount * AMP;
float a0 = A * sin(FlapPhase);
float a1 = A * TIP_GAIN * sin(FlapPhase - TIP_LAG);
float phi = a0 + a1 * v;
float2 C;                        // the middle plane's point (along the span, up) after bending
if (abs(a1) > 1e-3)
{
	C = WING_LEN * float2(sin(phi) - sin(a0), cos(a0) - cos(phi)) / a1;
}
else
{
	float m = a0 + 0.5 * a1 * v;
	C = WING_LEN * v * float2(cos(m), sin(m));
}
float h = LP.z - SHOULDER_Z;
float dSpan = C.x - h * sin(phi) - WING_LEN * v;
float dUp = C.y + h * cos(phi) - h;
float on = wing * step(1e-5, FlapAmount);
return float3(0.0, side * dSpan, dUp) * on;
// ---- END NODE 2 ----
