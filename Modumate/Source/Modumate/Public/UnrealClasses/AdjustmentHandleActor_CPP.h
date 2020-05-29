// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <functional>

#include "Runtime/Engine/Classes/Engine/StaticMeshActor.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Database/ModumateObjectEnums.h"

#include "AdjustmentHandleActor_CPP.generated.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{
	class FModumateObjectInstance;
	class IAdjustmentHandleImpl;
}
UCLASS()
class MODUMATE_API AAdjustmentHandleActor_CPP : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAdjustmentHandleActor_CPP();

	void SetHandleScale(const FVector &s);
	void SetHandleScaleScreenSize(const FVector &s);
	void SetOverrideHandleDirection(const FVector& overrideHandleDirection, Modumate::FModumateObjectInstance* MOI, float offsetDist);
	void SetWallHandleSide(const int32& side, Modumate::FModumateObjectInstance* MOI, float offsetDist);
	void SetPolyHandleSide(const TArray<int32>& CP, Modumate::FModumateObjectInstance* MOI, float offsetDist);
	void SetHandleRotation(const FQuat &r);
	void SetActorMesh(UStaticMesh *mesh);

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	//~ End AActor Interface

	UPROPERTY()
	UMaterialInterface *OriginalMaterial;

	UPROPERTY()
	UStaticMeshComponent* HandleMesh;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial;

	UFUNCTION()
	void OnHover(bool bNewHovered);

	bool bEnabled = false;
	Modumate::IAdjustmentHandleImpl * Implementation = nullptr;
	FVector HandleScreenSize = FVector(0.005, 0.005, 0.005);
	float AngleScreenOffset = 0.0f;
	int32 Side = 0;
	Modumate::FModumateObjectInstance* ParentMOI = nullptr;
	TArray<int32> SideCP;
	FPlane polyPlane = FPlane(ForceInitToZero);
	FVector OverrideHandleDirection = FVector::ZeroVector;
	FVector HandleDirection = FVector::ZeroVector;
	float OffSetScreenDistance = 0.0;
	bool bIsHovered = false;
	FVector ScreenOffset;
	bool bIsPointAdjuster = false;
	bool AsUpwardBillboard = false; // The handle acts as a UE4 billboard but always face upward
	bool AcceptRawInputNumber = false; // Raw user input is accepted if true, else it converts to imperial unit
	EHandleType HandleType = EHandleType::Default;
	bool bShowHandleChildren = false;

	UPROPERTY()
	AAdjustmentHandleActor_CPP* HandleParent;

	UPROPERTY()
	TArray<AAdjustmentHandleActor_CPP*> HandleChildren;

	// The distance between adjustment handle and player's camera.
	// At this distance, the handle scale will be multiplied by HandleNearScaleMultiplier
	float HandleNearDistance = 1000.f;

	// The distance between adjustment handle and player's camera.
	// At this distance, the handle scale will be multiplied by HandleFarScaleMultiplier
	float HandleFarDistance = 10000.f; // 6000

	// Multiply scale of the handle when player's camera is near, relative to HandleNearDistance
	float HandleNearScaleMultiplier = 1.f;

	// Multiply scale of the handle when player's camera is far, relative to HandleFarDistance
	float HandleFarScaleMultiplier = .25f; // .5

	UPROPERTY()
	AEditModelPlayerController_CPP *Controller;

	bool BeginUse();
	bool UpdateUse();
	bool EndUse();
	bool AbortUse();
	bool IsInUse() const;
	bool HandleInputNumber(float number);
	bool HandleInvert();
	void SetEnabled(bool bNewEnabled);

	void ShowHoverWallDimensionString();
	void ShowHoverFloorDimensionString(bool ShowCurrentSide);
	void ShowHoverMetaPlaneDimensionString();
	void ShowHoverCabinetDimensionString();

	// Show dim string drawn from one of the two control points affected by this handle. 
	void ShowSideCPDimensionString(const int32 sideCPNum, const bool editable, const int32 groupIndex, const float offset = 40.f);

	// Draw a dim string from both CPs affected by this handle if only the affected edges are parallel. Return true if edges are parallel
	bool ShowSideCPParallelDimensionString(const bool alwaysditable, const int32 groupIndex, const float offset = 40.f, const float parallelThreshold = 0.01f);
};
