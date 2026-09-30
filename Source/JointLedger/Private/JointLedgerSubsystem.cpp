// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "JointLedgerSubsystem.h"

#include "JointLedgerLog.h"
#include "JointLedgerSettings.h"
#include "JointLedgerStatics.h"

// DIESE DREI ZEILEN SIND AM 30.09.2026 VOM BAU OHNE UNITY ERZWUNGEN WORDEN — der Unity-Bau
// war gruen, weil ein Nachbar im selben Block sie mitgebracht hat. Ohne sie endet der
// Einzelbau mit 102 Fehlern:
//
//   JointLedgerSubsystem.cpp(27,23): error C2065: "GEngine": nichtdeklarierter Bezeichner
//   JointLedgerSubsystem.cpp(28,54): error C2653: "EGetWorldErrorMode": Keine Klasse
//   JointLedgerSubsystem.cpp(383,13): error C3203: "TJsonWriter": nicht spezialisiertes
//                                     Klassen-Template ... ein echter Typ wurde erwartet
//
// Genau deshalb wird VOR dem Ausliefern ohne Unity gegengebaut: beim Kaeufer steht diese
// Datei irgendwo anders im Block — oder allein.
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UJointLedgerSubsystem* UJointLedgerSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	const UWorld* Welt = GEngine
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return Welt ? Welt->GetSubsystem<UJointLedgerSubsystem>() : nullptr;
}

void UJointLedgerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	UE_LOG(LogJointLedger, Log, TEXT("JointLedger watching at %.1f Hz"), S ? S->SampleHz : 4.f);
}

bool UJointLedgerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UJointLedgerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UJointLedgerSubsystem, STATGROUP_Tickables);
}

void UJointLedgerSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedSeconds += DeltaTime;
	SinceSample += DeltaTime;

	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	const float Hz = (S && S->SampleHz > 0.f) ? S->SampleHz : 4.f;
	const float Takt = 1.f / Hz;

	if (SinceSample >= Takt)
	{
		// DIE LAENGSTE LUECKE GEHOERT IN DEN BERICHT. Eine Messung, die ihre eigene Guete
		// verschweigt, moechte geglaubt und nicht geprueft werden.
		LongestGapSeconds = FMath::Max(LongestGapSeconds, SinceSample);
		SinceSample = 0.f;
		SampleWorld();
	}

	if (bGateRunning)
	{
		GateElapsed += DeltaTime;
		if (GateElapsed >= GateSeconds)
		{
			FinishGate();
		}
	}
}

void UJointLedgerSubsystem::ResetLedger()
{
	Records.Reset();
	Index.Reset();
	SinceSample = 0.f;
	ElapsedSeconds = 0.f;
	SamplesTaken = 0;
	LongestGapSeconds = 0.f;
	bAnyConstraintSeen = false;
	bHitCap = false;
}

void UJointLedgerSubsystem::SampleNow()
{
	SampleWorld();
}

EJointFrameState UJointLedgerSubsystem::ResolveFrame(const UPhysicsConstraintComponent* Constraint,
	int32 Side, FName& OutWanted, UPrimitiveComponent*& OutComponent)
{
	OutComponent = nullptr;
	OutWanted = NAME_None;
	if (!Constraint)
	{
		return EJointFrameState::Empty;
	}

	const TWeakObjectPtr<UPrimitiveComponent>& Override =
		(Side == 1) ? Constraint->OverrideComponent1 : Constraint->OverrideComponent2;
	if (Override.IsValid())
	{
		OutComponent = Override.Get();
		return EJointFrameState::Resolved;
	}

	const FName Name = (Side == 1)
		? Constraint->ComponentName1.ComponentName
		: Constraint->ComponentName2.ComponentName;
	AActor* Actor = (Side == 1) ? Constraint->ConstraintActor1 : Constraint->ConstraintActor2;
	OutWanted = Name;

	// DIE ENTSCHEIDENDE ZEILE, und sie steht so auch in der Engine (:111):
	// weder Actor noch Name -> das Gelenk haengt an der WELT. Das ist kein Mangel.
	if (Actor == nullptr && Name.IsNone())
	{
		return EJointFrameState::Empty;
	}

	if (Actor == nullptr)
	{
		Actor = Constraint->GetOwner();
	}
	if (Actor == nullptr)
	{
		return EJointFrameState::Missing;
	}

	if (Name.IsNone())
	{
		OutComponent = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		return OutComponent ? EJointFrameState::Resolved : EJointFrameState::Missing;
	}

	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp || Comp->GetFName() != Name)
		{
			continue;
		}
		// DER SONDERFALL, DEN MAN NICHT ERRAET: bei einer ChildActorComponent nimmt die Engine
		// das RootComponent des KINDAKTORS, nicht die Komponente selbst (:134-140).
		if (UChildActorComponent* Kind = Cast<UChildActorComponent>(Comp))
		{
			if (AActor* KindAktor = Kind->GetChildActor())
			{
				OutComponent = Cast<UPrimitiveComponent>(KindAktor->GetRootComponent());
			}
		}
		else
		{
			OutComponent = Cast<UPrimitiveComponent>(Comp);
		}
		break;
	}

	return OutComponent ? EJointFrameState::Resolved : EJointFrameState::Missing;
}

