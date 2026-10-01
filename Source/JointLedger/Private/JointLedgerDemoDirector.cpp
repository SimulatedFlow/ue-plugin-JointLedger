// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "JointLedgerDemoDirector.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

#include "JointLedgerHUD.h"
#include "JointLedgerLog.h"
#include "JointLedgerStatics.h"
#include "JointLedgerSubsystem.h"

namespace
{
	const TCHAR* const WuerfelPfad = TEXT("/Engine/BasicShapes/Cube.Cube");
	// Das EINZIGE Engine-Material mit einem Farbparameter, den man setzen kann. Der
	// Engine-Wuerfel traegt `WorldGridMaterial`, und das hat gar keine Parameter — am
	// 29.09.2026 lief `SetVectorParameterValue` deshalb stillschweigend ins Leere.
	const TCHAR* const FarbMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial");

	/** Der Name, den `Joint_Typo` sucht — und den es mit Absicht nicht gibt. */
	const TCHAR* const TippfehlerName = TEXT("Cube_Swing_I");
}

AJointLedgerDemoDirector::AJointLedgerDemoDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SceneShot = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneShot"));
	SceneShot->SetupAttachment(GetRootComponent());
	SceneShot->bCaptureEveryFrame = false;
	SceneShot->bCaptureOnMovement = false;
	SceneShot->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneShot->bAlwaysPersistRenderingState = true;
}

void AJointLedgerDemoDirector::BeginPlay()
{
	Super::BeginPlay();
	BuildStage();
}

