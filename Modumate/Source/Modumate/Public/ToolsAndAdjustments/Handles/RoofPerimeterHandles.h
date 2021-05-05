// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Graph/Graph3DTypes.h"
#include "ModumateCore/ModumateRoofStatics.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "RoofPerimeterHandles.generated.h"


UCLASS()
class MODUMATE_API ACreateRoofFacesHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	TArray<FVector> EdgePoints;
	TArray<int32> EdgeIDs;
	FRoofEdgeProperties DefaultEdgeProperties;
	TArray<FRoofEdgeProperties> EdgeProperties;

	TArray<FVector> CombinedPolyVerts;
	TArray<int32> PolyVertIndices;
};

UCLASS()
class MODUMATE_API ARetractRoofFacesHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	TSet<int32> TempGroupMembers;
	TArray<int32> TempFaceIDs;
};

UCLASS()
class MODUMATE_API AEditRoofEdgeHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	AEditRoofEdgeHandle();

	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndUse() override;
	virtual void AbortUse() override;
	virtual FVector GetHandlePosition() const override;

	void SetTargetEdge(FGraphSignedID InTargetEdgeID);
	void UpdateWidgetData();

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	FGraphSignedID TargetEdgeID;

	UPROPERTY()
	class URoofPerimeterPropertiesWidget* PropertiesWidget;
};