void UJointLedgerSubsystem::CollectBodyNames(const AActor* Actor, int32 MaxNames,
	TArray<FName>& OutNames, int32& OutOmitted)
{
	OutNames.Reset();
	OutOmitted = 0;
	if (!Actor)
	{
		return;
	}
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Cast<UPrimitiveComponent>(Comp))
		{
			continue;
		}
		if (OutNames.Num() < MaxNames)
		{
			OutNames.Add(Comp->GetFName());
		}
		else
		{
			++OutOmitted;
		}
	}
}

FJointRecord* UJointLedgerSubsystem::FindOrAdd(const UPhysicsConstraintComponent* Constraint)
{
	const AActor* Besitzer = Constraint->GetOwner();
	// DER SCHLUESSEL TRAEGT BEIDES. Ein Ragdoll haengt Dutzende Constraints an denselben
	// Actor; ohne den Komponentennamen verschmelzen sie zu einem Eintrag, und ein Befund am
	// Oberarm verschwindet hinter einem gesunden Oberschenkel.
	const FString Schluessel = FString::Printf(TEXT("%s|%s"),
		Besitzer ? *Besitzer->GetName() : TEXT("(no owner)"),
		*Constraint->GetName());

	if (const int32* Platz = Index.Find(Schluessel))
	{
		return &Records[*Platz];
	}

	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	const int32 Kappung = S ? S->Budget.MaxTrackedConstraints : 512;
	if (Records.Num() >= Kappung)
	{
		bHitCap = true;
		return nullptr;
	}

	FJointRecord Neu;
	Neu.ConstraintName = Constraint->GetFName();
	Neu.OwnerName = Besitzer ? Besitzer->GetFName() : NAME_None;
	const int32 Platz = Records.Add(Neu);
	Index.Add(Schluessel, Platz);
	return &Records[Platz];
}

void UJointLedgerSubsystem::SampleWorld()
{
	UWorld* Welt = GetWorld();
	if (!Welt)
	{
		return;
	}

	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	const int32 MaxNamen = S ? S->Budget.MaxBodyNamesListed : 8;
	const float Takt = (S && S->SampleHz > 0.f) ? (1.f / S->SampleHz) : 0.25f;

	++SamplesTaken;

	for (TActorIterator<AActor> It(Welt); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}
		TArray<UPhysicsConstraintComponent*> Gelenke;
		Actor->GetComponents(Gelenke);
		for (UPhysicsConstraintComponent* C : Gelenke)
		{
			if (!IsValid(C))
			{
				continue;
			}
			bAnyConstraintSeen = true;

			FJointRecord* R = FindOrAdd(C);
			if (!R)
			{
				continue;
			}

			UPrimitiveComponent* Eins = nullptr;
			UPrimitiveComponent* Zwei = nullptr;
			FName Gewollt1 = NAME_None;
			FName Gewollt2 = NAME_None;
			R->Frame1 = ResolveFrame(C, 1, Gewollt1, Eins);
			R->Frame2 = ResolveFrame(C, 2, Gewollt2, Zwei);
			R->WantedName1 = Gewollt1;
			R->WantedName2 = Gewollt2;
			R->ResolvedName1 = Eins ? Eins->GetFName() : NAME_None;
			R->ResolvedName2 = Zwei ? Zwei->GetFName() : NAME_None;

			// DAS HOECHSTE JE GESEHENE ERGEBNIS, NICHT DAS LETZTE. Ein Constraint, das erst
			// spaet entsteht oder spaet erwacht, war trotzdem da — wer den Schlusszustand
			// nimmt, meldet jedes aufgeraeumte Ragdoll als nie existent.
			R->bWasEverValid |= C->ConstraintInstance.IsValidConstraintInstance();
			R->bBody1EverSimulated |= (Eins && Eins->IsSimulatingPhysics());
			R->bBody2EverSimulated |= (Zwei && Zwei->IsSimulatingPhysics());

			if (R->Frame1 == EJointFrameState::Missing || R->Frame2 == EJointFrameState::Missing)
			{
				const AActor* Suchort = (R->Frame1 == EJointFrameState::Missing)
					? (C->ConstraintActor1 ? ToRawPtr(C->ConstraintActor1) : C->GetOwner())
					: (C->ConstraintActor2 ? ToRawPtr(C->ConstraintActor2) : C->GetOwner());
				CollectBodyNames(Suchort, MaxNamen, R->AvailableBodyNames, R->BodyNamesOmitted);
			}

			R->ObservedSeconds += Takt;
			R->Fault = UJointLedgerStatics::ClassifyRecord(*R,
				S ? S->Budget : FJointBudget());
		}
	}
}

