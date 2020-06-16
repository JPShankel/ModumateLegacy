// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"

#include "DimensionWidget.generated.h"

UCLASS()
class MODUMATE_API UDimensionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDimensionWidget(const FObjectInitializer& ObjectInitializer);

	void SetIsEditable(bool bIsEditable);

	void UpdateLengthTransform(const FVector2D position, const FVector2D edgeDirection, const FVector2D offsetDirection, float length);
	void UpdateDegreeTransform(const FVector2D position, const FVector2D offsetDirection, float angle);
	void UpdateText(float length);
	void UpdateDegreeText(float angle);
	void ResetText();

protected:
	// TODO: potentially have this used by more things depending on how 
	// unique this format is
	void SanitizeInput(float InLength, FText &OutText);

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UEditableTextBox *Measurement;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	FSlateFontInfo ReadOnlyFont;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	FSlateFontInfo EditableFont;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	int32 PixelOffset;

private:
	FText LastCommittedText;
	float LastMeasurement;
};
