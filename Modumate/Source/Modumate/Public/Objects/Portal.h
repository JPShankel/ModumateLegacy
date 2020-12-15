// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "Portal.generated.h"

UENUM()
enum class EPortalOrientation : uint8 { Up, Left, Down, Right };

inline constexpr EPortalOrientation operator+(EPortalOrientation a, int b)
	{ return (EPortalOrientation)( ((int)a + b + 4) % 4 ); }

USTRUCT()
struct MODUMATE_API FMOIPortalData
{
	GENERATED_BODY()

	UPROPERTY()
	bool bNormalInverted = false;

	UPROPERTY()
	bool bLateralInverted = false;

	UPROPERTY()
	float Justification = 0.0f;

	UPROPERTY()
	EPortalOrientation Orientation = EPortalOrientation::Up;
};

class AAdjustmentHandleActor;
class AModumateObjectInstance;

UCLASS()
class MODUMATE_API AMOIPortal : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIPortal();

	virtual AActor *CreateActor(const FVector &loc, const FQuat &rot) override;
	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual FTransform GetWorldTransform() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;

	virtual FVector GetNormal() const;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual bool GetInvertedState(FMOIStateData& OutState) const override;

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override;

	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;

	UPROPERTY()
	FMOIPortalData InstanceData;

protected:
	TWeakObjectPtr<AEditModelPlayerController_CPP> Controller;

	bool SetupCompoundActorGeometry();
	bool SetRelativeTransform(const FVector2D& InRelativePos, const FQuat& InRelativeRot);

	EDoorOperationType GetDoorType() const;

	FVector2D CachedRelativePos;
	FVector CachedWorldPos;
	FQuat CachedRelativeRot, CachedWorldRot;
	bool bHaveValidTransform;
};
