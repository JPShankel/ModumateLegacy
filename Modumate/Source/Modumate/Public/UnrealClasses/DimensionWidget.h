// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Math/UnitConversion.h"
#include "ModumateCore/ModumateDimensionString.h"

#include "DimensionWidget.generated.h"

UENUM()
enum class EDimensionDisplayType : uint8
{
	None,
	Linear,
	Angular
};

UCLASS()
class MODUMATE_API UDimensionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDimensionWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	void SetIsEditable(bool bIsEditable);

	void UpdateLengthTransform(const FVector2D& Position, const FVector2D& EdgeDirection, const FVector2D& OffsetDirection, double Length);
	void UpdateDegreeTransform(const FVector2D& Position, const FVector2D& OffsetDirection, double Angle);
	void UpdateText(double Length, bool bForce = false);
	void UpdateDegreeText(double Angle, bool bForce = false);
	void UpdateUnits();
	void ResetText();
	void ResetWidget();

protected:
	virtual void OnWidgetRebuilt() override;

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
	EDimensionUnits DisplayUnitType = EDimensionUnits::DU_Imperial;
	EUnit DisplayOverrideUnit = EUnit::Unspecified;

	FText LastCommittedText;
	double LastMeasurement = 0.0;
	EDimensionDisplayType LastDisplayType = EDimensionDisplayType::None;
};
