// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "JointLedgerTypes.h"

#include "JointLedgerStatics.generated.h"

/**
 * Die Regeln — als reine Funktionen, jede einzeln pruefbar.
 *
 * KEINE EINZIGE DAVON FASST DIE WELT AN. Das Subsystem sammelt, diese Bibliothek urteilt.
 * Nur so laesst sich jede Regel im Automationstest an erfundenen Eintraegen pruefen, ohne
 * eine Physikszene, zwei Koerper und ein tickendes Spiel aufbauen zu muessen — und nur so
 * kann jede Regel einmal sabotiert werden, um zu zeigen, dass der Test sie wirklich festhaelt.
 */
UCLASS()
class JOINTLEDGER_API UJointLedgerStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------- Enthaltung zuerst

	/**
	 * Darf ueber diesen Eintrag ueberhaupt geurteilt werden?
	 *
	 * Zwei Gruende, es zu lassen, und beide sind Unwissen und nicht Unschuld:
	 *  1. Das Constraint wurde kuerzer beobachtet als `MinObservedSeconds`. „Nie simuliert"
	 *     braucht einen Nenner — ein Ragdoll wird oft erst nach Sekunden scharfgeschaltet.
	 *  2. Der besitzende Actor hiess nie etwas. Dann ist der Eintrag eine Huelse, und jede
	 *     Aussage darueber waere eine Aussage ueber nichts.
	 */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static bool IsJudgeable(const FJointRecord& Record, const FJointBudget& Budget);

	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static bool IsUnjudged(const FJointRecord& Record, const FJointBudget& Budget);

	// ------------------------------------------------- Der Fall, der KEIN Befund ist

	/**
	 * Genau eine Seite ist leer: die Engine koppelt den Koerper an die Welt.
	 *
	 * DAS IST DIE WICHTIGSTE FUNKTION IN DIESER DATEI, obwohl sie nichts meldet. Ihr eigener
	 * Kommentar steht in der Engine: „If neither actor nor component name specified, joint to
	 * 'world'" (PhysicsConstraintComponent.cpp:110). Ein Werkzeug ohne diese Unterscheidung
	 * meldet jede Tuer, jede Schaukel und jedes Pendel als Fehler.
	 */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static bool IsWorldAnchor(const FJointRecord& Record);

	// ---------------------------------------------------------------- Die vier Befunde

	/** Beide Seiten zeigen ins Leere — das Gelenk haelt nichts. Die Behebung sind ZWEI Namen. */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static bool IsBothFramesEmpty(const FJointRecord& Record);

	/** Ein Name ist gesetzt, es gibt aber keine Komponente dieses Namens. Die Behebung ist EIN NAME. */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static bool IsNamedBodyMissing(const FJointRecord& Record);

	/**
	 * Die Koerper sind aufloesbar, das Constraint kam trotzdem nie zustande.
	 *
	 * Gemessen am Griff selbst (`IsValidConstraintInstance`), nicht aus den Namen geschlossen.
	 */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static bool IsNeverInitialised(const FJointRecord& Record);

	/** Beide Koerper da, keiner simuliert je. Warnung, kein Fehler. */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static bool IsNothingSimulating(const FJointRecord& Record);

	/**
	 * Die Rangfolge.
	 *
	 * 1. DIE ENTHALTUNG ZUERST. Ein Constraint, das drei Sekunden lang beobachtet wurde, sagt
	 *    ueber „simuliert nie" nichts aus — und wer zuerst auf `NothingSimulates` prueft,
	 *    meldet jedes Ragdoll, das noch schlaeft.
	 *
	 * 2. DANN DER WELT-ANKER, und zwar als AUSSCHLUSS. Er steht vor allen Befunden, weil eine
	 *    leere Seite sonst als „Koerper fehlt" durchginge. Das ist derselbe Fehler, den
	 *    SocketLedger mit `hand_r` beinahe gemacht haette: ein voellig richtiger Zustand, der
	 *    einem zu eifrigen Werkzeug wie ein Mangel aussieht.
	 *
	 * 3. DANN `BothFramesEmpty` VOR `NeverInitialised`, obwohl der erste den zweiten nach sich
	 *    zieht: beide Seiten leer IST der Grund, warum kein Constraint entstand. Die Ursache
	 *    gewinnt gegen ihre Folge — sonst liest der Bericht „wurde nie erzeugt", und der
	 *    Leser sucht nach einem Physikproblem statt nach zwei leeren Feldern.
	 *
	 * 4. DANN `NamedBodyMissing` VOR `NeverInitialised`, aus demselben Grund: ein Tippfehler
	 *    ist eine Adresse, „nie erzeugt" ist ein Symptom. Die praezisere Adresse gewinnt.
	 */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static EJointFault ClassifyRecord(const FJointRecord& Record, const FJointBudget& Budget);

	// ---------------------------------------------------------------- Zaehlen und urteilen

	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static int32 CountFaults(const TArray<FJointRecord>& Records, EJointFault Fault,
		const FJointBudget& Budget);

	/**
	 * Wie viele der beurteilten Gelenke wirklich zustande kamen, in Prozent.
	 *
	 * Der Nenner sind die BEURTEILTEN, nicht alle — sonst druecken Enthaltungen die Quote und
	 * ein Bericht sieht schlechter aus, als die Lage ist. **Gibt es keinen Nenner, gibt es
	 * keine Zahl: dann −1 und im Bericht `n/a`.** Eine 0 waere eine Zahl, die niemand gemessen
	 * hat.
	 */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static float HealthyShare(const TArray<FJointRecord>& Records, const FJointBudget& Budget);

	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static EJointVerdict Judge(const TArray<FJointRecord>& Records, const FJointBudget& Budget,
		bool bAnyConstraintSeen);

	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static int32 VerdictExitCode(EJointVerdict Verdict);

	UFUNCTION(BlueprintPure, Category = "JointLedger|Rules")
	static FString VerdictText(EJointVerdict Verdict);

	// ---------------------------------------------------------------- Darstellung

	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FString FaultName(EJointFault Fault);

	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FString FrameStateName(EJointFrameState State);

	/**
	 * Die Notiz hinter einem Eintrag — in Worten, die jemand lesen kann.
	 *
	 * Beim Befund `NamedBodyMissing` nennt sie AUCH DIE VORHANDENEN KOMPONENTEN. Ohne sie
	 * steht dort ein Symptom; mit ihr steht dort der Tippfehler.
	 */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FString FaultText(const FJointRecord& Record, const FJointBudget& Budget);

	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FString RecordLine(const FJointRecord& Record, const FJointBudget& Budget);

	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FString RecordHeader();

	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FLinearColor VerdictColour(EJointVerdict Verdict);

	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FLinearColor FaultColour(EJointFault Fault);

	/** Die Grenzen dieses Werkzeugs, im Klartext und nicht in einer Fussnote. */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FString ExplainLimits(const FJointBudget& Budget);

	/** Die Komponentennamen eines Eintrags als lesbare Aufzaehlung, Kappung ausgewiesen. */
	UFUNCTION(BlueprintPure, Category = "JointLedger|Report")
	static FString BodyNamesText(const FJointRecord& Record);
};
