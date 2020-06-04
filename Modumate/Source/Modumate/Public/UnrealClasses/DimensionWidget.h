// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DimensionWidget.generated.h"

UCLASS()
class MODUMATE_API UDimensionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDimensionWidget(const FObjectInitializer& ObjectInitializer);

	void SetTarget(int32 InTargetEdgeID, int32 InTargetObjID);
	void UpdateTransform();

protected:
	virtual void InitializeNativeClassData() override;

	// TODO: potentially have this used by more things depending on how 
	// unique this format is
	void SanitizeInput(float InLength, FText &OutText);

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTextBlock *Measurement;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder *RootBorder;

	int32 TargetEdgeID;
	int32 TargetObjID;

	int32 OffsetFromEdge;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;
};
