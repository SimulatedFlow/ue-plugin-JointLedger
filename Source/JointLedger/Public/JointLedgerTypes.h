// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JointLedgerTypes.generated.h"

/**
 * Die Befundarten.
 *
 * DREI FEHLER, EINE WARNUNG, EINE ENTHALTUNG — und was NICHT dabei ist, ist genauso wichtig:
 * ein einzelner leerer Frame. Die Engine nennt das in ihrem eigenen Kommentar „joint to
 * 'world'" (PhysicsConstraintComponent.cpp:110), und ein Werkzeug, das jeden leeren Frame
 * anschwaerzt, meldet massenhaft voellig richtige Welt-Anker. Erst wenn BEIDE Seiten leer
 * sind, haelt das Gelenk wirklich nichts.
 */
UENUM(BlueprintType)
enum class EJointFault : uint8
{
	/** Kein Befund: das Gelenk kam zustande und haelt etwas. */
	None UMETA(DisplayName = "None"),

	/**
	 * DER EIGENTLICHE FUND: beide Seiten zeigen ins Leere.
	 *
	 * Kein Actor, kein Komponentenname, kein Override — auf keiner der beiden Seiten. Die
	 * Engine ruft `InitConstraint` dann gar nicht erst auf: der Aufruf haengt an
	 * `if (Object1 || Object2)` (PhysicsConstraintComponent.cpp:344). Es gibt keine Meldung,
	 * keine Warnung, kein `ensure` — und `bIsBrokenLocal` wird in derselben Funktion trotzdem
	 * auf `false` gesetzt (:349), weshalb `IsBroken()` sauber „nein" antwortet.
	 *
	 * EIN GELENK, DAS NIE ZUSTANDE KAM, SIEHT VON AUSSEN AUS WIE EIN GESUNDES. FEHLER.
	 */
	BothFramesEmpty UMETA(DisplayName = "Both frames resolve to nothing"),

	/**
	 * Ein Name ist gesetzt, aber es gibt keine Komponente, die so heisst.
	 *
	 * Die Engine laeuft `Actor->GetComponents()` durch und vergleicht auf `GetFName()`
	 * (PhysicsConstraintComponent.cpp:130-146). Passt nichts, bleibt der Zeiger schlicht NULL
	 * — ohne ein Wort. Das ist der Tippfehler-Fall, und er ist der wertvollste Befund dieses
	 * Werkzeugs, weil die BEHEBUNG EIN NAME IST.
	 *
	 * Deshalb nennt der Befund den gesuchten Namen UND die Namen, die der Actor wirklich hat.
	 * FEHLER.
	 */
	NamedBodyMissing UMETA(DisplayName = "Named body does not exist on that actor"),

	/**
	 * Die Koerper sind aufloesbar, das Constraint gibt es trotzdem nicht.
	 *
	 * `FConstraintInstance::IsValidConstraintInstance()` ist schlicht `ConstraintHandle
	 * .IsValid()` (ConstraintInstance.cpp:704). Ist der Griff ungueltig, obwohl beide Seiten
	 * auf etwas zeigen, ist die Initialisierung unterwegs gescheitert — etwa weil die
	 * Komponente zu diesem Zeitpunkt noch keinen Physikkoerper hatte.
	 *
	 * DAS IST DER ZWEITE, UNABHAENGIGE MESSWEG. Der Befund oben schliesst aus den Namen, dieser
	 * misst das Ergebnis. Zwei Wege zum selben Zustand sind der Grund, warum ein Fehlschluss
	 * hier auffaellt statt durchzurutschen. FEHLER.
	 */
	NeverInitialised UMETA(DisplayName = "Constraint instance was never created"),

	/**
	 * Das Gelenk haelt etwas — nur bewegt sich nichts davon je.
	 *
	 * Beide Koerper existieren, aber keiner simuliert Physik. Ein Constraint zwischen zwei
	 * statischen Koerpern kostet Aufbau und wirkt nie.
	 *
	 * KEIN FEHLER, und das mit Bedacht: ein Ragdoll wird oft erst spaet scharfgeschaltet, und
	 * bis dahin ist genau dieser Zustand richtig. Deshalb WARNUNG — und deshalb zaehlt hier
	 * `MinObservedSeconds`, damit ein kurzer Lauf nicht jedes schlafende Ragdoll anschwaerzt.
	 */
	NothingSimulates UMETA(DisplayName = "Neither body simulates physics"),

