// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "JointLedgerStatics.h"
#include "JointLedgerTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace JointLedgerTests
{
	// `EAutomationTestFlags` ist in UE 5.8 eine `enum class` — ein `int32` hier gibt ein
	// Dutzend Uebersetzungsfehler.
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter;

	/** Ein gesundes Gelenk: beide Seiten aufgeloest, Constraint da, beide simulieren. */
	FJointRecord Gesund()
	{
		FJointRecord R;
		R.ConstraintName = TEXT("Constraint_Arm");
		R.OwnerName = TEXT("Ragdoll");
		R.Frame1 = EJointFrameState::Resolved;
		R.Frame2 = EJointFrameState::Resolved;
		R.WantedName1 = TEXT("upperarm_l");
		R.WantedName2 = TEXT("lowerarm_l");
		R.ResolvedName1 = TEXT("upperarm_l");
		R.ResolvedName2 = TEXT("lowerarm_l");
		R.bWasEverValid = true;
		R.bBody1EverSimulated = true;
		R.bBody2EverSimulated = true;
		R.ObservedSeconds = 10.f;
		return R;
	}

	/** Ein Welt-Anker: Seite 2 leer, und das ist voellig richtig so. */
	FJointRecord WeltAnker()
	{
		FJointRecord R = Gesund();
		R.ConstraintName = TEXT("Constraint_Door");
		R.OwnerName = TEXT("Door");
		R.Frame2 = EJointFrameState::Empty;
		R.WantedName2 = NAME_None;
		R.ResolvedName2 = NAME_None;
		R.bBody2EverSimulated = false;   // die Welt simuliert nie
		return R;
	}
}

// ---------------------------------------------------------------------------------------
// 1. DER WELT-ANKER IST KEIN BEFUND.
//
// Das ist der Test, der dieses Plugin von einer naiven Fassung unterscheidet. Die Engine
// koppelt mit genau diesem Mittel an die Welt („joint to 'world'",
// PhysicsConstraintComponent.cpp:110); wer ihn meldet, schwaerzt jede Tuer und jedes Pendel
// an.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointLedgerWorldAnchorTest,
	"JointLedger.Rules.WorldAnchorIsNotAFinding",
	JointLedgerTests::TestFlags)

bool FJointLedgerWorldAnchorTest::RunTest(const FString&)
{
	using namespace JointLedgerTests;
	const FJointBudget Budget;

	const FJointRecord Anker = WeltAnker();
	TestTrue(TEXT("eine leere Seite ist ein Welt-Anker"),
		UJointLedgerStatics::IsWorldAnchor(Anker));
	TestEqual(TEXT("und damit KEIN Befund"),
		UJointLedgerStatics::ClassifyRecord(Anker, Budget), EJointFault::None);

	// GEGENPROBE: beide Seiten leer ist KEIN Anker. Ohne sie wuerde `IsWorldAnchor` auch dann
	// gruenen, wenn es schlicht „irgendwo ist etwas leer" pruefte.
	FJointRecord Beide = Anker;
	Beide.Frame1 = EJointFrameState::Empty;
	TestFalse(TEXT("beide Seiten leer ist kein Anker"),
		UJointLedgerStatics::IsWorldAnchor(Beide));

	// UND DIE ANDERE RICHTUNG — SIE HAT GEFEHLT, UND DIE SABOTAGEPROBE HAT ES GEZEIGT.
	//
	// `IsBothFramesEmpty` liess sich von `&&` auf `||` verfaelschen, ohne dass ein einziger
	// Test anschlug: geprueft war nur, dass zwei leere Seiten `true` ergeben. Dass EINE leere
	// Seite `false` ergeben muss, hat niemand verlangt — und in `ClassifyRecord` faellt es
	// nicht auf, weil der Welt-Anker-Zweig vorher greift und den Unterschied verdeckt.
	TestFalse(TEXT("eine einzelne leere Seite ist NICHT 'beide leer'"),
		UJointLedgerStatics::IsBothFramesEmpty(Anker));
	TestTrue(TEXT("zwei leere Seiten sind es sehr wohl"),
		UJointLedgerStatics::IsBothFramesEmpty(Beide));

	// Und wer es doch gemeldet haben will, bekommt es — aber nur ausdruecklich.
	FJointBudget Streng;
	Streng.bWorldAnchorIsAFinding = true;
	TestNotEqual(TEXT("mit bWorldAnchorIsAFinding wird daraus ein Befund"),
		UJointLedgerStatics::ClassifyRecord(Anker, Streng), EJointFault::None);
	return true;
}

