// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "JointLedgerTypes.h"

#include "JointLedgerSettings.generated.h"

/**
 * Die Einstellungen — an EINER Stelle, damit es nicht zwei Wahrheiten gibt.
 *
 * Besonders `bShowOverlay`: haette das HUD einen eigenen Schalter, koennten Einstellung und
 * Anzeige auseinanderlaufen, und niemand wuesste, welcher der beiden gilt.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "JointLedger"))
class JOINTLEDGER_API UJointLedgerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UJointLedgerSettings* Get() { return GetDefault<UJointLedgerSettings>(); }

	/** Wird der Kasten gezeichnet? */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	bool bShowOverlay = true;

	/**
	 * Wie oft je Sekunde abgetastet wird.
	 *
	 * NIEDRIGER ALS BEI SLOTLEDGER (10 Hz), und das ist kein Sparzwang: ein Constraint ist ein
	 * Zustand, kein Vorgang. Was sich aendern kann, ist das Simulieren — und das schaltet
	 * niemand fuenfmal je Sekunde um. Haeufiger abzutasten kostet nur Zeit und misst dasselbe.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "JointLedger",
		meta = (ClampMin = "0.5"))
	float SampleHz = 4.f;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	FJointBudget Budget;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
