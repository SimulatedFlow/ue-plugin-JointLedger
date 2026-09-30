// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "JointLedgerStatics.h"

// ---------------------------------------------------------------- Enthaltung zuerst

bool UJointLedgerStatics::IsJudgeable(const FJointRecord& Record, const FJointBudget& Budget)
{
	if (Record.OwnerName.IsNone())
	{
		return false;
	}
	return Record.ObservedSeconds >= Budget.MinObservedSeconds;
}

bool UJointLedgerStatics::IsUnjudged(const FJointRecord& Record, const FJointBudget& Budget)
{
	return !IsJudgeable(Record, Budget);
}

// ------------------------------------------------- Der Fall, der KEIN Befund ist

bool UJointLedgerStatics::IsWorldAnchor(const FJointRecord& Record)
{
	// GENAU EINE Seite leer. Beide leer ist kein Anker, sondern der Befund.
	const bool bEins = Record.Frame1 == EJointFrameState::Empty;
	const bool bZwei = Record.Frame2 == EJointFrameState::Empty;
	return bEins != bZwei;
}

// ---------------------------------------------------------------- Die vier Befunde

bool UJointLedgerStatics::IsBothFramesEmpty(const FJointRecord& Record)
{
	return Record.Frame1 == EJointFrameState::Empty && Record.Frame2 == EJointFrameState::Empty;
}

bool UJointLedgerStatics::IsNamedBodyMissing(const FJointRecord& Record)
{
	return Record.Frame1 == EJointFrameState::Missing || Record.Frame2 == EJointFrameState::Missing;
}

bool UJointLedgerStatics::IsNeverInitialised(const FJointRecord& Record)
{
	// NUR wenn wenigstens eine Seite wirklich aufgeloest wurde. Sonst ist „nie erzeugt" die
	// Folge eines anderen Befundes und nicht sein eigener.
	const bool bIrgendwasAufgeloest =
		Record.Frame1 == EJointFrameState::Resolved || Record.Frame2 == EJointFrameState::Resolved;
	return bIrgendwasAufgeloest && !Record.bWasEverValid;
}

bool UJointLedgerStatics::IsNothingSimulating(const FJointRecord& Record)
{
	// Fuer einen Welt-Anker ist das KEIN Mangel: die Welt simuliert nie, und trotzdem ist das
	// Gelenk genau so gemeint. Geprueft wird deshalb nur die Seite, die einen Koerper hat.
	if (Record.Frame1 == EJointFrameState::Resolved && Record.Frame2 == EJointFrameState::Resolved)
	{
		return !Record.bBody1EverSimulated && !Record.bBody2EverSimulated;
	}
	if (Record.Frame1 == EJointFrameState::Resolved)
	{
		return !Record.bBody1EverSimulated;
	}
	if (Record.Frame2 == EJointFrameState::Resolved)
	{
		return !Record.bBody2EverSimulated;
	}
	return false;
}

EJointFault UJointLedgerStatics::ClassifyRecord(const FJointRecord& Record,
	const FJointBudget& Budget)
{
	// 1. Enthaltung. Ohne Nenner kein „nie".
	if (IsUnjudged(Record, Budget))
	{
		return Budget.bReportUnjudged ? EJointFault::Unjudged : EJointFault::None;
	}

	// 2. Der Welt-Anker — normalerweise ein AUSSCHLUSS, vor allen Befunden.
	if (IsWorldAnchor(Record))
	{
		// Wer ihn ausdruecklich sehen will, bekommt ihn als WARNUNG. Das ist der Befund, den
		// der Schalter im Budget die ganze Zeit versprochen hat, ohne dass es ihn gab.
		if (Budget.bWorldAnchorIsAFinding)
		{
			return EJointFault::WorldAnchor;
		}
		// Sonst bleibt er unauffaellig — darf aber trotzdem „schlafend" sein; das ist eine
		// Warnung und kein Fehler.
		if (IsNothingSimulating(Record))
		{
			return Budget.bReportNothingSimulates ? EJointFault::NothingSimulates : EJointFault::None;
		}
		return EJointFault::None;
	}

	// 3. Die Ursache vor ihrer Folge.
	if (IsBothFramesEmpty(Record))
	{
		return EJointFault::BothFramesEmpty;
	}

	// 4. Die praezisere Adresse zuerst: ein Name laesst sich korrigieren.
	if (IsNamedBodyMissing(Record))
	{
		return EJointFault::NamedBodyMissing;
	}

	if (IsNeverInitialised(Record))
	{
		return EJointFault::NeverInitialised;
	}

	if (IsNothingSimulating(Record))
	{
		return Budget.bReportNothingSimulates ? EJointFault::NothingSimulates : EJointFault::None;
	}

	return EJointFault::None;
}

