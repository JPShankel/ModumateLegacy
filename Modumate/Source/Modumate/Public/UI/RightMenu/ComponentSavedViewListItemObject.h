// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/ModumateCameraView.h"

#include "ComponentSavedViewListItemObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UComponentSavedViewListItemObject : public UObject
{
	GENERATED_BODY()

public:

	FModumateCameraView CameraView;
	int32 ID = -1;
};
