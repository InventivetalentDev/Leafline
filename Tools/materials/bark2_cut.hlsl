// M_TreeBark2, Custom node "Cut": the camera cutaway and buried bark (opacity mask outside the shadow pass).
// Inputs: WP, Cam, Ant (MPC_View CamPos, AntFocus), R (CutawayRadius), On (CutawayOn), Band (CutawayBand, rim width as
// a fraction of the radius), Taper (CutawayTaper, narrowing toward the ant), NearA/NearB (NearFadeStart/End),
// VC (the vertex colour's alpha output; below 0.44 is bark buried inside another branch, see TreeMeshBuilder.h).
float3 c = Cam.xyz;
float3 d = Ant.xyz - c;
float t = saturate(dot(WP.xyz - c, d) / (dot(d, d) + 1.0));
float r = R * pow(saturate(1.0 - t), Taper);
float cut = smoothstep(r * (1.0 - Band), r + 0.5, distance(WP.xyz, c + d * t));
float nearCam = smoothstep(NearA, NearB, distance(WP.xyz, c));
float keep = step(0.44, VC);
return lerp(1.0, min(cut, nearCam), saturate(On)) * keep;