// ---------------------------------------------------------------- Zaehlen und urteilen

int32 UJointLedgerStatics::CountFaults(const TArray<FJointRecord>& Records, EJointFault Fault,
	const FJointBudget& Budget)
{
	int32 Zahl = 0;
	for (const FJointRecord& R : Records)
	{
		if (ClassifyRecord(R, Budget) == Fault)
		{
			++Zahl;
		}
	}
	return Zahl;
}

float UJointLedgerStatics::HealthyShare(const TArray<FJointRecord>& Records,
	const FJointBudget& Budget)
{
	int32 Nenner = 0;
	int32 Zaehler = 0;
	for (const FJointRecord& R : Records)
	{
		if (!IsJudgeable(R, Budget))
		{
			continue;
		}
		++Nenner;
		if (ClassifyRecord(R, Budget) == EJointFault::None)
		{
			++Zaehler;
		}
	}
	// KEIN NENNER, KEINE ZAHL. Eine 0 waere hier eine Zahl, die niemand gemessen hat — und sie
	// laese sich im Bericht nicht von „alles kaputt" unterscheiden.
	if (Nenner == 0)
	{
		return -1.f;
	}
	return 100.f * static_cast<float>(Zaehler) / static_cast<float>(Nenner);
}

EJointVerdict UJointLedgerStatics::Judge(const TArray<FJointRecord>& Records,
	const FJointBudget& Budget, bool bAnyConstraintSeen)
{
	// EIN LAUF OHNE CONSTRAINT IST KEIN BESTEHEN — er hat nichts geprueft.
	if (!bAnyConstraintSeen && Budget.bNoConstraintsIsAnError)
	{
		return EJointVerdict::Fail;
	}

	const int32 Leer = CountFaults(Records, EJointFault::BothFramesEmpty, Budget);
	const int32 Fehlend = CountFaults(Records, EJointFault::NamedBodyMissing, Budget);
	const int32 Nie = CountFaults(Records, EJointFault::NeverInitialised, Budget);
	const int32 Schlafend = CountFaults(Records, EJointFault::NothingSimulates, Budget);
	const int32 Anker = CountFaults(Records, EJointFault::WorldAnchor, Budget);

	const bool bLeerUeber = Leer > Budget.MaxBothFramesEmpty;
	const bool bFehlendUeber = Fehlend > Budget.MaxNamedBodyMissing;
	const bool bNieUeber = Nie > Budget.MaxNeverInitialised;

	// EIN ABGESCHALTETER FEHLER WIRD ZUR WARNUNG, NIE ZUM SCHWEIGEN. Wer
	// `bBothFramesEmptyIsError` ausschaltet, will kein rotes Tor — er will nicht, dass der
	// Befund verschwindet.
	if (bLeerUeber && !Budget.bBothFramesEmptyIsError)
	{
		return EJointVerdict::Warn;
	}
	if (bLeerUeber || bFehlendUeber || bNieUeber)
	{
		return EJointVerdict::Fail;
	}
	if (Schlafend > Budget.MaxNothingSimulates)
	{
		return EJointVerdict::Warn;
	}
	// Wer WorldAnchorIsAFinding einschaltet, will JEDEN sehen — deshalb kein Budget:
	// ein Schwellwert wuerde genau die Liste wieder unterdruecken, um die gebeten wurde.
	if (Anker > 0)
	{
		return EJointVerdict::Warn;
	}
	return EJointVerdict::Pass;
}

