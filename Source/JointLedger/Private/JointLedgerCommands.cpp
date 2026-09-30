// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"

#include "JointLedgerLog.h"
#include "JointLedgerSettings.h"
#include "JointLedgerStatics.h"
#include "JointLedgerSubsystem.h"

namespace
{
	UJointLedgerSubsystem* Ledger(UWorld* W)
	{
		UJointLedgerSubsystem* S = W ? W->GetSubsystem<UJointLedgerSubsystem>() : nullptr;
		if (!S)
		{
			UE_LOG(LogJointLedger, Warning,
				TEXT("JointLedger: no game world - this only runs while a game world is up."));
		}
		return S;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdShow(
		TEXT("JointLedger.Show"),
		TEXT("JointLedger.Show - draw the panel."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld*)
			{
				GetMutableDefault<UJointLedgerSettings>()->bShowOverlay = true;
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdHide(
		TEXT("JointLedger.Hide"),
		TEXT("JointLedger.Hide - hide the panel."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld*)
			{
				GetMutableDefault<UJointLedgerSettings>()->bShowOverlay = false;
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdReset(
		TEXT("JointLedger.Reset"),
		TEXT("JointLedger.Reset - forget everything and start the run over."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* W)
			{
				if (UJointLedgerSubsystem* S = Ledger(W))
				{
					S->ResetLedger();
					UE_LOG(LogJointLedger, Display, TEXT("JointLedger: run reset."));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdDump(
		TEXT("JointLedger.Dump"),
		TEXT("JointLedger.Dump - print every constraint to the log, findings and all."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* W)
			{
				UJointLedgerSubsystem* S = Ledger(W);
				if (!S)
				{
					return;
				}
				const FJointBudget Budget = S->GetBudget();
				UE_LOG(LogJointLedger, Display, TEXT("%s"), *S->SummaryLine());
				UE_LOG(LogJointLedger, Display, TEXT("%s"), *UJointLedgerStatics::RecordHeader());

				const TArray<FJointRecord>& Alle = S->GetRecords();
				if (Alle.Num() == 0)
				{
					// KEINE ZEILE IST KEINE GUTE NACHRICHT. Wer hier nichts schreibt, laesst
					// den Leser glauben, es sei alles in Ordnung.
					UE_LOG(LogJointLedger, Display,
						TEXT("   (no physics constraint has been seen yet - nothing has been "
							 "checked)"));
					return;
				}
				for (const FJointRecord& R : Alle)
				{
					UE_LOG(LogJointLedger, Display, TEXT("%s"),
						*UJointLedgerStatics::RecordLine(R, Budget));
				}
			}));

	/**
	 * Die Komponentennamen auflisten — der Befehl, der beim Suchen wirklich hilft.
	 *
	 * Wer einen Befund „named body missing" bekommt, will genau das wissen: wie heissen die
	 * Komponenten, die es WIRKLICH gibt? Das steht zwar in der Notiz, aber dort gekappt.
	 */
	FAutoConsoleCommandWithWorldAndArgs CmdBodies(
		TEXT("JointLedger.Bodies"),
		TEXT("JointLedger.Bodies - for every constraint whose named body is missing, print what "
			 "that actor really has."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* W)
			{
				UJointLedgerSubsystem* S = Ledger(W);
				if (!S)
				{
					return;
				}
				const FJointBudget Budget = S->GetBudget();
				int32 Gezeigt = 0;
				for (const FJointRecord& R : S->GetRecords())
				{
					if (UJointLedgerStatics::ClassifyRecord(R, Budget)
						!= EJointFault::NamedBodyMissing)
					{
						continue;
					}
					++Gezeigt;
					const bool bEins = R.Frame1 == EJointFrameState::Missing;
					UE_LOG(LogJointLedger, Display,
						TEXT("%s on %s: frame %d wants '%s' - that actor has: %s"),
						*R.ConstraintName.ToString(), *R.OwnerName.ToString(),
						bEins ? 1 : 2,
						*(bEins ? R.WantedName1 : R.WantedName2).ToString(),
						*UJointLedgerStatics::BodyNamesText(R));
				}
				if (Gezeigt == 0)
				{
					// AUSDRUECKLICH UNTERSCHEIDEN: nichts zu zeigen heisst hier wirklich, dass
					// kein Name ins Leere zeigt — und nicht, dass der Befehl nichts tut.
					UE_LOG(LogJointLedger, Display,
						TEXT("   (no constraint names a body that does not exist)"));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdReport(
		TEXT("JointLedger.Report"),
		TEXT("JointLedger.Report [path] - write the report as JSON "
			 "(default: Saved/JointLedger/report.json)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* W)
			{
				if (UJointLedgerSubsystem* S = Ledger(W))
				{
					if (S->WriteReport(Args.Num() > 0 ? Args[0] : TEXT("report")).IsEmpty())
					{
						UE_LOG(LogJointLedger, Warning,
							TEXT("JointLedger.Report: nothing was written."));
					}
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdGate(
		TEXT("JointLedger.Gate"),
		TEXT("JointLedger.Gate <seconds> [-noexit] - measure, write the report, "
			 "exit 0 clean / 1 warnings / 2 errors."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* W)
			{
				UJointLedgerSubsystem* S = W ? W->GetSubsystem<UJointLedgerSubsystem>() : nullptr;
				if (!S)
				{
					// EIN LAUF OHNE WELT IST KEIN BESTEHEN. Es gibt keine saubere Messung von
					// nichts.
					UE_LOG(LogJointLedger, Error,
						TEXT("JointLedger.Gate: no game world, so nothing can be measured. "
							 "That is not a pass."));
					FPlatformMisc::RequestExitWithStatus(/*Force=*/true, 2);
					return;
				}

				float Sekunden = 60.f;
				bool bExit = true;
				for (const FString& A : Args)
				{
					if (A.Equals(TEXT("-noexit"), ESearchCase::IgnoreCase))
					{
						bExit = false;
					}
					else if (A.IsNumeric())
					{
						Sekunden = FCString::Atof(*A);
					}
				}

				// EIN HINWEIS, DER GELD SPART: wer das Tor kuerzer laufen laesst als die
				// Beobachtungsschwelle, bekommt lauter Enthaltungen und wundert sich ueber
				// einen gruenen Lauf, der nichts geprueft hat. Bei JointLedger steht die
				// Schwelle bei 5 s — hier ist das kein Randfall, sondern der Regelfall fuer
				// jeden, der das Tor kurz ausprobiert.
				const float Mindest = S->GetBudget().MinObservedSeconds;
				if (Sekunden > 0.f && Sekunden < Mindest)
				{
					UE_LOG(LogJointLedger, Warning,
						TEXT("JointLedger.Gate: %.2f s is shorter than MinObservedSeconds "
							 "(%.2f s), so every constraint will be 'not judged' - a green run "
							 "that proves nothing."),
						Sekunden, Mindest);
				}

				S->StartGate(Sekunden, bExit);
			}));
}