void AJointLedgerDemoDirector::BuildStage()
{
	if (bBuilt)
	{
		return;
	}

	UStaticMesh* Wuerfel = LoadObject<UStaticMesh>(nullptr, WuerfelPfad);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, FarbMaterial);
	if (!Wuerfel || !Material)
	{
		// LAUT ABBRECHEN, NICHT STILL WEITERLAUFEN. Eine Demo ohne Assets zeigt „0 Befunde"
		// und sieht aus wie ein sauberer Lauf.
		UE_LOG(LogJointLedger, Error,
			TEXT("JointLedger demo: assets missing (cube %d, material %d) - the stage was NOT "
				 "built, and an empty stage is not a clean run."),
			Wuerfel ? 1 : 0, Material ? 1 : 0);
		return;
	}

	auto NeuerWuerfel = [&](const TCHAR* Name, const FVector& Ort, bool bSimuliert,
		const FVector& Groesse) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* K = NewObject<UStaticMeshComponent>(this, FName(Name));
		K->SetupAttachment(RootComponent);
		K->SetRelativeLocation(Ort);
		K->SetRelativeScale3D(Groesse);
		K->SetStaticMesh(Wuerfel);
		K->SetMaterial(0, Material);
		K->SetMobility(EComponentMobility::Movable);
		K->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		K->SetSimulatePhysics(bSimuliert);
		K->RegisterComponent();
		Cubes.Add(K);
		return K;
	};

	auto NeueSaeule = [&](const TCHAR* Name, float X) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* K = NewObject<UStaticMeshComponent>(this, FName(Name));
		K->SetupAttachment(RootComponent);
		K->SetRelativeLocation(FVector(X, 260.f, 150.f));
		K->SetRelativeScale3D(FVector(0.6f, 0.6f, 3.0f));
		K->SetStaticMesh(Wuerfel);
		// ERST DAS MATERIAL TAUSCHEN, DANN DAS MID BAUEN. Andersherum haengt das MID am
		// parameterlosen WorldGridMaterial und jede Farbe verpufft lautlos.
		K->SetMaterial(0, Material);
		K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		K->RegisterComponent();
		Pillars.Add(K);
		return K;
	};

	auto NeuesGelenk = [&](const TCHAR* Name, const FVector& Ort) -> UPhysicsConstraintComponent*
	{
		UPhysicsConstraintComponent* C =
			NewObject<UPhysicsConstraintComponent>(this, FName(Name));
		C->SetupAttachment(RootComponent);
		C->SetRelativeLocation(Ort);
		C->RegisterComponent();
		Joints.Add(C);
		return C;
	};

	const float Hoehe = 380.f;

	// ========================================================================================
	// DIE X-WERTE LAUFEN ABSICHTLICH RUECKWAERTS: +540 ganz LINKS, -540 ganz RECHTS.
	// ========================================================================================
	//
	// Die Aufnahme steht bei y = -900 und blickt in **+Y**. Dabei erscheint **-X links**, und
	// wer die Stationen in aufsteigendem X aufstellt, bekommt sie im Bild spiegelverkehrt.
	// Genau das ist am 30.09.2026 passiert: der Kasten schrieb „left to right: healthy, WORLD
	// ANCHOR, holds nothing, typo", und im Bild standen die beiden roten Saeulen LINKS.
	//
	// Eine Beschriftung, die nicht zum Bild passt, ist schlimmer als gar keine — sie fuehrt
	// den Leser zuverlaessig in die Irre. Dieselbe Falle hatte SlotLedger einen Tag vorher.
	// ========================================================================================

	// ---------------------------------------------------------------- 1. gesund (links)
	UStaticMeshComponent* Traeger = NeuerWuerfel(TEXT("Cube_Beam"),
		FVector(540.f, 0.f, Hoehe), /*simuliert=*/false, FVector(1.2f, 1.2f, 0.4f));
	UStaticMeshComponent* Haenger = NeuerWuerfel(TEXT("Cube_Hanging"),
		FVector(540.f, 0.f, Hoehe - 180.f), /*simuliert=*/true, FVector(1.f));
	{
		UPhysicsConstraintComponent* C = NeuesGelenk(TEXT("Joint_Healthy"),
			FVector(540.f, 0.f, Hoehe));
		// Ueber die Namen und nicht ueber `SetConstrainedComponents`: genau diesen Weg geht
		// ein Kaeufer im Editor, und genau hier passieren die Tippfehler.
		C->ConstraintActor1 = this;
		C->ComponentName1.ComponentName = Traeger->GetFName();
		C->ConstraintActor2 = this;
		C->ComponentName2.ComponentName = Haenger->GetFName();
	}
	NeueSaeule(TEXT("Pillar_Healthy"), 540.f);

	// ---------------------------------------------------------------- 2. Welt-Anker
	UStaticMeshComponent* Pendel = NeuerWuerfel(TEXT("Cube_Swing"),
		FVector(180.f, 0.f, Hoehe - 180.f), /*simuliert=*/true, FVector(1.f));
	{
		UPhysicsConstraintComponent* C = NeuesGelenk(TEXT("Joint_WorldAnchor"),
			FVector(180.f, 0.f, Hoehe));
		C->ConstraintActor1 = this;
		C->ComponentName1.ComponentName = Pendel->GetFName();
		// SEITE 2 BLEIBT LEER — das ist der Welt-Anker, und er ist voellig richtig so.
		C->ConstraintActor2 = nullptr;
		C->ComponentName2.ComponentName = NAME_None;
	}
	NeueSaeule(TEXT("Pillar_WorldAnchor"), 180.f);

	// ---------------------------------------------------------------- 3. haelt nichts
	{
		UPhysicsConstraintComponent* C = NeuesGelenk(TEXT("Joint_HoldsNothing"),
			FVector(-180.f, 0.f, Hoehe));
		C->ConstraintActor1 = nullptr;
		C->ComponentName1.ComponentName = NAME_None;
		C->ConstraintActor2 = nullptr;
		C->ComponentName2.ComponentName = NAME_None;
	}
	NeueSaeule(TEXT("Pillar_HoldsNothing"), -180.f);

	// ---------------------------------------------------------------- 4. Tippfehler
	UStaticMeshComponent* Faellt = NeuerWuerfel(TEXT("Cube_Falling"),
		FVector(-540.f, 0.f, Hoehe - 180.f), /*simuliert=*/true, FVector(1.f));
	{
		UPhysicsConstraintComponent* C = NeuesGelenk(TEXT("Joint_Typo"),
			FVector(-540.f, 0.f, Hoehe));
		C->ConstraintActor1 = this;
		C->ComponentName1.ComponentName = Faellt->GetFName();
		C->ConstraintActor2 = this;
		// GROSSES I STATT KLEINEM L — der Tippfehler, den niemand sieht. Der Wuerfel faellt
		// einfach, und die Engine sagt kein Wort.
		C->ComponentName2.ComponentName = FName(TippfehlerName);
	}
	NeueSaeule(TEXT("Pillar_Typo"), -540.f);

	// Die Gelenke erst jetzt scharfschalten, wenn alle Komponenten stehen.
	for (UPhysicsConstraintComponent* C : Joints)
	{
		C->InitComponentConstraint();
	}

	bBuilt = true;
	UE_LOG(LogJointLedger, Display,
		TEXT("JointLedger demo: four joints on one actor - healthy, world anchor (NOT a "
			 "finding), holds nothing, and a typo ('%s' does not exist). The director expects "
			 "exactly 2 findings."), TippfehlerName);
}

