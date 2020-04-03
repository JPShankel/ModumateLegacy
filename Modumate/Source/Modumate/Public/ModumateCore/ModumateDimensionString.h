// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine.h"
#include "ModumateDimensionString.generated.h"

UENUM(BlueprintType)
enum class EDimensionUnits : uint8
{
	DU_Imperial,
	DU_Metric
};

UENUM(BlueprintType)
enum class EEnterableField : uint8
{
	None,
	NonEditableText, // Draw dim string line with text
	EditableText_ImperialUnits_UserInput, // Draw dim string line with editable text box, auto convert its length to imperial units, and auto replace string by user input
	EditableText_ImperialUnits_NoUserInput, // Same as EditableText_ImperialUnits_UserInput, but does not auto replace by user input
};

UENUM(BlueprintType)
enum class EAutoEditableBox : uint8
{
	Never,
	UponUserInput, // Auto turn functionality from nonEditable to Editable if controller has user input
	UponUserInput_SameGroupIndex, // Same as UponUserInput but also the playerState must have the same dimension group index
};

UENUM(BlueprintType)
enum class EDimStringStyle : uint8
{
	Fixed,
	HCamera,
	RotateToolLine,
	DegreeString
};

USTRUCT(BlueprintType)
struct FModelDimensionString
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector Point1 = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector Point2 = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float AngleDegrees = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	EEnterableField Functionality = EEnterableField::NonEditableText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float Offset = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FName UniqueID = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	AActor* Owner = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	EDimStringStyle Style = EDimStringStyle::Fixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector OffsetDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FName GroupID = "Default";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	int32 GroupIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool bAlwaysVisible = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector DegreeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector DegreeDirectionStart = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector DegreeDirectionEnd = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool bDegreeClockwise = false;
};
