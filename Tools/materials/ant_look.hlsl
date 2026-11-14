// M_Ant2 (the ant's look, Saturday 14 Nov). Source of truth for ONE Custom node, "AntLook". M_Ant2 is a duplicate of
// M_Ant: its World Position Offset chain (GaitPhase, GaitAmount) is untouched; only Base Colour, Roughness and Emissive
// are fed from this node. Built by Tools/materials/build_ant2_mcp.py. Brief: docs/briefs/ant-and-ground.md.
//
// Wiring (Code = the lines between BEGIN and END)
//   Inputs   UV0 = TexCoord[0], UV1 = TexCoord[1], LP = Local Position (LocalOrigin Primitive, ExcludeOffsets: the mesh
//            vertex in cm, so the gait does not shift the gaster mask), Fres = Fresnel (default exponent 5),
//            HeadThoraxColor, GasterColor, LegColor = vector parameters (rgb), Roughness, Fill, RimStrength = scalar
//            parameters.
//   Output   float3 -> Base Colour.  Additional outputs: Rough (float) -> Roughness, Emit (float3) -> Emissive Colour.
//   Parameters are set from AAntPawn on the dynamic instance (FAntLookTuning): defaults here are the A look.
//
// Mesh data (Art/Blender/README.md "Ant mesh"; Unreal flips V on import):
//   UV1.x part id: 0 body (thorax, waist, gaster, neck), 0.2 head, 0.4 antennae, 0.6 mandibles, 0.8 legs
//   UV0.x leg group (0 body, 0.25 tripod A, 0.75 tripod B); UV0.y leg distance. As authored hip is 0 .. foot 1 and Unreal
//   flips V on import, so the material sees 1 - V: the node reads leg distance as saturate(1 - UV0.y) (README).
//   LP.x in cm: nose +10.3, head centre +5.8, thorax -3 .. +3.6, waist (petiole, postpetiole) -4.2 .. -3.3, gaster
//   centre -7.0, tip -9.7. No gaster part id, so the gaster is masked by LP.x.
//
// Fill: emissive = colour * Fill (a brightness floor in deep shade; keep Fill small, under the bloom threshold).
// Rim: warm Fresnel edge scaled by RimStrength; 0 = off (the new default, the owner dislikes the selection-highlight look).

// ---- BEGIN NODE 1 (paste from the next line) ----
int part = (int)round(UV1.x * 5.0);                  // 0 body, 1 head, 2 antennae, 3 mandibles, 4 legs
float gaster = 1.0 - smoothstep(-5.6, -4.8, LP.x);   // 1 behind the waist, 0 on the thorax
float3 col = lerp(HeadThoraxColor, GasterColor, gaster);
float rough = Roughness;
// broad soft highlight on the big rear segment: a little glossier than the rest (still not a tight sparkle)
rough = lerp(rough, max(rough - 0.10, 0.18), gaster);
if (part == 4)
{
    float leg = saturate(1.0 - UV0.y);               // 0 hip .. 1 foot
    col = LegColor * lerp(1.08, 0.88, leg);           // slightly lighter at the hip, never darker than the body
    rough = Roughness + 0.12;
}
else if (part == 2) { col = LegColor * 0.9; rough = Roughness + 0.12; }
else if (part == 3) { col = HeadThoraxColor * 0.7; rough = Roughness + 0.05; }
float3 rim = float3(0.9, 0.6, 0.3) * Fres * RimStrength;
Rough = saturate(rough);
Emit = col * Fill + rim;
return col;
// ---- END NODE 1 ----
