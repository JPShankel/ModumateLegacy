// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "DocumentManagement/ModumateDynamicObjectBase.h"
#include "ModumateCore/ModumateObjectStatics.h"

class AEditModelPlayerController_CPP;
class ADynamicMeshActor;
class FModumateObjectInstance;

class MODUMATE_API FMOITrimImpl : public FModumateObjectInstanceImplBase
{
protected:
	TWeakObjectPtr<ADynamicMeshActor> DynamicMeshActor;
	TWeakObjectPtr<UWorld> World;

	// Cached values for the trim, interpreted from MOI's ControlPoints and ControlIndices
	float StartAlongEdge, EndAlongEdge;
	int32 EdgeStartIndex, EdgeEndIndex, EdgeMountIndex;
	bool bUseLengthAsPercent;
	ETrimMiterOptions MiterOptionStart, MiterOptionEnd;
	FVector TrimStartPos, TrimEndPos, TrimNormal, TrimUp, TrimDir;
	FVector2D UpperExtensions, OuterExtensions;

	void InternalUpdateGeometry(bool bRecreate, bool bCreateCollision);

public:
	FMOITrimImpl(FModumateObjectInstance *moi);
	virtual ~FMOITrimImpl();

	virtual void SetRotation(const FQuat &r) override;
	virtual FQuat GetRotation() const override;

	virtual void SetLocation(const FVector &p) override;
	virtual FVector GetLocation() const override;

	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;

	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

	virtual FModumateWallMount GetWallMountForSelf(int32 originIndex) const override;
	virtual void SetWallMountForSelf(const FModumateWallMount &wm) override;

	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;
};

