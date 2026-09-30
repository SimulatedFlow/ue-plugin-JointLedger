// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "JointLedgerHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "JointLedgerLog.h"
#include "JointLedgerSettings.h"
#include "JointLedgerStatics.h"
#include "JointLedgerSubsystem.h"

AJointLedgerHUD::AJointLedgerHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AJointLedgerHUD::ToggleOverlay()
{
	// Die Sichtbarkeit haengt an der Einstellung, nicht an einem zweiten Schalter im HUD —
	// sonst gibt es zwei Wahrheiten darueber, ob der Kasten zu sehen sein soll.
	UJointLedgerSettings* S = GetMutableDefault<UJointLedgerSettings>();
	S->bShowOverlay = !S->bShowOverlay;
}

void AJointLedgerHUD::DrawHUD()
{
	Super::DrawHUD();

	if (Canvas && UJointLedgerSettings::Get()->bShowOverlay)
	{
		DrawPanel(Canvas, PanelOrigin, PanelWidth);
	}
}

void AJointLedgerHUD::DrawPanel(UCanvas* InCanvas, const FVector2D& Origin, float Width)
{
	if (!InCanvas)
	{
		return;
	}
	UJointLedgerSubsystem* S = UJointLedgerSubsystem::Get(this);
	if (!S)
	{
		return;
	}
	UFont* Schrift = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Schrift)
	{
		return;
	}

	const FJointBudget Budget = S->GetBudget();

	// SCHON SORTIERT — `GetFindingsBySeverity` tut das, und zwar fuer Kasten, Bericht und
	// Kopfzeile gemeinsam. Eine eigene Rangfolge im Kasten waere die erste Stelle, an der
	// Anzeige und Urteil auseinanderlaufen.
	TArray<FJointRecord> MitUrteil = S->GetFindingsBySeverity();
	MitUrteil.RemoveAll([&Budget](const FJointRecord& R)
	{
		// Enthaltungen stehen im Bericht, aber nicht in der Fundliste des Kastens: sie sind
		// kein Befund, und zwischen echten Befunden gelesen sehen sie wie welche aus.
		return UJointLedgerStatics::ClassifyRecord(R, Budget) == EJointFault::Unjudged;
	});

	// Die Grenzen sind lang und mehrzeilig. Sie werden UMGEBROCHEN und nicht abgeschnitten:
	// Abschneiden waere die bequemste Art, eine Einschraenkung verschwinden zu lassen.
	TArray<FString> GrenzTeile;
	S->LimitsLine().ParseIntoArrayLines(GrenzTeile, false);

	const int32 ZeilenGezeigt = FMath::Min(MitUrteil.Num(), FMath::Max(1, MaxRecordRows));
	const int32 ZeilenVersteckt = FMath::Max(0, MitUrteil.Num() - ZeilenGezeigt);

	const float Zeile = 15.f;
	const float Kopf = Zeile * (2.f + GrenzTeile.Num() + (Note.IsEmpty() ? 0.f : 1.f));
	const float ZeilenBlock = Zeile * (1.f + FMath::Max(1, ZeilenGezeigt)
		+ (ZeilenVersteckt > 0 ? 1.f : 0.f));
	const float Hoehe = 8.f + Kopf + ZeilenBlock + 10.f;

	FCanvasTileItem Kasten(FVector2D(Origin.X, Origin.Y), FVector2D(Width, Hoehe),
		FLinearColor(0.02f, 0.04f, 0.09f, 0.82f));
	Kasten.BlendMode = SE_BLEND_Translucent;
	InCanvas->DrawItem(Kasten);

	float Y = Origin.Y + 6.f;
	const float X = Origin.X + 10.f;
	auto Schreib = [&](const FString& Text, const FLinearColor& Farbe)
	{
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Schrift, Farbe);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		InCanvas->DrawItem(Item);
		Y += Zeile;
	};

	const FLinearColor Matt(0.62f, 0.70f, 0.78f);
	const FLinearColor Weiss(0.86f, 0.92f, 0.98f);
	const FLinearColor Gruen(0.38f, 0.92f, 0.52f);

	Schreib(S->SummaryLine(), UJointLedgerStatics::VerdictColour(S->GetVerdict()));

	// Der schwerste Befund im Klartext. Er ist der Grund, das Plugin zu behalten, und gehoert
	// nicht in einer Tabellenspalte vergraben.
	{
		const FString Dom = S->DominantLine();
		if (!Dom.IsEmpty())
		{
			TArray<FString> DomTeile;
			Dom.ParseIntoArrayLines(DomTeile, false);
			for (const FString& T : DomTeile)
			{
				Schreib(T, Weiss);
			}
		}
		else if (S->HasSeenAnyConstraint())
		{
			Schreib(TEXT("every constraint in this run resolved to real bodies and came into "
				"existence"), Gruen);
		}
		else
		{
			// KEIN CONSTRAINT HEISST NICHT „ALLES GUT". Das ist der gefaehrlichste gruene
			// Nullwert, den ein pruefendes Werkzeug liefern kann.
			Schreib(TEXT("no physics constraint in this run - nothing was checked, and that is "
				"not a pass"), UJointLedgerStatics::VerdictColour(EJointVerdict::Fail));
		}
	}

	for (const FString& T : GrenzTeile)
	{
		Schreib(T, Matt);
	}

	if (!Note.IsEmpty())
	{
		Schreib(Note, FLinearColor(1.00f, 0.84f, 0.35f));
	}

	Schreib(UJointLedgerStatics::RecordHeader(), Matt);

	if (MitUrteil.Num() == 0)
	{
		Schreib(TEXT("   (no findings)"), Gruen);
	}
	else
	{
		for (int32 i = 0; i < ZeilenGezeigt; ++i)
		{
			Schreib(UJointLedgerStatics::RecordLine(MitUrteil[i], Budget),
				UJointLedgerStatics::FaultColour(
					UJointLedgerStatics::ClassifyRecord(MitUrteil[i], Budget)));
		}
		if (ZeilenVersteckt > 0)
		{
			Schreib(FString::Printf(
				TEXT("   ... and %d more constraint(s) with a finding - the full list is in the "
					 "report"), ZeilenVersteckt), Matt);
		}
	}
}

