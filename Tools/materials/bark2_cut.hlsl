// M_TreeBark2, Custom node "Cut": the camera cutaway and buried bark (opacity mask outside the shadow pass).
// Inputs: WP, Cam, Ant (MPC_View CamPos, AntFocus), R (CutawayRadius), On (CutawayOn), Band (CutawayBand, rim width as
// a fraction of the radius), Taper (CutawayTaper, narrowing toward the ant), NearA/NearB (NearFadeStart/End),
// VC (UV3: y is the signed distance in cm to the branch this bark overlaps, negative inside; see TreeMeshBuilder.h),
// Depth (MPC_View BuriedDepth), Cut (MPC_View BarkCut: 0 when no wood lies between camera and ant, 1 when it does;
// scales only the cutaway radius, so the near-camera fade keeps working), Floor (MPC_View CutawayFloor: how much of the
// cut-away bark stays as a dither, a faint see-through layer, so the ant never floats in an empty hole; owner, 13 Nov).
float3 c = Cam.xyz;
float3 d = Ant.xyz - c;
float t = saturate(dot(WP.xyz - c, d) / (dot(d, d) + 1.0));
float r = R * saturate(Cut) * pow(saturate(1.0 - t), Taper);
float cut = smoothstep(r * (1.0 - Band), r + 0.5, distance(WP.xyz, c + d * t));
float nearCam = smoothstep(NearA, NearB, distance(WP.xyz, c));
float keep = step(-Depth, VC.y); // hidden when more than Depth (MPC_View BuriedDepth, cm) inside another branch
float cutA = lerp(saturate(Floor), 1.0, cut);
return lerp(1.0, min(cutA, nearCam), saturate(On)) * keep;
