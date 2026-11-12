# Brief: the ant mesh

A self-contained job for a parallel agent. It needs Blender 5.1 running with the MCP add-on enabled (`blender-mcp` in `.mcp.json`). It does not touch the Unreal editor.

## Goal

A leafcutter worker ant as one static mesh, generated entirely by a Python script, with the data the Unreal material needs to animate the legs. No rig, no textures.

## Rules

- Write the generator script from scratch during this jam. Do not look at or reuse anything from `D:\UnrealProjects\TreeAntGame` or any other project.
- The script is the source of truth. Commit it; anyone must be able to regenerate the mesh by running it.
- You deliver files. The main session imports them into Unreal.

## Deliverables

| File | Content |
|---|---|
| `Tools/blender/make_ant.py` | The generator. Parameters at the top. Running it in an empty scene builds the ant and exports the FBX. |
| `Art/Blender/SM_Ant.fbx` | The export |
| `Art/Blender/Ant.blend` | The saved scene |
| `Art/Blender/ant_preview.png` | A render from three-quarter view and one from the side |
| `Art/Blender/README.md` | The UV channel layout below, as built, plus the mandible attach point in mesh coordinates |

## The ant

Reference: a leafcutter worker (*Atta*), and the ant in `Art/Concept/Codex/2026-11-12/initial/01-tree-ascent.png`.

- Body length **20 cm**, forward along +X, up +Z, origin on the ground plane under the middle of the thorax. Feet touch Z = 0.
- Parts: a large heart-shaped head, elbowed antennae, open mandibles, a thorax with small dorsal spines, a narrow two-segment waist, an oval gaster, six legs in three pairs from the thorax, each with clear femur, tibia and tarsus segments.
- Readable from 2–3 metres behind and above, which is where the game camera sits. Favour a strong silhouette over fine detail.
- About 6–12 thousand triangles. Smooth shading, no degenerate faces, capped tube ends.
- No human features of any kind.

## Data for the shader

Vertex colours do not survive the FBX route into Unreal reliably, so everything travels in UV channels.

| Channel | U | V |
|---|---|---|
| UV0 | Leg group: 0 for body parts; 0.25 for tripod A (front-left, middle-right, rear-left); 0.75 for tripod B (front-right, middle-left, rear-right) | Distance along the leg from hip (0) to foot (1); 0 on the body |
| UV1 | Part id: 0 body, 0.2 head, 0.4 antennae, 0.6 mandibles, 0.8 legs | Side: 0 left, 1 right |
| UV2 | Leg pair: 0 front, 0.5 middle, 1 rear | Unused, 0 |

Unreal flips V on FBX import. Note in the README which values the material must invert.

The main session's material will swing each leg forward and back and lift it during the swing, using UV0 and a gait phase from C++. Legs must therefore be modelled in a neutral mid-stride pose, each leg's hip clearly at the body.

## Checks before you finish

- Re-run the script in an empty scene and confirm it reproduces the same mesh.
- Open the FBX in a fresh scene and confirm the three UV channels and their values survive export.
- Look at your own preview renders and fix anything that does not read as an ant.
- Report triangle count, bounding box, and the mandible attach point.