void AJointLedgerDemoDirector::ColourPillars()
{
	UJointLedgerSubsystem* S = UJointLedgerSubsystem::Get(this);
	if (!S || Pillars.Num() != Joints.Num())
	{
		return;
	}
	const FJointBudget Budget = S->GetBudget();

	for (int32 i = 0; i < Joints.Num(); ++i)
	{
		const UPhysicsConstraintComponent* C = Joints[i];
		UStaticMeshComponent* P = Pillars[i];
		if (!C || !P)
		{
			continue;
		}
		// DIE FARBE KOMMT AUS DEM URTEIL DES PLUGINS, nicht aus der Erwartung des Regisseurs.
		// Eine Demo, die nach ihrer eigenen Annahme einfaerbt, sieht auch dann gruen aus,
		// wenn das Plugin falsch liegt.
		EJointFault Befund = EJointFault::Unjudged;
		for (const FJointRecord& R : S->GetRecords())
		{
			if (R.ConstraintName == C->GetFName())
			{
				Befund = UJointLedgerStatics::ClassifyRecord(R, Budget);
				break;
			}
		}
		UMaterialInstanceDynamic* MID = P->CreateAndSetMaterialInstanceDynamic(0);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"),
				UJointLedgerStatics::FaultColour(Befund));
		}
	}
}

void AJointLedgerDemoDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}

	Elapsed += DeltaSeconds;
	ColourPillars();

	if (bTakeShots)
	{
		SinceShot += DeltaSeconds;
		if (SinceShot >= ShotEverySeconds)
		{
			SinceShot = 0.f;
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (AJointLedgerHUD* HUD = Cast<AJointLedgerHUD>(PC->GetHUD()))
				{
					HUD->Note = FString::Printf(
						TEXT("demo shot %d at %.1f s - left to right: healthy, WORLD ANCHOR "
							 "(not a finding), holds nothing, typo. Pillars are coloured by the "
							 "PLUGIN's verdict."), Shots, Elapsed);
				}
			}
			SaveSceneImage(1920, 1080,
				FString::Printf(TEXT("JointLedger_Shot_%02d"), Shots));
			++Shots;
		}
	}

	if (!bReported && Elapsed >= RunSeconds)
	{
		bReported = true;
		LogComparison();
	}
}

void AJointLedgerDemoDirector::LogComparison()
{
	UJointLedgerSubsystem* S = UJointLedgerSubsystem::Get(this);
	if (!S)
	{
		return;
	}
	const FJointBudget Budget = S->GetBudget();

	// DER REGISSEUR ZAEHLT SELBST, und zwar aus dem, was er AUFGEBAUT hat — nicht aus dem
	// Bericht. Stimmen die beiden Zahlen nicht ueberein, ist das ein Befund gegen das Plugin
	// und nicht gegen die Karte.
	const int32 Erwartet = 2;          // HoldsNothing + Typo
	const int32 ErwarteteAnker = 1;    // WorldAnchor, und der darf NICHT zaehlen

	const int32 Leer = UJointLedgerStatics::CountFaults(
		S->GetRecords(), EJointFault::BothFramesEmpty, Budget);
	const int32 Fehlend = UJointLedgerStatics::CountFaults(
		S->GetRecords(), EJointFault::NamedBodyMissing, Budget);
	int32 Anker = 0;
	for (const FJointRecord& R : S->GetRecords())
	{
		if (UJointLedgerStatics::IsWorldAnchor(R))
		{
			++Anker;
		}
	}

	UE_LOG(LogJointLedger, Display,
		TEXT("JointLedger demo: director expected %d finding(s) and %d world anchor(s) that "
			 "must NOT count; the plugin reports %d both-empty + %d missing body = %d, and "
			 "sees %d anchor(s)."),
		Erwartet, ErwarteteAnker, Leer, Fehlend, Leer + Fehlend, Anker);

	if (Leer + Fehlend != Erwartet || Anker != ErwarteteAnker)
	{
		UE_LOG(LogJointLedger, Error,
			TEXT("JointLedger demo: the director and the plugin DISAGREE. That is a finding "
				 "against the plugin, not against the map."));
	}
}

