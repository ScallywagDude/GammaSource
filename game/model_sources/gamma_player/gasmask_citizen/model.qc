$modelname "gamma/player/gasmask_citizen.mdl"
$model "body" "mesh.smd" {
	eyeball "eyeball_r" "ValveBiped.Bip01_Head1" -1.240186300 -3.512383501 68.534711840 "eyeball_r" 1.000000000 4.000000074 "unused" 0.629999990
	eyeball "eyeball_l" "ValveBiped.Bip01_Head1" 1.344113718 -3.521055710 68.534712598 "eyeball_l" 1.000000000 -4.000000074 "unused" 0.629999990
}
$surfaceprop "flesh"
$contents "solid"
$cdmaterials "models/gamma/player/gasmask_citizen"
$ikchain "rhand" "ValveBiped.Bip01_R_Hand" knee 0.707000017 0.707000017 0
$ikchain "lhand" "ValveBiped.Bip01_L_Hand" knee 0.707000017 0.707000017 0
$ikchain "rfoot" "ValveBiped.Bip01_R_Foot" knee 0.707000017 -0.707000017 0
$ikchain "lfoot" "ValveBiped.Bip01_L_Foot" knee 0.707000017 -0.707000017 0
$sequence "ragdoll" "reference.smd" activity ACT_DIERAGDOLL 1
$includemodel "gamma/player/male_anims.mdl"
$includemodel "gamma/player/male_neutral.mdl"
$attachment "anim_attachment_RH" "ValveBiped.Bip01_R_Hand" 0 0 0 rotate 0 0 0
$attachment "anim_attachment_LH" "ValveBiped.Bip01_L_Hand" 0 0 0 rotate 0 0 0
$attachment "eyes" "ValveBiped.Bip01_Head1" 4.601202376 -3.558704074 -0.051966188 rotate -0.192119064 -80.099929911 -89.999949737

// BEGIN GENERATED CHARACTER PHYSICS
$collisionjoints "collision.smd"
{
	$mass 65
	$inertia 10
	$damping 0.01
	$rotdamping 1.5
	$rootbone "ValveBiped.Bip01_Pelvis"
	$jointconstrain "ValveBiped.Bip01_Spine2" x limit -48 48 0
	$jointconstrain "ValveBiped.Bip01_Spine2" y limit -25 25 0
	$jointconstrain "ValveBiped.Bip01_Spine2" z limit -25 50 0
	$jointconstrain "ValveBiped.Bip01_Head1" x limit -20 20 0
	$jointconstrain "ValveBiped.Bip01_Head1" y limit -25 25 0
	$jointconstrain "ValveBiped.Bip01_Head1" z limit -13 30 0
	$jointconstrain "ValveBiped.Bip01_L_UpperArm" x limit -30 30 0
	$jointconstrain "ValveBiped.Bip01_L_UpperArm" y limit -95 84 0
	$jointconstrain "ValveBiped.Bip01_L_UpperArm" z limit -86 26 0
	$jointconstrain "ValveBiped.Bip01_L_Forearm" x limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_L_Forearm" y limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_L_Forearm" z limit -149 4 0
	$jointconstrain "ValveBiped.Bip01_L_Hand" x limit -37 37 0
	$jointconstrain "ValveBiped.Bip01_L_Hand" y limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_L_Hand" z limit -57 59 0
	$jointconstrain "ValveBiped.Bip01_L_Thigh" x limit -12 12 0
	$jointconstrain "ValveBiped.Bip01_L_Thigh" y limit -73 6 0
	$jointconstrain "ValveBiped.Bip01_L_Thigh" z limit -93 30 0
	$jointconstrain "ValveBiped.Bip01_L_Calf" x limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_L_Calf" y limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_L_Calf" z limit -8 126 0
	$jointconstrain "ValveBiped.Bip01_L_Foot" x limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_L_Foot" y limit -19 19 0
	$jointconstrain "ValveBiped.Bip01_L_Foot" z limit -15 35 0
	$jointconstrain "ValveBiped.Bip01_R_UpperArm" x limit -39 39 0
	$jointconstrain "ValveBiped.Bip01_R_UpperArm" y limit -79 95 0
	$jointconstrain "ValveBiped.Bip01_R_UpperArm" z limit -93 23 0
	$jointconstrain "ValveBiped.Bip01_R_Forearm" x limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_R_Forearm" y limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_R_Forearm" z limit -149 4 0
	$jointconstrain "ValveBiped.Bip01_R_Hand" x limit -60 60 0
	$jointconstrain "ValveBiped.Bip01_R_Hand" y limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_R_Hand" z limit -57 70 0
	$jointconstrain "ValveBiped.Bip01_R_Thigh" x limit -12 12 0
	$jointconstrain "ValveBiped.Bip01_R_Thigh" y limit -8 75 0
	$jointconstrain "ValveBiped.Bip01_R_Thigh" z limit -97 32 0
	$jointconstrain "ValveBiped.Bip01_R_Calf" x limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_R_Calf" y limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_R_Calf" z limit -12 126 0
	$jointconstrain "ValveBiped.Bip01_R_Foot" x limit 0 0 0
	$jointconstrain "ValveBiped.Bip01_R_Foot" y limit -25 6 0
	$jointconstrain "ValveBiped.Bip01_R_Foot" z limit -15 35 0
}
// END GENERATED CHARACTER PHYSICS