	/**
	 * Eine Seite haengt an der Welt — gemeldet NUR auf ausdrueckliches Verlangen.
	 *
	 * DIESE BEFUNDART WURDE VOM TEST ERZWUNGEN, nicht vom Entwurf: das Budget trug seit
	 * Anfang an den Schalter `bWorldAnchorIsAFinding`, aber es gab gar nichts, was er haette
	 * melden koennen. Der Anker fiel durch alle Befunde durch und landete bei `None` — ein
	 * Schalter, der nichts tut, ist schlimmer als keiner, weil sein Besitzer glaubt, er habe
	 * etwas eingeschaltet.
	 *
	 * IMMER NUR WARNUNG, nie Fehler: ein Welt-Anker ist ein bewusst gewaehlter Zustand. Wer
	 * ihn sehen will, will eine Liste — kein rotes Tor.
	 */
	WorldAnchor UMETA(DisplayName = "Anchored to the world"),

	/**
	 * ENTHALTUNG, KEIN FEHLER. Zwei Faelle:
	 *  1. Das Constraint wurde kuerzer beobachtet als `MinObservedSeconds` — „nie simuliert"
	 *     braucht einen Nenner.
	 *  2. Der besitzende Actor war waehrend der Beobachtung nie vollstaendig da (etwa frisch
	 *     gespawnt), sodass die Aufloesung nichts ueber den Endzustand aussagt.
	 */
	Unjudged UMETA(DisplayName = "Not judged")
};

/** Das Urteil ueber den ganzen Lauf. Dieselben drei Stufen wie in allen Ledger-Werkzeugen. */
UENUM(BlueprintType)
enum class EJointVerdict : uint8
{
	Pass UMETA(DisplayName = "PASS"),
	Warn UMETA(DisplayName = "WARN"),
	Fail UMETA(DisplayName = "FAIL")
};

/**
 * Die Schwellen. Jede einzelne ist eine Stelle, an der jemand anderer Meinung sein darf —
 * deshalb stehen sie hier und nicht verstreut im Code.
 */
USTRUCT(BlueprintType)
struct JOINTLEDGER_API FJointBudget
{
	GENERATED_BODY()

	/**
	 * Wie viele Gelenke mit zwei leeren Seiten das Urteil ueberleben.
	 * Voreinstellung 0: dieser Fall ist immer ein Fehler, er sieht nur nicht danach aus.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger", meta = (ClampMin = "0"))
	int32 MaxBothFramesEmpty = 0;

	/** Dasselbe fuer Namen, die auf keine Komponente zeigen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger", meta = (ClampMin = "0"))
	int32 MaxNamedBodyMissing = 0;

	/** Und fuer Constraints, die trotz aufloesbarer Koerper nie entstanden sind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger", meta = (ClampMin = "0"))
	int32 MaxNeverInitialised = 0;

	/**
	 * Wie viele schlafende Gelenke geduldet werden, bevor gewarnt wird. Nicht 0: ein Ragdoll,
	 * das in dieser Sitzung nie scharfgeschaltet wurde, ist kein Mangel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger", meta = (ClampMin = "0"))
	int32 MaxNothingSimulates = 4;

	/**
	 * WIE LANGE EIN CONSTRAINT BEOBACHTET SEIN MUSS, BEVOR „NIE" GESAGT WERDEN DARF.
	 *
	 * HIER IST DER NENNER DIE SITZUNG, nicht die Dauer eines Einzelvorgangs — anders als bei
	 * SlotLedger, wo gegen die Spieldauer eines Montages gemessen wird. Ein Constraint ist
	 * kein Vorgang, sondern ein Zustand; was sich aendern kann, ist allein das Simulieren.
	 * Deshalb dieselben 5 s wie bei LayerLedger und nicht die 0,2 s von SlotLedger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger", meta = (ClampMin = "0.0"))
	float MinObservedSeconds = 5.0f;

	/**
	 * IST EIN EINZELNER LEERER FRAME EIN BEFUND?
	 *
	 * VOREINSTELLUNG NEIN, und das ist die wichtigste Entscheidung in dieser Datei. Die Engine
	 * koppelt einen Koerper mit genau diesem Mittel an die Welt — ihr eigener Kommentar lautet
	 * „If neither actor nor component name specified, joint to 'world'"
	 * (PhysicsConstraintComponent.cpp:110). Wer das meldet, schwaerzt eine gaengige und
	 * richtige Technik an; SocketLedger waere an derselben Stelle beinahe ueber `hand_r`
	 * gestolpert.
	 *
	 * Einschaltbar fuer Projekte, die grundsaetzlich ohne Welt-Anker arbeiten — dann aber
	 * bewusst und sichtbar, nicht aus Versehen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	bool bWorldAnchorIsAFinding = false;

	/** Ist „nichts simuliert" ueberhaupt eine Warnung? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	bool bReportNothingSimulates = true;

	/** Zwei leere Seiten sind ein Fehler. Abschaltbar, aber nie zu einem stillen Bestehen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	bool bBothFramesEmptyIsError = true;

	/**
	 * Ein Lauf, in dem es ueberhaupt kein Constraint gab, ist KEIN Bestehen — er hat nichts
	 * geprueft. Derselbe gefaehrliche gruene Nullwert wie eine Karte ohne Data Layer bei
	 * LayerLedger oder ein Lauf ohne Montage bei SlotLedger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	bool bNoConstraintsIsAnError = true;

	/**
	 * Sollen Enthaltungen im Bericht auftauchen? Voreinstellung ja — eine Enthaltung, die man
	 * nicht sieht, ist von einem uebersehenen Gelenk nicht zu unterscheiden.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	bool bReportUnjudged = true;

	/** Obergrenze fuer verfolgte Constraints, damit eine volle Szene das Werkzeug nicht traegt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger", meta = (ClampMin = "1"))
	int32 MaxTrackedConstraints = 512;

	/**
	 * Wie viele vorhandene Komponentennamen der Befund hoechstens auflistet.
	 *
	 * Die Liste ist der GRUND des Tippfehler-Befundes — ohne sie steht dort nur ein Symptom.
	 * Ein Actor mit fuenfzig Komponenten wuerde den Bericht sprengen, also wird gekappt und
	 * die Kappung ausgewiesen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger", meta = (ClampMin = "1"))
	int32 MaxBodyNamesListed = 8;
};

/**
 * Was eine Seite des Gelenks vorgefunden hat.
 *
 * DIE DREI ZUSTAENDE SIND NICHT DASSELBE, und genau hier entscheidet sich, ob das Werkzeug
 * taugt: „nichts angegeben" ist ein Welt-Anker, „angegeben und gefunden" ist der Normalfall,
 * „angegeben und NICHT gefunden" ist der Tippfehler. Wer die erste und die dritte Lage
 * zusammenwirft, meldet Welt-Anker als Fehler.
 */
UENUM(BlueprintType)
enum class EJointFrameState : uint8
{
	/** Weder Actor noch Name noch Override: die Engine koppelt diese Seite an die Welt. */
	Empty UMETA(DisplayName = "Empty (world anchor)"),

