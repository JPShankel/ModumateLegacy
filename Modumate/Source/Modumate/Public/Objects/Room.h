// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ModumateCore/ModumateRoomStatics.h"
#include "Objects/ModumateObjectInstance.h"

#include "Room.generated.h"

class AEditModelPlayerController;
class AEditModelGameMode;

class AModumateObjectInstance;

UCLASS()
class MODUMATE_API AMOIRoom : public AModumateObjectInstance
{
	GENERATED_BODY()
public:
	AMOIRoom();

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;

	virtual bool OnHovered(AEditModelPlayerController *controller, bool bIsHovered) override;
	virtual bool OnSelected(bool bIsSelected) override;
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
	TWeakObjectPtr<AEditModelGameMode> GameMode;
	FArchitecturalMaterial Material;
	bool bHovered = false;
	FRoomConfigurationBlueprint CachedRoomConfig;

	float GetAlpha();
	void UpdateMaterial();
};