EJointVerdict UJointLedgerSubsystem::GetVerdict() const
{
	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	return UJointLedgerStatics::Judge(Records, S ? S->Budget : FJointBudget(), bAnyConstraintSeen);
}

FString UJointLedgerSubsystem::SummaryLine() const
{
	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	const FJointBudget Budget = S ? S->Budget : FJointBudget();
	const float Anteil = UJointLedgerStatics::HealthyShare(Records, Budget);
	const float Erreicht = (ElapsedSeconds > 0.f)
		? static_cast<float>(SamplesTaken) / ElapsedSeconds : 0.f;

	return FString::Printf(
		TEXT("JointLedger %s | %d constraint(s) | %d sample(s) @ %.1f Hz (%.1f wanted) in %.1f s "
			 "| healthy %s | %d both-empty, %d missing body, %d never created, %d asleep, "
			 "%d not judged%s"),
		*UJointLedgerStatics::VerdictText(GetVerdict()),
		Records.Num(), SamplesTaken, Erreicht, S ? S->SampleHz : 4.f, ElapsedSeconds,
		// KEIN NENNER, KEINE ZAHL — `n/a` und nicht 0 %.
		(Anteil < 0.f) ? TEXT("n/a") : *FString::Printf(TEXT("%.0f%%"), Anteil),
		UJointLedgerStatics::CountFaults(Records, EJointFault::BothFramesEmpty, Budget),
		UJointLedgerStatics::CountFaults(Records, EJointFault::NamedBodyMissing, Budget),
		UJointLedgerStatics::CountFaults(Records, EJointFault::NeverInitialised, Budget),
		UJointLedgerStatics::CountFaults(Records, EJointFault::NothingSimulates, Budget),
		UJointLedgerStatics::CountFaults(Records, EJointFault::Unjudged, Budget),
		bHitCap ? TEXT(" | TRACKING CAP HIT, counts are incomplete") : TEXT(""));
}

FJointBudget UJointLedgerSubsystem::GetBudget() const
{
	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	return S ? S->Budget : FJointBudget();
}

TArray<FJointRecord> UJointLedgerSubsystem::GetFindingsBySeverity() const
{
	const FJointBudget Budget = GetBudget();

	// Die Reihenfolge ist dieselbe wie in `ClassifyRecord` — der Leser soll den schwersten
	// Befund zuerst sehen, nicht den erstbesten.
	const EJointFault Reihenfolge[] = {
		EJointFault::BothFramesEmpty, EJointFault::NamedBodyMissing,
		EJointFault::NeverInitialised, EJointFault::NothingSimulates,
		EJointFault::Unjudged };

	TArray<FJointRecord> Aus;
	for (const EJointFault Art : Reihenfolge)
	{
		for (const FJointRecord& R : Records)
		{
			if (UJointLedgerStatics::ClassifyRecord(R, Budget) == Art)
			{
				Aus.Add(R);
			}
		}
	}
	return Aus;
}

FString UJointLedgerSubsystem::DominantLine() const
{
	const TArray<FJointRecord> MitBefund = GetFindingsBySeverity();
	for (const FJointRecord& R : MitBefund)
	{
		// Die Enthaltung ist kein Befund und darf nicht die Kopfzeile besetzen.
		if (UJointLedgerStatics::ClassifyRecord(R, GetBudget()) != EJointFault::Unjudged)
		{
			return UJointLedgerStatics::RecordLine(R, GetBudget());
		}
	}
	return FString();
}

