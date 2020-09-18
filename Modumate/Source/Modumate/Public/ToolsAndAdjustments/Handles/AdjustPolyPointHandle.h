// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustPolyPointHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustPolyPointHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	AAdjustPolyPointHandle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual void PostEndOrAbort() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;
	virtual bool HandleInputNumber(float number) override;

	UFUNCTION()
	void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void SetAdjustPolyEdge(bool bInAdjustPolyEdge);

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	bool GetTransforms(const FVector Offset, TMap<int32, FTransform>& OutTransforms);

	bool bAdjustPolyEdge;
	FPlane PolyPlane;
	FVector OriginalDirection;
	TArray<FVector> OriginalPolyPoints;
	TArray<FVector> LastValidPolyPoints;
	FVector AnchorLoc;

	// Dimension Widget
	UPROPERTY()
	class UModumateGameInstance *GameInstance;
	int32 PendingSegmentID = MOD_ID_NONE;
};

