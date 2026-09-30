// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "JointLedgerDemoGameMode.generated.h"

/**
 * Der Spielmodus der Demo — er tut genau eine Sache: das HUD setzen.
 *
 * Ohne ihn zeichnet niemand den Kasten, und die Demo-Bilder waeren vier Wuerfel ohne Aussage.
 */
UCLASS()
class JOINTLEDGER_API AJointLedgerDemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJointLedgerDemoGameMode();
};