FString UJointLedgerSubsystem::LimitsLine() const
{
	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	return UJointLedgerStatics::ExplainLimits(S ? S->Budget : FJointBudget());
}

FString UJointLedgerSubsystem::WriteReport(const FString& FileName) const
{
	const UJointLedgerSettings* S = UJointLedgerSettings::Get();
	const FJointBudget Budget = S ? S->Budget : FJointBudget();
	const float Anteil = UJointLedgerStatics::HealthyShare(Records, Budget);

	FString Aus;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Schreiber =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Aus);

	Schreiber->WriteObjectStart();
	Schreiber->WriteValue(TEXT("tool"), TEXT("JointLedger"));
	Schreiber->WriteValue(TEXT("verdict"), UJointLedgerStatics::VerdictText(GetVerdict()));
	Schreiber->WriteValue(TEXT("exitCode"),
		UJointLedgerStatics::VerdictExitCode(GetVerdict()));
	Schreiber->WriteValue(TEXT("summary"), SummaryLine());
	Schreiber->WriteValue(TEXT("constraintsSeen"), Records.Num());
	Schreiber->WriteValue(TEXT("samplesTaken"), SamplesTaken);
	Schreiber->WriteValue(TEXT("wantedHz"), S ? S->SampleHz : 4.f);
	Schreiber->WriteValue(TEXT("achievedHz"),
		(ElapsedSeconds > 0.f) ? static_cast<float>(SamplesTaken) / ElapsedSeconds : 0.f);
	Schreiber->WriteValue(TEXT("measuredSeconds"), ElapsedSeconds);
	Schreiber->WriteValue(TEXT("longestGapSeconds"), LongestGapSeconds);
	Schreiber->WriteValue(TEXT("anyConstraintSeen"), bAnyConstraintSeen);
	Schreiber->WriteValue(TEXT("hitTrackingCap"), bHitCap);

	// NULL STATT NULL-PROZENT: ohne Nenner gibt es keine Quote, und eine 0 waere eine Zahl,
	// die niemand gemessen hat.
	if (Anteil < 0.f)
	{
		Schreiber->WriteNull(TEXT("healthySharePercent"));
	}
	else
	{
		Schreiber->WriteValue(TEXT("healthySharePercent"), Anteil);
	}

	Schreiber->WriteValue(TEXT("bothFramesEmpty"),
		UJointLedgerStatics::CountFaults(Records, EJointFault::BothFramesEmpty, Budget));
	Schreiber->WriteValue(TEXT("namedBodyMissing"),
		UJointLedgerStatics::CountFaults(Records, EJointFault::NamedBodyMissing, Budget));
	Schreiber->WriteValue(TEXT("neverInitialised"),
		UJointLedgerStatics::CountFaults(Records, EJointFault::NeverInitialised, Budget));
	Schreiber->WriteValue(TEXT("nothingSimulates"),
		UJointLedgerStatics::CountFaults(Records, EJointFault::NothingSimulates, Budget));
	Schreiber->WriteValue(TEXT("notJudged"),
		UJointLedgerStatics::CountFaults(Records, EJointFault::Unjudged, Budget));
	Schreiber->WriteValue(TEXT("limitsText"), LimitsLine());

	Schreiber->WriteArrayStart(TEXT("records"));
	for (const FJointRecord& R : Records)
	{
		Schreiber->WriteObjectStart();
		Schreiber->WriteValue(TEXT("constraint"), R.ConstraintName.ToString());
		Schreiber->WriteValue(TEXT("owner"), R.OwnerName.ToString());
		Schreiber->WriteValue(TEXT("frame1"), UJointLedgerStatics::FrameStateName(R.Frame1));
		Schreiber->WriteValue(TEXT("frame2"), UJointLedgerStatics::FrameStateName(R.Frame2));
		Schreiber->WriteValue(TEXT("wanted1"), R.WantedName1.ToString());
		Schreiber->WriteValue(TEXT("wanted2"), R.WantedName2.ToString());
		Schreiber->WriteValue(TEXT("resolved1"), R.ResolvedName1.ToString());
		Schreiber->WriteValue(TEXT("resolved2"), R.ResolvedName2.ToString());
		Schreiber->WriteValue(TEXT("isWorldAnchor"), UJointLedgerStatics::IsWorldAnchor(R));
		Schreiber->WriteValue(TEXT("wasEverValid"), R.bWasEverValid);
		Schreiber->WriteValue(TEXT("body1EverSimulated"), R.bBody1EverSimulated);
		Schreiber->WriteValue(TEXT("body2EverSimulated"), R.bBody2EverSimulated);
		Schreiber->WriteValue(TEXT("observedSeconds"), R.ObservedSeconds);
		Schreiber->WriteValue(TEXT("fault"),
			UJointLedgerStatics::FaultName(UJointLedgerStatics::ClassifyRecord(R, Budget)));
		Schreiber->WriteValue(TEXT("note"), UJointLedgerStatics::FaultText(R, Budget));
		Schreiber->WriteArrayStart(TEXT("availableBodyNames"));
		for (const FName& N : R.AvailableBodyNames)
		{
			Schreiber->WriteValue(N.ToString());
		}
		Schreiber->WriteArrayEnd();
		Schreiber->WriteValue(TEXT("bodyNamesOmitted"), R.BodyNamesOmitted);
		Schreiber->WriteObjectEnd();
	}
	Schreiber->WriteArrayEnd();
	Schreiber->WriteObjectEnd();
	Schreiber->Close();

	const FString Ordner = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("JointLedger"));
	const FString Voll = Ordner / (FileName + TEXT(".json"));
	if (!FFileHelper::SaveStringToFile(Aus, *Voll))
	{
		UE_LOG(LogJointLedger, Warning, TEXT("JointLedger: could not write %s"), *Voll);
		return FString();
	}
	UE_LOG(LogJointLedger, Display, TEXT("JointLedger: report -> %s"), *Voll);
	return Voll;
}