// ---------------------------------------------------------------------------------------
// 2. DIE ENTHALTUNG STEHT VOR ALLEM ANDEREN.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointLedgerAbstentionTest,
	"JointLedger.Rules.AbstentionComesFirst",
	JointLedgerTests::TestFlags)

bool FJointLedgerAbstentionTest::RunTest(const FString&)
{
	using namespace JointLedgerTests;
	const FJointBudget Budget;

	FJointRecord Kurz = Gesund();
	Kurz.Frame1 = EJointFrameState::Empty;
	Kurz.Frame2 = EJointFrameState::Empty;
	Kurz.ObservedSeconds = 1.f;          // unter MinObservedSeconds (5 s)

	TestFalse(TEXT("zu kurz beobachtet ist nicht beurteilbar"),
		UJointLedgerStatics::IsJudgeable(Kurz, Budget));
	TestEqual(TEXT("und wird zur Enthaltung, NICHT zum Fehler"),
		UJointLedgerStatics::ClassifyRecord(Kurz, Budget), EJointFault::Unjudged);

	// Derselbe Eintrag, nur lange genug gesehen: jetzt ist es ein Fehler. Ohne diese Haelfte
	// wuerde der Test auch gruenen, wenn die Klassifizierung IMMER Unjudged saegte.
	Kurz.ObservedSeconds = 10.f;
	TestEqual(TEXT("lange genug beobachtet wird derselbe Eintrag zum Fehler"),
		UJointLedgerStatics::ClassifyRecord(Kurz, Budget), EJointFault::BothFramesEmpty);

	// Ein Eintrag ohne Besitzer ist eine Huelse und wird nie beurteilt.
	FJointRecord Huelse = Gesund();
	Huelse.OwnerName = NAME_None;
	TestFalse(TEXT("ohne Besitzer nicht beurteilbar"),
		UJointLedgerStatics::IsJudgeable(Huelse, Budget));
	return true;
}

// ---------------------------------------------------------------------------------------
// 3. DIE URSACHE GEWINNT GEGEN IHRE FOLGE.
//
// Beide Seiten leer IST der Grund, warum kein Constraint entstand. Meldete das Werkzeug
// „nie erzeugt", suchte der Leser ein Physikproblem statt zwei leerer Felder.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointLedgerCauseBeforeEffectTest,
	"JointLedger.Rules.CauseBeatsEffect",
	JointLedgerTests::TestFlags)

bool FJointLedgerCauseBeforeEffectTest::RunTest(const FString&)
{
	using namespace JointLedgerTests;
	const FJointBudget Budget;

	FJointRecord Leer = Gesund();
	Leer.Frame1 = EJointFrameState::Empty;
	Leer.Frame2 = EJointFrameState::Empty;
	Leer.bWasEverValid = false;          // beides trifft zu

	TestTrue(TEXT("beide Seiten leer"), UJointLedgerStatics::IsBothFramesEmpty(Leer));
	TestEqual(TEXT("gemeldet wird die URSACHE, nicht die Folge"),
		UJointLedgerStatics::ClassifyRecord(Leer, Budget), EJointFault::BothFramesEmpty);

	// `IsNeverInitialised` darf hier gar nicht erst zuschlagen: ohne aufgeloeste Seite ist
	// „nie erzeugt" kein eigener Befund.
	TestFalse(TEXT("ohne aufgeloeste Seite ist 'nie erzeugt' kein eigener Befund"),
		UJointLedgerStatics::IsNeverInitialised(Leer));
	return true;
}

// ---------------------------------------------------------------------------------------
// 4. DIE PRAEZISERE ADRESSE GEWINNT: ein Tippfehler schlaegt „nie erzeugt".
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointLedgerNameBeforeHandleTest,
	"JointLedger.Rules.NameBeatsHandle",
	JointLedgerTests::TestFlags)

