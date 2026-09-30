// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "JointLedgerDemoGameMode.h"

#include "GameFramework/SpectatorPawn.h"

#include "JointLedgerHUD.h"

AJointLedgerDemoGameMode::AJointLedgerDemoGameMode()
{
	HUDClass = AJointLedgerHUD::StaticClass();

	// KEIN `ADefaultPawn`. Der bringt eine sichtbare Kugel mit, und die erscheint im Ursprung —
	// also genau in der Bildmitte zwischen den vier Gelenken. In den ausgelieferten
	// Szenenbildern stand sie zwischen Station 2 und 3 und sah aus, als gehoere sie zur Demo.
	//
	// Aufgefallen ist es erst am 30.09.2026 beim TickLedger-Bildersatz, wo dieselbe Kugel auf
	// derselben Stelle stand. Nachgesehen statt vermutet: `JointLedger_Shot_03.png` zeigte sie
	// genauso.
	//
	// `ASpectatorPawn` statt `nullptr`: der Spielercontroller bleibt bestehen, und nur er
	// traegt das HUD — ohne ihn gaebe es keinen Kasten und damit Bilder ohne Aussage.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}