FString AJointLedgerHUD::SavePanelImage(int32 Width, int32 Height, const FString& FileName)
{
	UWorld* World = GetWorld();
	if (!World || Width < 16 || Height < 16)
	{
		return FString();
	}

	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
	RT->RenderTargetFormat = RTF_RGBA8;
	RT->ClearColor = FLinearColor(0.02f, 0.04f, 0.09f, 1.f);
	RT->bAutoGenerateMips = false;
	RT->InitAutoFormat(Width, Height);
	RT->UpdateResourceImmediate(true);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, RT, FLinearColor(0.02f, 0.04f, 0.09f, 1.f));

	UCanvas* C = nullptr;
	FVector2D Groesse = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Kontext;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, RT, C, Groesse, Kontext);
	if (C)
	{
		DrawPanel(C, FVector2D(24.f, 24.f),
			FMath::Min(static_cast<float>(Width) - 48.f, PanelWidth));
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Kontext);

	const FString Ordner = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("JointLedger") / TEXT("Shots"));
	const FString Datei = FileName.IsEmpty() ? TEXT("Panel") : FileName;
	const FString Voll = Ordner / (Datei + TEXT(".png"));

	// NICHT `UKismetRenderingLibrary::ExportRenderTarget`.
	//
	// Der Weg dahinter (`FImageUtils::ExportRenderTarget2DAsPNG`) schreibt
	// `CompressedData.GetAllocatedSize()` statt `.Num()`, also die KAPAZITAET des Puffers.
	// Hinter dem PNG stehen dadurch die ungenutzten Bytes der Allokation. Bildbetrachter
	// ueberlesen das, ffmpeg nicht.
	FImage Bild;
	TArray64<uint8> PNG;
	if (!FImageUtils::GetRenderTargetImage(RT, Bild)
		|| !FImageUtils::CompressImage(PNG, TEXT("PNG"), Bild)
		|| !FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PNG.GetData(), PNG.Num()), *Voll))
	{
		UE_LOG(LogJointLedger, Warning, TEXT("JointLedger: could not write %s"), *Voll);
		return FString();
	}
	UE_LOG(LogJointLedger, Display, TEXT("JointLedger: panel image -> %s"), *Voll);
	return Voll;
}
