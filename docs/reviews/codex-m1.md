# Codex review: M0/M1 code (2026-11-12)

Codex reviewed all of `Source/AntTreeGame` read-only (static review plus numerical reproductions, no compile). Its sandbox could not write files, so the findings were copied here by the main session, condensed, with what was done.

| # | Severity | Finding | Resolution |
|---|---|---|---|
| 1 | Medium | `AntMovementComponent` `ComputeWish`: adding projected camera-up to camera-forward reverses W and D when the camera looks up at more than 45° pitch (camera below the ant), and D can move screen-left on a twig's underside. | Camera-up is weighted by how much the camera faces the surface (zero when it is behind the surface plane); Right is flipped if it disagrees with screen right. |
| 2 | Medium | `AntCamera`: spring plus push-out can balance the focus on the wrong side of a branch after a teleport or a switch across the trunk. | If `March` finds wood between the ant and the focus, the focus is pulled to the ant's side first. |
| 3 | Medium | `TreeField::March` returned "clear" after 64 iterations even when it had covered a fraction of the segment, so grazing rays could pass through wood. | 96 iterations; running out now reports a hit where the trace stopped. |
| 4 | Medium | Tapered branch distance overestimates clearance (root flare slope about 0.67), so the 0.9 sphere-tracing factor could overshoot into wood. | Step factor 0.6. The zero level set still matches the mesh; only distances away from the surface are approximate. |
| 5 | Medium | Failed projections were committed as positions (for example a gradient-free point between two thin branches). | A failed substep restores the last good position and halves speed; placement nudges off a symmetric point and retries, and logs a warning if it still fails. |
| 6 | Medium | The 50 ms frame clamp slowed movement, stamina and camera below 20 fps. | Clamp raised to 100 ms; substeps already keep long frames safe. |
| 7 | Low | Culling with a fixed 3k margin does not preserve the ordered polynomial smooth union exactly (demonstrated error 0.003 cm). | Accepted; documented here. |

No packaged-build blockers were found in dependencies, object ownership or asset references.