int32 UJointLedgerStatics::VerdictExitCode(EJointVerdict Verdict)
{
	switch (Verdict)
	{
	case EJointVerdict::Fail: return 2;
	case EJointVerdict::Warn: return 1;
	default:                  return 0;
	}
}

FString UJointLedgerStatics::VerdictText(EJointVerdict Verdict)
{
	switch (Verdict)
	{
	case EJointVerdict::Fail: return TEXT("FAIL");
	case EJointVerdict::Warn: return TEXT("WARN");
	default:                  return TEXT("PASS");
	}
}

// ---------------------------------------------------------------- Darstellung

FString UJointLedgerStatics::FaultName(EJointFault Fault)
{
	switch (Fault)
	{
	case EJointFault::BothFramesEmpty:  return TEXT("BOTH FRAMES EMPTY");
	case EJointFault::NamedBodyMissing: return TEXT("NAMED BODY MISSING");
	case EJointFault::NeverInitialised: return TEXT("never created");
	case EJointFault::NothingSimulates: return TEXT("nothing simulates");
	case EJointFault::WorldAnchor:      return TEXT("anchored to world");
	case EJointFault::Unjudged:         return TEXT("not judged");
	default:                            return TEXT("ok");
	}
}

FString UJointLedgerStatics::FrameStateName(EJointFrameState State)
{
	switch (State)
	{
	case EJointFrameState::Resolved: return TEXT("resolved");
	case EJointFrameState::Missing:  return TEXT("MISSING");
	default:                         return TEXT("empty (world)");
	}
}

FString UJointLedgerStatics::BodyNamesText(const FJointRecord& Record)
{
	if (Record.AvailableBodyNames.Num() == 0)
	{
		return TEXT("this actor has no primitive components at all");
	}
	TArray<FString> Teile;
	for (const FName& N : Record.AvailableBodyNames)
	{
		Teile.Add(FString::Printf(TEXT("'%s'"), *N.ToString()));
	}
	FString Text = FString::Join(Teile, TEXT(", "));
	if (Record.BodyNamesOmitted > 0)
	{
		// EINE STILLE KAPPUNG WAERE EINE LUEGE: der Leser haelt die Liste sonst fuer vollstaendig
		// und schliesst aus einer fehlenden Zeile, dass es die Komponente nicht gibt.
		Text += FString::Printf(TEXT(" (and %d more)"), Record.BodyNamesOmitted);
	}
	return Text;
}

FString UJointLedgerStatics::FaultText(const FJointRecord& Record, const FJointBudget& Budget)
{
	switch (ClassifyRecord(Record, Budget))
	{
	case EJointFault::BothFramesEmpty:
		return TEXT("BOTH FRAMES EMPTY - neither side names an actor or a component, so the "
					"engine never called InitConstraint at all. IsBroken() still returns false "
					"and nothing was logged: this joint holds nothing and looks healthy");

	case EJointFault::NamedBodyMissing:
	{
		const bool bEins = Record.Frame1 == EJointFrameState::Missing;
		const FName Gesucht = bEins ? Record.WantedName1 : Record.WantedName2;
		return FString::Printf(
			TEXT("NAMED BODY MISSING - frame %d asks for '%s', but that actor has no component "
				 "of that name. It has: %s. Fix the name, not the joint"),
			bEins ? 1 : 2, *Gesucht.ToString(), *BodyNamesText(Record));
	}

	case EJointFault::NeverInitialised:
		return TEXT("never created - both sides resolve to a component, but the constraint "
					"handle was never valid. The bodies existed; the joint between them did not");

	case EJointFault::NothingSimulates:
		return TEXT("nothing simulates - the joint exists and holds real bodies, but none of "
					"them ever simulated physics in this run. A warning, not an error: a ragdoll "
					"that is switched on later looks exactly like this");

	case EJointFault::WorldAnchor:
		return FString::Printf(
			TEXT("anchored to world - frame %d names nothing, so this body is constrained "
				 "against the world. Listed because bWorldAnchorIsAFinding is on; it is a "
				 "deliberate technique, never an error"),
			(Record.Frame1 == EJointFrameState::Empty) ? 1 : 2);

	case EJointFault::Unjudged:
		return FString::Printf(
			TEXT("not judged - watched for %.1f s, the floor is %.1f s"),
			Record.ObservedSeconds, Budget.MinObservedSeconds);

	default:
		if (IsWorldAnchor(Record))
		{
			// DAS IST KEIN BEFUND, UND DER BERICHT SAGT DAS AUCH. Wer hier nichts schreibt,
			// laesst den Leser raten, ob die leere Seite uebersehen wurde.
			return TEXT("ok - one side is empty on purpose: this is an anchor to the world");
		}
		return TEXT("ok");
	}
}

