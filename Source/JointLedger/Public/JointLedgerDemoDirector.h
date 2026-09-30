// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JointLedgerDemoDirector.generated.h"

class UPhysicsConstraintComponent;
class USceneCaptureComponent2D;
class UStaticMeshComponent;

/**
 * Die Buehne — VIER Gelenke nebeneinander, und der Unterschied ist zu SEHEN.
 *
 *   1. `Joint_Healthy`      zwei Koerper, einer haengt und schwingt      -> kein Befund
 *   2. `Joint_WorldAnchor`  eine Seite leer: Pendel an der Welt          -> KEIN Befund
 *   3. `Joint_HoldsNothing` beide Seiten leer                            -> FEHLER
 *   4. `Joint_Typo`         ein Name zeigt ins Leere, der Wuerfel faellt -> FEHLER
 *
 * DER ZWEITE FALL IST DER WICHTIGSTE, obwohl er nichts meldet. Er sieht dem dritten zum
 * Verwechseln aehnlich — eine leere Seite —, und ein Werkzeug ohne diese Unterscheidung
 * wuerde jedes Pendel und jede Tuer anschwaerzen.
 *
 * ALLE VIER HAENGEN AM SELBEN ACTOR. Das ist kein Sparen: der Bericht muss zeigen, dass der
 * Schluessel (Actor, Constraint) und nicht der Actor allein die Eintraege trennt.
 */
UCLASS()
class JOINTLEDGER_API AJointLedgerDemoDirector : public AActor
{
	GENERATED_BODY()

public:
	AJointLedgerDemoDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Ein Szenenbild ueber ein Render-Target — Regel 3, nie `ExportRenderTarget`. */
	UFUNCTION(BlueprintCallable, Category = "JointLedger|Demo")
	FString SaveSceneImage(int32 Width, int32 Height, const FString& FileName);

	/** Wie lange der Regisseur laeuft, bevor er seine eigene Zaehlung meldet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger|Demo")
	float RunSeconds = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger|Demo")
	bool bTakeShots = false;

	/**
	 * Abstand zwischen zwei Bildern.
	 *
	 * BEWUSST KEIN TEILER VON `RunSeconds` und kein runder Wert: bei SlotLedger standen
	 * Aufnahme- und Wiedergabetakt beide auf 3 s, und jedes Standbild traf denselben
	 * Augenblick des Ablaufs. Hier bewegt sich Physik frei, aber der Grundsatz bleibt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger|Demo")
	float ShotEverySeconds = 2.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger|Demo")
	FVector ShotLocation = FVector(0.f, -900.f, 320.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger|Demo")
	FVector ShotTarget = FVector(0.f, 0.f, 180.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger|Demo")
	float ShotFOV = 70.f;

private:
	void BuildStage();
	void ColourPillars();
	void LogComparison();

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> SceneShot;

	/** Die Wuerfel. Namen sind hier Inhalt und nicht Zierde — der Tippfehler zielt darauf. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Cubes;

	/** Je eine Saeule hinter jedem Gelenk, eingefaerbt nach dem Urteil des PLUGINS. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Pillars;

	UPROPERTY()
	TArray<TObjectPtr<UPhysicsConstraintComponent>> Joints;

	bool bBuilt = false;
	bool bReported = false;
	float Elapsed = 0.f;
	float SinceShot = 0.f;
	int32 Shots = 0;
};
