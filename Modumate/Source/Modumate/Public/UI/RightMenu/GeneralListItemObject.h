// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RightMenu/GeneralListItem.h"

#include "GeneralListItemObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UGeneralListItemObject : public UObject
{
	GENERATED_BODY()

public:

	EGeneralListType CutPlaneType = EGeneralListType::None;
	FString DisplayName = FString(TEXT("New Cut Plane"));
	int32 ObjId = -1;
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	bool Visibility = true;
	bool bIsCulling = false;
};