bool FJointLedgerNameBeforeHandleTest::RunTest(const FString&)
{
	using namespace JointLedgerTests;
	const FJointBudget Budget;

	FJointRecord Tippfehler = Gesund();
	Tippfehler.Frame2 = EJointFrameState::Missing;
	Tippfehler.WantedName2 = TEXT("lowerarm_I");   // grosses i statt kleines L
	Tippfehler.ResolvedName2 = NAME_None;
	Tippfehler.bWasEverValid = false;              // beides trifft zu

	TestTrue(TEXT("ein Name zeigt ins Leere"),
		UJointLedgerStatics::IsNamedBodyMissing(Tippfehler));
	TestTrue(TEXT("und das Constraint kam nie zustande"),
		UJointLedgerStatics::IsNeverInitialised(Tippfehler));
	TestEqual(TEXT("gemeldet wird der NAME, denn den kann man korrigieren"),
		UJointLedgerStatics::ClassifyRecord(Tippfehler, Budget),
		EJointFault::NamedBodyMissing);

	// Und ohne den Tippfehler bleibt der andere Befund uebrig — sonst koennte
	// `NeverInitialised` tot sein, ohne dass es auffaellt.
	FJointRecord NurHandle = Gesund();
	NurHandle.bWasEverValid = false;
	TestEqual(TEXT("ohne Tippfehler bleibt 'nie erzeugt'"),
		UJointLedgerStatics::ClassifyRecord(NurHandle, Budget),
		EJointFault::NeverInitialised);
	return true;
}

// ---------------------------------------------------------------------------------------
// 5. „NICHTS SIMULIERT" IST EINE WARNUNG — und trifft den Welt-Anker nicht per Bauart.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointLedgerSleepingTest,
	"JointLedger.Rules.NothingSimulates",
	JointLedgerTests::TestFlags)

bool FJointLedgerSleepingTest::RunTest(const FString&)
{
	using namespace JointLedgerTests;
	const FJointBudget Budget;

	FJointRecord Schlaeft = Gesund();
	Schlaeft.bBody1EverSimulated = false;
	Schlaeft.bBody2EverSimulated = false;
	TestEqual(TEXT("zwei statische Koerper sind eine Warnung"),
		UJointLedgerStatics::ClassifyRecord(Schlaeft, Budget), EJointFault::NothingSimulates);

	// DER WELT-ANKER DARF DAVON NICHT GETROFFEN WERDEN, SOLANGE SEIN KOERPER SIMULIERT.
	// Die Welt simuliert nie; wer beide Seiten prueft, meldet jeden Anker als schlafend.
	const FJointRecord Anker = WeltAnker();
	TestFalse(TEXT("ein Anker mit simulierendem Koerper schlaeft nicht"),
		UJointLedgerStatics::IsNothingSimulating(Anker));

	// Schlaeft der Koerper des Ankers aber wirklich, ist es doch eine Warnung.
	FJointRecord AnkerSchlaeft = Anker;
	AnkerSchlaeft.bBody1EverSimulated = false;
	TestEqual(TEXT("ein Anker, dessen eigener Koerper nie simuliert, wird gemeldet"),
		UJointLedgerStatics::ClassifyRecord(AnkerSchlaeft, Budget),
		EJointFault::NothingSimulates);
	return true;
}

// ---------------------------------------------------------------------------------------
// 6. EHRLICHE ZAHLEN: ohne Nenner keine Quote.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointLedgerHonestNumbersTest,
	"JointLedger.Rules.HonestNumbers",
	JointLedgerTests::TestFlags)

bool FJointLedgerHonestNumbersTest::RunTest(const FString&)
{
	using namespace JointLedgerTests;
	const FJointBudget Budget;

	TArray<FJointRecord> Leer;
	TestTrue(TEXT("ohne Eintraege gibt es keine Quote, sondern -1"),
		UJointLedgerStatics::HealthyShare(Leer, Budget) < 0.f);

	// Nur Enthaltungen sind auch kein Nenner.
	FJointRecord Kurz = Gesund();
	Kurz.ObservedSeconds = 1.f;
	TArray<FJointRecord> NurEnthaltung = { Kurz };
	TestTrue(TEXT("nur Enthaltungen ergeben ebenfalls keine Quote"),
		UJointLedgerStatics::HealthyShare(NurEnthaltung, Budget) < 0.f);

	// Zwei Eintraege, einer gesund: 50 %. Ohne diese Haelfte koennte die Funktion immer -1
	// liefern und der Test trotzdem gruenen.
	FJointRecord Kaputt = Gesund();
	Kaputt.Frame1 = EJointFrameState::Empty;
	Kaputt.Frame2 = EJointFrameState::Empty;
	TArray<FJointRecord> Zwei = { Gesund(), Kaputt };
	TestEqual(TEXT("ein gesunder von zweien sind 50 Prozent"),
		UJointLedgerStatics::HealthyShare(Zwei, Budget), 50.f);
	return true;
}

