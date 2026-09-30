// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class JointLedger : ModuleRules
{
	public JointLedger(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// ==============================================================================
		// KEIN ZUSATZMODUL — und das ist der Unterschied zu SlotLedger, nicht ein Versehen.
		// ==============================================================================
		//
		// SlotLedger braucht "AnimGraphRuntime", weil der Typ FAnimNode_Slot dort liegt.
		// JointLedger braucht nichts dergleichen: alles, was dieses Werkzeug anfasst, steht
		// in "Engine" —
		//
		//   UPhysicsConstraintComponent   Runtime/Engine/Classes/PhysicsEngine/PhysicsConstraintComponent.h
		//   FConstraintInstance           Runtime/Engine/Classes/PhysicsEngine/ConstraintInstance.h
		//   UPrimitiveComponent           Runtime/Engine/Classes/Components/PrimitiveComponent.h
		//
		// "PhysicsCore" waere der naheliegende Reflex und ist hier FALSCH: dort liegen
		// Materialien und Einstellungen der Physik, nicht die Constraint-Komponente. Ein
		// Abhaengigkeitseintrag, den niemand braucht, ist genauso eine Vermutung wie ein
		// fehlender — deshalb steht er nicht drin, und der Bau belegt das.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
			"RenderCore"
		});
	}
}