	/** Angegeben und aufgeloest. */
	Resolved UMETA(DisplayName = "Resolved"),

	/** Angegeben, aber es gibt keine Komponente dieses Namens. */
	Missing UMETA(DisplayName = "Named but missing")
};

/**
 * Ein Constraint, ueber die ganze Sitzung.
 *
 * DER SCHLUESSEL IST (Actor, Constraint-Komponente) — nicht der Actor allein. Ein Ragdoll
 * traegt Dutzende Constraints auf demselben Actor; wer nur den Actor zaehlt, mittelt genau
 * den Befund weg, um den es geht.
 */
USTRUCT(BlueprintType)
struct JOINTLEDGER_API FJointRecord
{
	GENERATED_BODY()

	/** Die Constraint-Komponente, wie sie heisst — die Adresse des Befundes. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	FName ConstraintName;

	/** Der Actor, auf dem es passiert ist. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	FName OwnerName;

	/** Was Seite 1 vorgefunden hat. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	EJointFrameState Frame1 = EJointFrameState::Empty;

	/** Was Seite 2 vorgefunden hat. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	EJointFrameState Frame2 = EJointFrameState::Empty;

	/** Der gesuchte Name auf Seite 1 — leer, wenn keiner angegeben war. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	FName WantedName1;

	/** Der gesuchte Name auf Seite 2. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	FName WantedName2;

	/** Die aufgeloeste Komponente auf Seite 1 — leer, wenn nichts gefunden wurde. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	FName ResolvedName1;

	/** Die aufgeloeste Komponente auf Seite 2. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	FName ResolvedName2;

	/**
	 * Die Komponentennamen, die der Actor wirklich hat — gekappt auf `MaxBodyNamesListed`.
	 *
	 * Das ist der GRUND des Tippfehler-Befundes. Ohne diese Liste steht dort „Koerper nicht
	 * gefunden", und der Leser sucht selbst; mit ihr steht dort, wonach er suchen muss.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	TArray<FName> AvailableBodyNames;

	/** Wie viele Namen wegen der Kappung fehlen. Eine stille Kappung waere eine Luege. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	int32 BodyNamesOmitted = 0;

	/**
	 * War das Constraint zu irgendeinem Zeitpunkt wirklich vorhanden?
	 *
	 * Gemessen an `IsValidConstraintInstance()`, also am Griff selbst — und nicht daraus
	 * geschlossen, dass die Namen passten. Das HOECHSTE je gesehene Ergebnis wird behalten,
	 * nicht das letzte: ein Constraint, das erst spaet entsteht, war trotzdem da.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	bool bWasEverValid = false;

	/** Hat Seite 1 waehrend der Sitzung je simuliert? */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	bool bBody1EverSimulated = false;

	/** Hat Seite 2 waehrend der Sitzung je simuliert? */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	bool bBody2EverSimulated = false;

	/** Wie lange dieses Constraint beobachtet wurde. Der Nenner jeder „nie"-Aussage. */
	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	float ObservedSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "JointLedger")
	EJointFault Fault = EJointFault::None;
};