void UJointLedgerSubsystem::StartGate(float Seconds, bool bExitWhenDone)
{
	bGateRunning = true;
	bGateExits = bExitWhenDone;
	GateSeconds = FMath::Max(0.1f, Seconds);
	GateElapsed = 0.f;
	UE_LOG(LogJointLedger, Display,
		TEXT("JointLedger.Gate: watching for %.1f s%s"), GateSeconds,
		bGateExits ? TEXT("") : TEXT(" (will not exit)"));
}

void UJointLedgerSubsystem::FinishGate()
{
	bGateRunning = false;

	// NOCH EINMAL ABTASTEN, BEVOR GEURTEILT WIRD. Sonst entscheidet das Tor auf einem Stand,
	// der bis zu einem Takt alt ist — und bei 4 Hz ist das eine Viertelsekunde, in der ein
	// Ragdoll erwacht sein kann.
	SampleWorld();

	const FString Pfad = WriteReport();
	const EJointVerdict Urteil = GetVerdict();
	const int32 Code = UJointLedgerStatics::VerdictExitCode(Urteil);

	UE_LOG(LogJointLedger, Display, TEXT("%s"), *SummaryLine());
	const FString Auffaellig = DominantLine();
	if (!Auffaellig.IsEmpty())
	{
		UE_LOG(LogJointLedger, Display, TEXT("%s"), *Auffaellig);
	}
	UE_LOG(LogJointLedger, Display,
		TEXT("JointLedger.Gate: %s -> exit code %d (report: %s)"),
		*UJointLedgerStatics::VerdictText(Urteil), Code, *Pfad);

	if (!bGateExits)
	{
		UE_LOG(LogJointLedger, Display,
			TEXT("JointLedger.Gate: -noexit given, staying alive. The exit code WOULD be %d."),
			Code);
		return;
	}

	// GENAU EIN AUSSTIEGSWEG.
	//
	// Hier stand zuerst zusaetzlich ein `GEngine->DeferredCommands.Add("quit_editor")`. Das
	// war falsch und faellt nur nicht sofort auf: der aufgeschobene Befehl laeuft im naechsten
	// Tick und beendet den Editor auf dem NORMALEN Weg — also mit Rueckgabe 0. Wer beide Wege
	// einbaut, hat ein Wettrennen eingebaut, und die 0 gewinnt manchmal.
	//
	// FORCE=TRUE, SONST BEKOMMT DIE SHELL IMMER 0. Elf Plugins haben diesen Fehler einmal
	// gehabt: das Protokoll sagte „exit code 2", der Bauserver sah eine 0 und war zufrieden.
	FPlatformMisc::RequestExitWithStatus(/*Force=*/true, static_cast<uint8>(Code));
}
