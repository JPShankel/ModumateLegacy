// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateCore/EdgeDetailData.h"
#include "Objects/ModumateObjectInstance.h"

#include "EdgeDetailObj.generated.h"

UCLASS()
class MODUMATE_API AMOIEdgeDetail : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	// TODO: replace this from being InstanceData to being stored in its FBIMAssemblySpec, from a shareable preset
	UPROPERTY()
	FEdgeDetailData InstanceData;

protected:
	virtual void PostLoadInstanceData() override;
};
