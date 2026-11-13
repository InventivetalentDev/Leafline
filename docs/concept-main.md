# ant tree game

- procedurally generated trees, no models/textures, only math and shaders
- you play as an ant, climbing up a tree to collect leaves, food, etc
- 3d model for ant made in blender? 
- potentially wind or birds as enemies
- roguelike, you keep going out for resources, and gradually build up your home

- best should be an upgrade hub with skill trees and endgame rocket launchers to take out the birds 
- use scent lines to show previous runs 
- realistic scale; tiny ant on a huge tree - it should take a bit to get around the main trunk, for example 
- research how exactly ants gather stuff from trees and what, make that somewhat realistic 
- roguelite, tree needs to regenerate for fresh runs
	- I'd like there to be multiple trees instead of just one, but 100% how to make that work
		- maybe just a circle of trees around the nest, and you rotate to the next one over each day/run while the old one is all "harvested" 
- some sort of stamina system to limit run length, with upgrades that can be unlocked (maybe also need to eat to recharge) 
- camera should be kinda floaty, not locked to the ant, to make movement feel smooth
	- basically let the ant move independently, changing surfaces/gravity, while the camera follows along smoothly in 3rd person 
	- should help to avoid getting disoriented e.g. in sharp turns to a branch (ant just makes the turn, while the camera stays in pretty much the same place, easy to see where you're going)
- predators like occasional birds (probably the main thing, but research this a bit too for other things that could make sense as enemies)
- able to carry leaves and bigger items home, should attach to the and physically (not inventory)
	- another good ability to upgrade as you go
	- potentially make the whole game so you can only carry like one item? would be most realistic, but also a bit annoying potentially 
	- better idea: ^ do one item at a time, BUT have a way for the ant to auto return, e.g. by following your own scent line back 
		- birds etc. would still be around and risky, so you'd want to take those out or distract them (throw food etc., trade-off that you yourself end up with less or more depending on if you do that) or risk getting your returning ant killed
		- switch to a different ant back at the home base as soon as you trigger the return
			- potential upgrade to control more and more ants as you progress, but would likely need some kind of auto mode so ants can go *up* the tree on their own as well
				- or just switch between multiple maybe, with stuff like auto-defending ants that attack enemies (not the biggest fan of this idea tbh) or just an auto gather upgrade (leaning more into automation game, not sure if this fits or should rather stay manual)
				- 