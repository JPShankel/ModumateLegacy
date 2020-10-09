// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateRoomStatics.h"


class AEditModelPlayerController_CPP;
class AEditModelGameMode_CPP;

class FModumateObjectInstance;

class MODUMATE_API FMOIRoomImpl : public FModumateObjectInstanceImplBase
{
public:
	FMOIRoomImpl(FModumateObjectInstance *moi);
	virtual ~FMOIRoomImpl();

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;

	virtual void OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered) override;
	virtual void OnSelected(bool bIsSelected) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;

	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;

protected:
	// Only used temporarily inside of GetStructuralPointsAndLines
	mutable TArray<FStructurePoint> TempPoints;
	mutable TArray<FStructureLine> TempLines;

	TWeakObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
	TWeakObjectPtr<UMaterialInstanceDynamic> VolumeMaterial;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
	FArchitecturalMaterial Material;
	bool bHovered = false;
	FRoomConfigurationBlueprint CachedRoomConfig;

	float GetAlpha();
	void UpdateMaterial();
};

