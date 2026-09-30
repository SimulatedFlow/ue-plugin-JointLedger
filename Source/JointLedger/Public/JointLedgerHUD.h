// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "JointLedgerHUD.generated.h"

class UCanvas;

/**
 * Der Kasten.
 *
 * Auf `UCanvas` durch `AHUD` gezeichnet und nicht in UMG — kein Widget, kein Material, kein
 * Asset. Er uebersteht einen gekochten Shipping-Bau.
 */
UCLASS()
class JOINTLEDGER_API AJointLedgerHUD : public AHUD
{
	GENERATED_BODY()

public:
	AJointLedgerHUD();

	virtual void DrawHUD() override;

	/**
	 * Der Kasten auf ein beliebiges Canvas.
	 *
	 * Oeffentlich, weil `SavePanelImage` genau dasselbe auf ein Render-Target zeichnet: EIN
	 * Zeichenweg fuer Bildschirm und Bild. Zwei waeren zwei Wahrheiten — und die im Bild
	 * waere die, die im Laden haengt.
	 */
	UFUNCTION(BlueprintCallable, Category = "JointLedger")
	void DrawPanel(UCanvas* InCanvas, const FVector2D& Origin, float Width);

	/**
	 * Den Kasten als PNG schreiben — headless, in einem Commandlet.
	 *
	 * Nicht ueber `UKismetRenderingLibrary::ExportRenderTarget`: der Weg dahinter schreibt
	 * die KAPAZITAET des Puffers statt seiner Laenge, und hinter dem IEND stehen dann die
	 * ungenutzten Bytes. Das ist Regel 3.
	 */
	UFUNCTION(BlueprintCallable, Category = "JointLedger")
	FString SavePanelImage(int32 Width, int32 Height, const FString& FileName);

	UFUNCTION(BlueprintCallable, Category = "JointLedger")
	void ToggleOverlay();

	/** Ein Satz, den die Demo hineinschreibt, damit das Bild sagt, welche Stufe es zeigt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	FString Note;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	FVector2D PanelOrigin = FVector2D(24.f, 24.f);

	/** Breite des Kastens. Was nicht passt, wird UMGEBROCHEN und nicht abgeschnitten. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	float PanelWidth = 1870.f;

	/** Wie viele Zeilen. Gezeigt wird, was ein URTEIL hat, nicht die ersten Zeilen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JointLedger")
	int32 MaxRecordRows = 10;
};
