// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/ModumateObjectEnums.h"
#include "GameFramework/Actor.h"
#include "Objects/MOIState.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UObject/WeakObjectPtr.h"
#include "Types/SlateEnums.h"
#include "Styling/SlateWidgetStyleAsset.h"

#include "AdjustmentHandleActor.generated.h"

class AModumateObjectInstance;

class FWidgetStyle;

UCLASS()
class MODUMATE_API AAdjustmentHandleActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAdjustmentHandleActor();

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	//~ End AActor Interface

	bool bInitialized = false;

	virtual void Initialize();
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const;
	virtual void PostEndOrAbort();
	virtual void UpdateTargetGeometry();

public:
	virtual void ApplyWidgetStyle();
	virtual void SetEnabled(bool bNewEnabled);
	virtual bool BeginUse();
	virtual bool UpdateUse();
	virtual void Tick(float DeltaTime) override;
	virtual void EndUse();
	virtual void AbortUse();
	virtual bool IsInUse() const;
	virtual bool HandleInputNumber(float number);
	virtual bool HasDimensionActor();
	virtual bool HasDistanceTextInput();
	virtual bool HandleInvert();
	virtual FVector GetHandlePosition() const;
	virtual FVector GetHandleDirection() const;

	void SetTargetMOI(AModumateObjectInstance *InTargetMOI);
	void SetTargetIndex(int32 InTargetIndex);
	void SetSign(float InSign);

	UFUNCTION()
	void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	bool bEnabled = false;
	bool bIsInUse = false;

	// The MOI that created this handle
	AModumateObjectInstance* SourceMOI = nullptr;
	// The MOI that this handle effects
	AModumateObjectInstance* TargetMOI = nullptr;

	FMOIStateData TargetOriginalState;

	TArray<AModumateObjectInstance*> TargetDescendents;
	TArray<AModumateObjectInstance*> TargetConnected;

	int32 TargetIndex = INDEX_NONE;
	float Sign = 0.0f;
	FVector AnchorLoc;

	EMouseMode OriginalMouseMode;
	bool AcceptRawInputNumber = false; // Raw user input is accepted if true, else it converts to imperial unit

	// Dimension Widget
	UPROPERTY()
	class UModumateGameInstance *GameInstance;
	int32 PendingSegmentID = MOD_ID_NONE;

	UPROPERTY()
	FColor SegmentColor = FColor::Black;

	UPROPERTY()
	float SegmentThickness = 3.0f;

	// Handle sprite widget
	UPROPERTY()
	class UAdjustmentHandleWidget* Widget;

	UPROPERTY()
	AAdjustmentHandleActor* HandleParent;

	UPROPERTY()
	TArray<AAdjustmentHandleActor*> HandleChildren;

	UPROPERTY()
	class AEditModelGameState *GameState;

	UPROPERTY()
	class AEditModelPlayerController *Controller;

	UPROPERTY()
	class AEditModelPlayerHUD *PlayerHUD;

	UPROPERTY()
	float HoveredScale;

	const static FName StateRequestTag;
};
