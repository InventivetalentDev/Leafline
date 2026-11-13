// M_TreeBark2, Custom node "Keep": opacity in the shadow pass. Shadows ignore the cutaway, but buried bark casts none.
return step(-Depth, VC.y); // VC: UV3, y = signed distance to the overlapping branch (cm); Depth: MPC_View BuriedDepth
