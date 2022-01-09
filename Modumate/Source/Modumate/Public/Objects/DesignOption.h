// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "DesignOption.generated.h"


USTRUCT()
struct MODUMATE_API FMOIDesignOptionData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Groups;

	UPROPERTY()
	TArray<int32> SubOptions;

	UPROPERTY()
	FString Name;
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
