// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/CameraView.h"

#include "ComponentSavedViewListItemObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UComponentSavedViewListItemObject : public UObject
{
	GENERATED_BODY()

public:

	FMOICameraViewData CameraView;
};
