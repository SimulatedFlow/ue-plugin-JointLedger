// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "JointLedger.h"

#include "JointLedgerLog.h"

// ============================================================================
// COMPILE-ZEIT-BELEG FUER DAS, WAS DIE BUILD.CS BEHAUPTET UND DIE
// MACHBARKEITSPROBE GEMESSEN HAT.
// ============================================================================
//
// Die Build.cs sagt: "Engine reicht, kein PhysicsCore". Der blosse Bau des Geruests
// beweist das NICHT — solange keine Datei die Constraint-Typen anfasst, wuerde auch
// eine falsche Modulliste durchlaufen. Diese beiden Includes sind der Beleg.
//
// Und die Machbarkeitsprobe hat gemessen, dass die sechs Felder oeffentlich sind, weil
// sie nach GENERATED_UCLASS_BODY() stehen. Auch das ist eine Behauptung, solange kein
// Compiler sie prueft: auf ein privates Feld zeigt `decltype` nicht. Die Zusicherungen
// unten fallen also genau dann um, wenn eine Engine-Fassung die Sichtbarkeit aendert —
// und dann hier, beim Bau, und nicht spaeter beim Kaeufer.
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

static_assert(std::is_same_v<decltype(DeclVal<UPhysicsConstraintComponent>().ConstraintActor1),
	TObjectPtr<AActor>>, "ConstraintActor1 ist nicht mehr oeffentlich oder hat einen anderen Typ.");
static_assert(std::is_same_v<decltype(DeclVal<UPhysicsConstraintComponent>().ComponentName1),
	FConstrainComponentPropName>, "ComponentName1 ist nicht mehr oeffentlich oder hat einen anderen Typ.");
static_assert(std::is_same_v<decltype(DeclVal<UPhysicsConstraintComponent>().OverrideComponent1),
	TWeakObjectPtr<UPrimitiveComponent>>, "OverrideComponent1 ist nicht mehr oeffentlich.");
static_assert(std::is_same_v<decltype(DeclVal<UPhysicsConstraintComponent>().ConstraintInstance),
	FConstraintInstance>, "ConstraintInstance ist nicht mehr oeffentlich.");

#define LOCTEXT_NAMESPACE "FJointLedgerModule"

DEFINE_LOG_CATEGORY(LogJointLedger);

void FJointLedgerModule::StartupModule()
{
}

void FJointLedgerModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FJointLedgerModule, JointLedger)
