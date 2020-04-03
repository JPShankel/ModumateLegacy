// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ModumateCore/ModumateTypes.h"
#include "ModumateObjectComponent_CPP.generated.h"


// A minimal component that is used for attaching data to actors that can be used for looking up MOIs.
UCLASS()
class MODUMATE_API UModumateObjectComponent_CPP : public UActorComponent
{
	GENERATED_BODY()

public:
	UModumateObjectComponent_CPP() { }
	UModumateObjectComponent_CPP(int32 InObjectID);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 ObjectID = MOD_ID_NONE;
};
