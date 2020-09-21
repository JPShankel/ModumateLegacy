// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"

class AEditModelPlayerController_CPP;

class MODUMATE_API FMOITrimImpl : public FModumateObjectInstanceImplBase
{
public:
	FMOITrimImpl(FModumateObjectInstance *moi);
	virtual ~FMOITrimImpl();

	virtual void SetRotation(const FQuat &r) override;
	virtual FQuat GetRotation() const override;

	virtual void SetLocation(const FVector &p) override;
	virtual FVector GetLocation() const override;

	virtual FVector GetNormal() const override;

	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP* Controller) override;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP* Controller, bool bShow) override;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;

protected:
	// Cached values for the trim, derived from instance properties and the parent SurfaceEdge
	FVector TrimStartPos, TrimEndPos, TrimNormal, TrimUp, TrimDir, TrimScale;
	FVector2D UpperExtensions, OuterExtensions;

	bool UpdateCachedStructure();
	bool UpdateMitering();
	bool InternalUpdateGeometry(bool bRecreate, bool bCreateCollision);
};