FString UJointLedgerStatics::RecordHeader()
{
	return FString::Printf(TEXT("%-28s %-22s %-14s %-14s %-5s  %s"),
		TEXT("constraint"), TEXT("owner"), TEXT("frame 1"), TEXT("frame 2"),
		TEXT("valid"), TEXT("note"));
}

FString UJointLedgerStatics::RecordLine(const FJointRecord& Record, const FJointBudget& Budget)
{
	return FString::Printf(TEXT("%-28s %-22s %-14s %-14s %-5s  %s"),
		*Record.ConstraintName.ToString(),
		*Record.OwnerName.ToString(),
		*FrameStateName(Record.Frame1),
		*FrameStateName(Record.Frame2),
		Record.bWasEverValid ? TEXT("yes") : TEXT("NO"),
		*FaultText(Record, Budget));
}

FLinearColor UJointLedgerStatics::VerdictColour(EJointVerdict Verdict)
{
	switch (Verdict)
	{
	case EJointVerdict::Fail: return FLinearColor(0.95f, 0.25f, 0.25f);
	case EJointVerdict::Warn: return FLinearColor(0.95f, 0.80f, 0.20f);
	default:                  return FLinearColor(0.30f, 0.85f, 0.40f);
	}
}

FLinearColor UJointLedgerStatics::FaultColour(EJointFault Fault)
{
	switch (Fault)
	{
	case EJointFault::BothFramesEmpty:
	case EJointFault::NamedBodyMissing:
	case EJointFault::NeverInitialised: return FLinearColor(0.95f, 0.25f, 0.25f);
	case EJointFault::NothingSimulates: return FLinearColor(0.95f, 0.80f, 0.20f);
	case EJointFault::WorldAnchor:      return FLinearColor(0.55f, 0.75f, 0.95f);
	case EJointFault::Unjudged:         return FLinearColor(0.65f, 0.65f, 0.70f);
	default:                            return FLinearColor(0.75f, 0.78f, 0.82f);
	}
}

FString UJointLedgerStatics::ExplainLimits(const FJointBudget& Budget)
{
	TArray<FString> Zeilen;
	Zeilen.Add(TEXT("1. It records what each constraint was in THIS run. A joint on an actor "
					"that never spawned is not in the report at all - this tool watches what "
					"ran, it does not search your project."));
	Zeilen.Add(FString::Printf(
		TEXT("2. A constraint watched for less than %.2f s is not judged. Without that floor a "
			 "short run reports every sleeping ragdoll as broken - correctly computed and "
			 "completely worthless."),
		Budget.MinObservedSeconds));
	Zeilen.Add(TEXT("3. ONE empty frame is not a finding. The engine's own comment calls that "
					"'joint to world', and it is how a door, a swing or a pendulum is built. "
					"Only both sides empty is a fault."));
	Zeilen.Add(TEXT("4. 'nothing simulates' is a warning, never an error: a ragdoll that is "
					"switched on later is indistinguishable from one that never wakes up."));
	Zeilen.Add(TEXT("5. Validity is read from the constraint handle itself, not concluded from "
					"the names. Names and handle are two independent measurements of the same "
					"joint."));
	Zeilen.Add(FString::Printf(
		TEXT("6. It tracks at most %d constraints; when the cap is hit the report says so."),
		Budget.MaxTrackedConstraints));
	Zeilen.Add(TEXT("7. It is not a profiler: frames, names and simulation state, never "
					"milliseconds and never solver iterations."));
	Zeilen.Add(TEXT("8. It changes nothing - no constraint is created, broken or re-targeted."));
	return FString::Join(Zeilen, TEXT("\n"));
}
