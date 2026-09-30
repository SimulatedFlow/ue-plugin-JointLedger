// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "JointLedgerTypes.h"

#include "JointLedgerSubsystem.generated.h"

class UPhysicsConstraintComponent;
class UPrimitiveComponent;

/**
 * Das Auge: geht die Constraints der laufenden Welt durch und schreibt auf, was es findet.
 *
 * ES URTEILT NICHT. Jede Regel steht in `UJointLedgerStatics`, und zwar als reine Funktion.
 * Diese Trennung ist der Grund, warum sich jede Regel an erfundenen Eintraegen pruefen und
 * einzeln sabotieren laesst, ohne eine Physikszene aufzubauen.
 */
UCLASS()
class JOINTLEDGER_API UJointLedgerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UJointLedgerSubsystem* Get(const UObject* WorldContext);

	// ------------------------------------------------------------ Subsystem / Tickable
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// ------------------------------------------------------------ Steuerung
	UFUNCTION(BlueprintCallable, Category = "JointLedger")
	void ResetLedger();

	/** Einmal ausserhalb des Takts abtasten — fuer Konsolenbefehle und Tests. */
	UFUNCTION(BlueprintCallable, Category = "JointLedger")
	void SampleNow();

	// ------------------------------------------------------------ Ergebnis
	UFUNCTION(BlueprintPure, Category = "JointLedger")
	const TArray<FJointRecord>& GetRecords() const { return Records; }

	/** Das geltende Budget — aus den Einstellungen, damit es nicht zwei Wahrheiten gibt. */
	UFUNCTION(BlueprintPure, Category = "JointLedger")
	FJointBudget GetBudget() const;

	/**
	 * Die Einträge mit Befund, nach Schwere sortiert.
	 *
	 * EINE EINZIGE RANGFOLGE FÜR ALLE. Kasten, Bericht und `DominantLine` fragen dieselbe
	 * Funktion — zwei Sortierungen wären die erste Stelle, an der Anzeige und Urteil
	 * auseinanderlaufen, und der Kasten zeigt dann etwas anderes als das Tor meldet.
	 */
	UFUNCTION(BlueprintPure, Category = "JointLedger")
	TArray<FJointRecord> GetFindingsBySeverity() const;

	UFUNCTION(BlueprintPure, Category = "JointLedger")
	EJointVerdict GetVerdict() const;

	UFUNCTION(BlueprintPure, Category = "JointLedger")
	FString SummaryLine() const;

	/** Die auffaelligste Zeile — was ein Leser zuerst sehen soll. */
	UFUNCTION(BlueprintPure, Category = "JointLedger")
	FString DominantLine() const;

	UFUNCTION(BlueprintPure, Category = "JointLedger")
	FString LimitsLine() const;

	UFUNCTION(BlueprintPure, Category = "JointLedger")
	bool HasSeenAnyConstraint() const { return bAnyConstraintSeen; }

	UFUNCTION(BlueprintPure, Category = "JointLedger")
	bool HasHitTrackingCap() const { return bHitCap; }

	/** Schreibt `Saved/JointLedger/report.json` und liefert den vollen Pfad. */
	UFUNCTION(BlueprintCallable, Category = "JointLedger")
	FString WriteReport(const FString& FileName = TEXT("report")) const;

	// ------------------------------------------------------------ Das Tor

	/**
	 * `bExitWhenDone = false` misst und urteilt, beendet den Prozess aber NICHT.
	 *
	 * Ohne diesen Schalter liesse sich das Tor in einer laufenden Sitzung ueberhaupt nicht
	 * pruefen: jeder Versuch wuerde den Editor mitnehmen. Ein Tor, das man nur einmal und nur
	 * im Ernstfall ausprobieren kann, ist ungeprueft.
	 */
	void StartGate(float Seconds, bool bExitWhenDone = true);
	bool IsGateRunning() const { return bGateRunning; }

private:
	/**
	 * Eine Seite des Gelenks aufloesen — OHNE das protected `GetComponentInternal`.
	 *
	 * DIE LOGIK IST AUS DER ENGINE ABGELESEN, NICHT NACHEMPFUNDEN
	 * (PhysicsConstraintComponent.cpp:50-147): Override zuerst · sonst Actor, ersatzweise der
	 * Besitzer · leerer Name → RootComponent · sonst Suche ueber `GetComponents()`. Der
	 * Sonderfall `UChildActorComponent` gehoert dazu: die Engine nimmt dann nicht die
	 * Komponente selbst, sondern das RootComponent ihres Kindaktors (:134-140). Wer ihn
	 * weglaesst, meldet ein voellig gesundes Gelenk als kaputt.
	 */
	static EJointFrameState ResolveFrame(const UPhysicsConstraintComponent* Constraint,
		int32 Side, FName& OutWanted, UPrimitiveComponent*& OutComponent);

	/** Alle Primitivkomponenten eines Actors, gekappt — der Grund des Tippfehler-Befundes. */
	static void CollectBodyNames(const AActor* Actor, int32 MaxNames,
		TArray<FName>& OutNames, int32& OutOmitted);

	void SampleWorld();
	FJointRecord* FindOrAdd(const UPhysicsConstraintComponent* Constraint);
	void FinishGate();

	UPROPERTY()
	TArray<FJointRecord> Records;

	/** Schluessel `Owner|Constraint` → Platz in `Records`. */
	TMap<FString, int32> Index;

	float SinceSample = 0.f;
	float ElapsedSeconds = 0.f;
	int32 SamplesTaken = 0;
	float LongestGapSeconds = 0.f;

	bool bAnyConstraintSeen = false;
	bool bHitCap = false;

	bool bGateRunning = false;
	float GateSeconds = 0.f;
	float GateElapsed = 0.f;
	bool bGateExits = true;
};