// ---------------------------------------------------------------------------------------
// 7. DAS URTEIL — und dass ein abgeschalteter Fehler zur WARNUNG wird, nicht zum Schweigen.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointLedgerVerdictTest,
	"JointLedger.Rules.Verdict",
	JointLedgerTests::TestFlags)

bool FJointLedgerVerdictTest::RunTest(const FString&)
{
	using namespace JointLedgerTests;
	const FJointBudget Budget;

	TArray<FJointRecord> Gut = { Gesund() };
	TestEqual(TEXT("ein gesundes Gelenk besteht"),
		UJointLedgerStatics::Judge(Gut, Budget, true), EJointVerdict::Pass);

	// EIN LAUF OHNE CONSTRAINT IST KEIN BESTEHEN.
	TArray<FJointRecord> Nichts;
	TestEqual(TEXT("kein Constraint gesehen ist ein Fehler, kein gruenes Nichts"),
		UJointLedgerStatics::Judge(Nichts, Budget, false), EJointVerdict::Fail);

	FJointRecord Leer = Gesund();
	Leer.Frame1 = EJointFrameState::Empty;
	Leer.Frame2 = EJointFrameState::Empty;
	TArray<FJointRecord> MitFehler = { Leer };
	TestEqual(TEXT("zwei leere Seiten sind ein Fehler"),
		UJointLedgerStatics::Judge(MitFehler, Budget, true), EJointVerdict::Fail);

	// ABGESCHALTET HEISST WARNUNG, NICHT SCHWEIGEN.
	FJointBudget Weich;
	Weich.bBothFramesEmptyIsError = false;
	TestEqual(TEXT("abgeschaltet wird daraus eine Warnung"),
		UJointLedgerStatics::Judge(MitFehler, Weich, true), EJointVerdict::Warn);

	TestEqual(TEXT("PASS -> 0"),
		UJointLedgerStatics::VerdictExitCode(EJointVerdict::Pass), 0);
	TestEqual(TEXT("WARN -> 1"),
		UJointLedgerStatics::VerdictExitCode(EJointVerdict::Warn), 1);
	TestEqual(TEXT("FAIL -> 2"),
		UJointLedgerStatics::VerdictExitCode(EJointVerdict::Fail), 2);
	return true;
}

// ---------------------------------------------------------------------------------------
// 8. DIE NOTIZ NENNT DIE VORHANDENEN NAMEN — und weist die Kappung aus.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointLedgerNoteTest,
	"JointLedger.Report.NoteNamesTheBodies",
	JointLedgerTests::TestFlags)

bool FJointLedgerNoteTest::RunTest(const FString&)
{
	using namespace JointLedgerTests;
	const FJointBudget Budget;

	FJointRecord R = Gesund();
	R.Frame2 = EJointFrameState::Missing;
	R.WantedName2 = TEXT("lowerarm_I");
	R.ResolvedName2 = NAME_None;
	R.AvailableBodyNames = { FName(TEXT("upperarm_l")), FName(TEXT("lowerarm_l")) };
	R.BodyNamesOmitted = 3;

	const FString Notiz = UJointLedgerStatics::FaultText(R, Budget);
	TestTrue(TEXT("die Notiz nennt den gesuchten Namen"), Notiz.Contains(TEXT("lowerarm_I")));
	TestTrue(TEXT("und die vorhandenen"), Notiz.Contains(TEXT("lowerarm_l")));
	// EINE STILLE KAPPUNG WAERE EINE LUEGE: der Leser haelt die Liste sonst fuer vollstaendig.
	TestTrue(TEXT("und weist die Kappung aus"), Notiz.Contains(TEXT("3 more")));

	// Beim Welt-Anker sagt die Notiz ausdruecklich, dass das so gemeint ist.
	const FString AnkerNotiz = UJointLedgerStatics::FaultText(WeltAnker(), Budget);
	TestTrue(TEXT("der Anker wird als Absicht ausgewiesen"),
		AnkerNotiz.Contains(TEXT("anchor to the world")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
