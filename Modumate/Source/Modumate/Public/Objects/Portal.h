// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/DimensionOffset.h"
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

	FMOIPortalData();
	FMOIPortalData(int32 InVersion);

	UPROPERTY()
	int32 Version = 0;

	UPROPERTY()
	bool bNormalInverted = false;

	UPROPERTY()
	bool bLateralInverted = false;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The data formerly stored in Justification is now in Offset."))
	float Justification_DEPRECATED = 0.0f;

	UPROPERTY()
	FDimensionOffset Offset;

	UPROPERTY()
	EPortalOrientation Orientation = EPortalOrientation::Up;

	static constexpr int32 CurrentVersion = 1;
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
	virtual void SetupAdjustmentHandles(AEditModelPlayerController *controller) override;

	virtual FVector GetNormal() const;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

	virtual bool GetInvertedState(FMOIStateData& OutState) const override;
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;
	virtual bool GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override;

	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;

	virtual void PostLoadInstanceData() override;

	virtual bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const override;

	UPROPERTY()
	FMOIPortalData InstanceData;

protected:
	TWeakObjectPtr<AEditModelPlayerController> Controller;

	bool SetupCompoundActorGeometry();
	bool SetRelativeTransform(const FVector2D& InRelativePos, const FQuat& InRelativeRot);

	EDoorOperationType GetDoorType() const;

	UFUNCTION()
	void OnInstPropUIChangedFlip(int32 FlippedAxisInt);

	UFUNCTION()
	void OnInstPropUIChangedOffset(const FDimensionOffset& NewValue);

	virtual void UpdateQuantities() override;

	FVector2D CachedRelativePos;
	FVector CachedWorldPos;
	FQuat CachedRelativeRot, CachedWorldRot;
	bool bHaveValidTransform;
};
