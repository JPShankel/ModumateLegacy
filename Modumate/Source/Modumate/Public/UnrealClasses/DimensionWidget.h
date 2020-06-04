// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"

#include "DimensionWidget.generated.h"

namespace Modumate 
{
	class FGraph3D;
}

UCLASS()
class MODUMATE_API UDimensionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDimensionWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	void SetTarget(int32 InTargetEdgeID, int32 InTargetObjID, bool bIsEditable);

	void SetTarget(int32 InTargetEdgeID, int32 InTargetObjID);
	void UpdateTransform();

protected:
	virtual void InitializeNativeClassData() override;

	// TODO: potentially have this used by more things depending on how 
	// unique this format is
	void SanitizeInput(float InLength, FText &OutText);

	UFUNCTION()
	void OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void UpdateText();

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UEditableTextBox *Measurement;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	FSlateFontInfo ReadOnlyFont;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	FSlateFontInfo EditableFont;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	int32 PixelOffset;

	int32 TargetEdgeID;
	int32 TargetObjID;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

private:
	FText LastCommittedText;
	float LastLength;

	const Modumate::FGraph3D *Graph;
};
