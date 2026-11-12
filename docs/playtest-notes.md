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
  - Done: the camera no longer pulls in for wood; bark between the camera and the ant (and right in front of the camera) dithers away. Leaves now fade the same way (since 12 Nov evening), except right beside the ant, where the leaf you are cutting usually is. The old behaviour is the camera setting `bAvoidThickOcclusion`. Only seen in still screenshots so far; how it feels in motion needs your verdict. In the editor build.
- [x] ant starts inside the nest mesh at the start (no colission?)
  - Fixed: the nest mound is part of the walking surface, so ants start on top of the entrance and walk over the mound. In the editor build.
- [x] can't walk around around branches anymore? all of WASD just seem to walk up and down the branch; also makes it hard to navigate to even smaller branches
  - Fixed: on thin branches, clearly sideways input (A/D, or the stick well off the branch) walks the ant round the branch again; small sideways parts are still ignored so walking along a twig doesn't spiral. At the silhouette edge, turn the camera to keep going round. In the package of 12 Nov 18:28.
- [ ] aiming to pick leafes is a bit difficult
  - The leaf the ant would cut now has a pulsing outline, so you can see the target before pressing cut. Picking still takes the leaf nearest the ant's mouth; tell me if it should follow the camera instead. In the package of 12 Nov 18:28.
- [ ] eventually make selection outlines for leaves etc, instead of just showing an UI overlay
  - Leaves done (outline on the targeted leaf). Trails not yet.
- [ ] low priority: leaf cutting animation would be nice
- [x] low priority: if you're already halfway back down the tree and you engage the auto-return, ideally it should just follow the scent line if you're close to it from that location, instead of walking strictly back (and going back up the tree etc.)
  - Done: a worker sent home now starts from the scent line point within 1 m that is nearest the nest (its own line or another worker's), then follows it home and skips its loops (where the line comes back within 35 cm of itself it cuts across). It only cuts across where the straight line stays on the bark, never through the air to another branch. Test: an ant that wandered on the trunk for 45 s went home along 61 m instead of retracing 105 m (both include a 40 m jump from the test's teleport), and reached the nest. In the package of 12 Nov 18:28.
- [ ] add key/controller icons (https://thoseawesomeguys.com/prompts/)
- [x] add controller support
  - Xbox pads worked already (XInput). PS5 and PS4 pads now work through Microsoft GameInput, mapped per SDL_GameControllerDB and checked with your DualSense. Right stick looks inverted, as you asked. In the package of 12 Nov 18:28.
- [x] mouse cursor controls in the UI would be nice
  - Done: in the title, recap, end card, nest and pause screens the cursor appears as soon as you move the mouse (W/S or the stick hide it again). In the nest, hovering picks a row and clicking buys or starts; a click beside the list does nothing. The pause screen has Resume and Quit buttons. Elsewhere a click continues. The camera no longer turns with the mouse behind a menu. I checked the pause screen layout in a screenshot but could not move a real mouse here, so the clicking itself is untested.
- [x] got stuck on the bottom of the tree on day 4
  - Fixed: the walking surface had a thin vertical strip (up to 20 cm tall) on the outer side of every slight bend in a branch, left over from my earlier fix; the ant bounced off it. Replayed day 4's tree (seed 208038): the climb from the nest now goes straight up. In the package of 12 Nov 18:28.
- [x] leaf animation seems to play without actually moving the leaf, only the shadow moves?
  - Real cause: every leaf was built twice (a front and a back face with opposite normals, on a two-sided material). The two copies z-fought, and the sway, which moves vertices along their normals, pushed them apart in opposite directions: two leaves cutting through each other, and doubled shadows. Leaves are one face now (half the leaf geometry). Sway is a little larger than before (about 2 cm calm, 20 cm at the tip in a full gust). In the package of 12 Nov 18:28.
- [x] glow around the ant looks weird
  - Toned down: that was a rim light I added so the ant reads in video. It was near-white at strength 2 over a wide edge; now a faint warm sheen right at the silhouette (strength 0.35, tighter falloff). The ant reads dark red-brown again. If it should go entirely, it is one number in `M_Ant` (`Multiply_11`).
- [x] nanite warning text visible in packaged build
  - Fixed: that was the virtual shadow map warning "[VSM] Non-Nanite Marking Job Queue overflow", which shows whenever the canopy fills the view. It only costs a little speed; the on-screen text is now off (`r.Shadow.Virtual.AllowScreenOverflowMessages=0` in `DefaultEngine.ini`).

## To revisit: thin-branch controls (2026-11-12)

Controls are camera-relative, but on thin branches two rules override them: input is snapped to "along the branch" unless it is clearly sideways (more than about 27 degrees off the branch on screen), and clearly sideways input walks the ant round the branch, with the circling direction locked while the stick holds still. Both can make "right" move the ant up on screen. Options:

- [ ] Steering-wheel A/D on thin branches: W/S always along the branch (away from / toward the camera), A/D always clockwise / anticlockwise round it as seen from the camera. Consistent, less literal.
- [ ] Softer snapping: blend along and around instead of the 27-degree threshold, so diagonal input moves diagonally on the surface. Closer to pure screen-relative, but a slight aim off the branch spirals.
- [ ] Keep the current rules and tune the specific spots that feel wrong (needs examples: twig or thicker branch, where the camera was).
- Also contributing: the camera slowly rolls to follow the surface on mid-size and big branches, so screen directions drift while walking.

