// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "DesignOption.generated.h"


USTRUCT()
struct MODUMATE_API FMOIDesignOptionData
{
	GENERATED_BODY()

	UPROPERTY(DisplayName = "Name")
	FString Name;

	UPROPERTY(DisplayName="Groups")
	TArray<int32> Groups;

	UPROPERTY(DisplayName="Child Options")
	TArray<int32> SubOptions;

	UPROPERTY(DisplayName="Hex Color")
	FString HexColor;
};

UCLASS()
class MODUMATE_API AMOIDesignOption : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIDesignOption();
	virtual ~AMOIDesignOption() {};

	UPROPERTY()
	FMOIDesignOptionData InstanceData;
};
