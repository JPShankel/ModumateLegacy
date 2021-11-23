// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "PointHosted.generated.h"

UCLASS()
class MODUMATE_API AMOIPointHosted : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIPointHosted();

	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;

protected:
	void InternalUpdateGeometry(bool bCreateCollision);
};
