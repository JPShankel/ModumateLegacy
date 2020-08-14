// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Database/ModumateObjectEnums.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"

#include "AdjustmentHandleActor.generated.h"

namespace Modumate
{
	class FModumateObjectInstance;
}

class FWidgetStyle;

UCLASS()
class MODUMATE_API AAdjustmentHandleActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAdjustmentHandleActor(const FObjectInitializer &ObjectInitializer);

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	//~ End AActor Interface

	bool bInitialized = false;

	virtual void Initialize();
	virtual void ApplyWidgetStyle();
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const;
	virtual void PostEndOrAbort();
	virtual void UpdateTargetGeometry();

public:
	virtual void SetEnabled(bool bNewEnabled);
	virtual bool BeginUse();
	virtual bool UpdateUse();
	virtual void Tick(float DeltaTime) override;
	virtual void EndUse();
	virtual void AbortUse();
	virtual bool IsInUse() const;
	virtual bool HandleInputNumber(float number);
	virtual bool HandleInvert();
	virtual FVector GetHandlePosition() const;
	virtual FVector GetHandleDirection() const;

	void SetTargetMOI(Modumate::FModumateObjectInstance *InTargetMOI);
	void SetTargetIndex(int32 InTargetIndex);
	void SetSign(float InSign);

	bool bEnabled = false;
	bool bIsInUse = false;

	// The MOI that created this handle
	Modumate::FModumateObjectInstance* SourceMOI = nullptr;
	// The MOI that this handle effects
	Modumate::FModumateObjectInstance* TargetMOI = nullptr;

	TArray<Modumate::FModumateObjectInstance*> TargetDescendents;

	int32 TargetIndex = INDEX_NONE;
	float Sign = 0.0f;

	EMouseMode OriginalMouseMode;
	bool AcceptRawInputNumber = false; // Raw user input is accepted if true, else it converts to imperial unit

	UPROPERTY()
	class UAdjustmentHandleWidget* Widget;

	UPROPERTY()
	AAdjustmentHandleActor* HandleParent;

	UPROPERTY()
	TArray<AAdjustmentHandleActor*> HandleChildren;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY()
	class AEditModelPlayerHUD *PlayerHUD;

	const static FName StateRequestTag;
};
