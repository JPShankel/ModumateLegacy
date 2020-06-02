// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/UserWidgetPool.h"
#include "ModumateCore/ModumateTypes.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "HUDDrawWidget.generated.h"

USTRUCT(BlueprintType)
struct FModumateLines
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector Point1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector Point2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float AngleDegrees;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool Visible = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FLinearColor Color = FLinearColor(1.0, 1.0, 1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	TArray<FString> OutStrings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector TextLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float TextRotationDegree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	AActor* OwnerActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool EnterableText = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	EEnterableField Functionality = EEnterableField::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	int32 DraftAestheticType = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FName UniqueID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool AntiAliased = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float Thickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float DashLength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float DashSpacing = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	int32 Priority = 0;

	bool operator<(const FModumateLines &Other) const
	{
		return Priority < Other.Priority;
	}
};

/**
 *
 */
UCLASS()
class MODUMATE_API UHUDDrawWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHUDDrawWidget(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UUserWidget Interface

	UPROPERTY()
	TArray<FModumateLines> LinesToDrawOnPaint;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WidgetTag)
	TArray<FVector> HoverTagLocations;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FModumateLines> LinesToDraw;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FModumateLines> LinesWithText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FModumateLines> LinesWithTextEditable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FModelDimensionString> DegreeStringToDraw;

	UFUNCTION(BlueprintImplementableEvent, Category = "Tools")
	void ShowRoomInfoWidget(int32 RoomID);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tools")
	void ClearRoomInfoWidget(int32 RoomID);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tools")
	void DrawLines(const TArray<FModumateLines>& Lines);

public:

	UPROPERTY(Transient)
	FUserWidgetPool UserWidgetPool;
};
