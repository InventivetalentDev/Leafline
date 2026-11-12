- [x] I'm still climbing through the air at the trunk base
  - Fixed: the walking surface now matches the visible bark (mean gap 0.12 cm around the trunk base and roots, was 52 cm; worst 7.5 cm in the small blend where roots meet the trunk). In the editor build.
- [x] "safe" text when cutting leaves transitions to weird informational text about it being dangerous in wind which just seems confusing
  - Fixed: the label under the cut arc names what it is about: "This piece: safe in a gust", "This piece: risky in a gust" or "This piece: too big for a gust" (the wind risk of the piece being cut, with your Grip upgrade). In the editor build.
- [x] pretty blurry UI text
  - Fixed: all HUD text is rasterised at its on-screen size instead of being stretched. In the editor build.
- [x] tree texture has some UV issues; weird lines and stretched textures in places
  - Fixed: new bark material without UV seams; fissures run along each branch. The root flare reads more like plates than fissures, and some fissures are thin hairlines. The look needs your verdict. In the editor build.
- [x] stuck in pause screen, none of the supposed resume buttons did anything (neither did the quit buttons)
  - Fixed: the HUD now ticks while the game is paused, so the buttons respond. In the current packaged build.
- [x] potentially allow the camera to clip through the tree so moving around e.g. branches is less jarring, but make it seem a bit more intentional by dithering the material when the camera gets close
  - Done: the camera no longer pulls in for wood; bark between the camera and the ant (and right in front of the camera) dithers away. Leaves are not dithered yet. The old behaviour is the camera setting `bAvoidThickOcclusion`. Only seen in still screenshots so far; how it feels in motion needs your verdict. In the editor build.
- [x] ant starts inside the nest mesh at the start (no colission?)
  - Fixed: the nest mound is part of the walking surface, so ants start on top of the entrance and walk over the mound. In the editor build.
- [x] can't walk around around branches anymore? all of WASD just seem to walk up and down the branch; also makes it hard to navigate to even smaller branches
  - Fixed: on thin branches, clearly sideways input (A/D, or the stick well off the branch) walks the ant round the branch again; small sideways parts are still ignored so walking along a twig doesn't spiral. At the silhouette edge, turn the camera to keep going round. Not in the packaged build yet.
- [ ] aiming to pick leafes is a bit difficult
  - The leaf the ant would cut now has a pulsing outline, so you can see the target before pressing cut. Picking still takes the leaf nearest the ant's mouth; tell me if it should follow the camera instead. Not in the packaged build yet.
- [ ] eventually make selection outlines for leaves etc, instead of just showing an UI overlay
  - Leaves done (outline on the targeted leaf). Trails not yet.
- [ ] low priority: leaf cutting animation would be nice
- [ ] low priority: if you're already halfway back down the tree and you engage the auto-return, ideally it should just follow the scent line if you're close to it from that location, instead of walking strictly back (and going back up the tree etc.)
- [ ] add key/controller icons (https://thoseawesomeguys.com/prompts/)
- [x] add controller support
  - Xbox pads worked already (XInput). PS5 and PS4 pads now work through Microsoft GameInput, mapped per SDL_GameControllerDB and checked with your DualSense. Right stick looks inverted, as you asked. Not in the packaged build yet.
- [ ] mouse cursor controls in the UI would be nice
- [x] got stuck on the bottom of the tree on day 4
  - Fixed: the walking surface had a thin vertical strip (up to 20 cm tall) on the outer side of every slight bend in a branch, left over from my earlier fix; the ant bounced off it. Replayed day 4's tree (seed 208038): the climb from the nest now goes straight up. Not in the packaged build yet.
- [x] leaf animation seems to play without actually moving the leaf, only the shadow moves?
  - The sway was tiny: at most 7 cm at the tip of a 2 m leaf in a full gust, under 1 cm when calm; a low sun stretched that in the shadow. Now about 5 cm calm and 40 cm at the tip in a full gust. Not in the packaged build yet.

## To revisit: thin-branch controls (2026-11-12)

Controls are camera-relative, but on thin branches two rules override them: input is snapped to "along the branch" unless it is clearly sideways (more than about 27 degrees off the branch on screen), and clearly sideways input walks the ant round the branch, with the circling direction locked while the stick holds still. Both can make "right" move the ant up on screen. Options:

- [ ] Steering-wheel A/D on thin branches: W/S always along the branch (away from / toward the camera), A/D always clockwise / anticlockwise round it as seen from the camera. Consistent, less literal.
- [ ] Softer snapping: blend along and around instead of the 27-degree threshold, so diagonal input moves diagonally on the surface. Closer to pure screen-relative, but a slight aim off the branch spirals.
- [ ] Keep the current rules and tune the specific spots that feel wrong (needs examples: twig or thicker branch, where the camera was).
- Also contributing: the camera slowly rolls to follow the surface on mid-size and big branches, so screen directions drift while walking.

