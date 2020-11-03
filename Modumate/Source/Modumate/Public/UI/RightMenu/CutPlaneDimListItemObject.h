// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RightMenu/CutPlaneDimListItem.h"

#include "CutPlaneDimListItemObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UCutPlaneDimListItemObject : public UObject
{
	GENERATED_BODY()

public:

	ECutPlaneType CutPlaneType = ECutPlaneType::None;
	FString DisplayName = FString(TEXT("New Cut Plane"));
	int32 ObjId = -1;
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	bool Visibility = true;
	bool CanExport = false;
};