FString AJointLedgerDemoDirector::SaveSceneImage(int32 Width, int32 Height,
	const FString& FileName)
{
	UWorld* Welt = GetWorld();
	if (!Welt || !SceneShot || Width < 16 || Height < 16)
	{
		return FString();
	}
	// OHNE RHI KEIN BILD — und das wird GESAGT, nicht stillschweigend uebergangen.
	// `FImageUtils::GetRenderTargetImage` liest ein Render-Target, das in einem
	// kopflosen Lauf keine Ressource hat: EXCEPTION_ACCESS_VIOLATION auf 0x0, Shell=3
	// und kein Rueckgabewert im Protokoll. Die Messung und das Tor bleiben unberuehrt.
	if (!FApp::CanEverRender())
	{
		UE_LOG(LogJointLedger, Warning,
			TEXT("JointLedger: no RHI (headless) - skipping the image. ")
			TEXT("The measurement and the gate are unaffected."));
		return FString();
	}

	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
	RT->RenderTargetFormat = RTF_RGBA8;
	RT->ClearColor = FLinearColor::Black;
	RT->bAutoGenerateMips = false;
	RT->InitAutoFormat(Width, Height);
	RT->UpdateResourceImmediate(true);
	SceneShot->TextureTarget = RT;

	SceneShot->SetWorldLocationAndRotation(ShotLocation, (ShotTarget - ShotLocation).Rotation());
	SceneShot->FOVAngle = ShotFOV;

	// ZWEIMAL. Der erste Aufruf baut den Renderzustand auf, der zweite liefert das Bild.
	SceneShot->CaptureScene();
	SceneShot->CaptureScene();

	// Der Kasten kommt in DASSELBE Ziel — ein eigenes haette durchsichtigen Grund.
	APlayerController* PC = Welt->GetFirstPlayerController();
	AJointLedgerHUD* HUD = PC ? Cast<AJointLedgerHUD>(PC->GetHUD()) : nullptr;
	if (HUD)
	{
		UCanvas* C = nullptr;
		FVector2D Groesse = FVector2D::ZeroVector;
		FDrawToRenderTargetContext Kontext;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(Welt, RT, C, Groesse, Kontext);
		if (C)
		{
			HUD->DrawPanel(C, FVector2D(24.f, 24.f),
				FMath::Min(static_cast<float>(Width) - 48.f, HUD->PanelWidth));
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(Welt, Kontext);
	}
	else
	{
		UE_LOG(LogJointLedger, Warning,
			TEXT("JointLedger demo: no JointLedgerHUD on the player controller, the shot will "
				 "have no panel. Is the game mode set on this map?"));
	}

	const FString Ordner = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("JointLedger") / TEXT("Shots"));
	const FString Voll = Ordner / ((FileName.IsEmpty() ? TEXT("Scene") : FileName) + TEXT(".png"));

	// REGEL 3.
	FImage Bild;
	TArray64<uint8> PNG;
	if (!FImageUtils::GetRenderTargetImage(RT, Bild)
		|| !FImageUtils::CompressImage(PNG, TEXT("PNG"), Bild)
		|| !FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PNG.GetData(), PNG.Num()), *Voll))
	{
		UE_LOG(LogJointLedger, Warning, TEXT("JointLedger: could not write %s"), *Voll);
		return FString();
	}
	UE_LOG(LogJointLedger, Display, TEXT("JointLedger: scene image -> %s"), *Voll);
	return Voll;
}
