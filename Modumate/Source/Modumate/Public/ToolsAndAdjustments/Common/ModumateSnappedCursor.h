// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateSnappedCursor.generated.h"

UENUM(BlueprintType)
enum class ESnapType : uint8
{
	CT_NOSNAP = 0,
	CT_CORNERSNAP,
	CT_MIDSNAP,
	CT_EDGESNAP,
	CT_ALIGNMENTSNAP,
	CT_FACESELECT,
	CT_WORLDSNAPX,
	CT_WORLDSNAPY,
	CT_WORLDSNAPXY,
	CT_WORLDSNAPZ,
	CT_USERPOINTSNAP,
	CT_CUSTOMSNAPX,
	CT_CUSTOMSNAPY,
	CT_CUSTOMSNAPZ
};

UENUM()
enum class EMouseMode : uint8
{
	Object,
	Location
};

struct MODUMATE_API FMouseWorldHitType
{
	bool Valid = false;
	ESnapType SnapType = ESnapType::CT_NOSNAP;
	FVector Location = FVector::ZeroVector;
	FVector Origin = FVector::ZeroVector;
	FVector EdgeDir = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;
	int32 CP1 = -1;
	int32 CP2 = -1;
	TWeakObjectPtr<AActor> Actor;
};

// Affordance frames are set by tools and handles to define the sketch plane and the custom affordance direction
// The sketch plane is defined by the origin and the normal and the preferred direction for affordance is the tangent
// Tangent may be the zero vector if there is no custom affordance on the plane
struct MODUMATE_API FAffordanceFrame
{
	FVector Origin, Normal, Tangent;
	bool bHasValue = false;
};

struct FAffordanceLine;

USTRUCT(BlueprintType)
struct MODUMATE_API FSnappedCursor
{
	GENERATED_BODY()

	// These values are set in EditModelPlayerController::UpdateMouseHits

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector2D ScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector OriginPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector OriginDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector2D AxisOrigin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	ESnapType SnapType = ESnapType::CT_NOSNAP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool Visible = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector WorldPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool HasProjectedPosition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector ProjectedPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	AActor *Actor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	int32 CP1 = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	int32 CP2 = INDEX_NONE;

	FVector HitNormal = FVector::ZeroVector;
	FVector HitTangent = FVector::ZeroVector;

	// Affordance values are NOT updated every frame but are established on an initiating event (ie BeginUse in a tool)
	// These values are then used on subsequent frames to reconcile the mouse's world position against an anchored reference frame
	// "Custom" affordances are established when we click on a directed element, like the edge of a wall or floor, and provide
	// a reference frame aligned along that element. There are also world aligned affordances along global X, Y and Z at the
	// same origin as the custom affordance
	bool WantsVerticalAffordanceSnap = false;
	FAffordanceFrame AffordanceFrame;
	bool HasAffordanceSet() const;
	void SetAffordanceFrame(const FVector &origin, const FVector &normal, const FVector &tangent = FVector::ZeroVector);
	void ClearAffordanceFrame();
	FVector SketchPlaneProject(const FVector &p) const;

	// Set in the controller on a per-frame basis
	bool ShiftLocked = false;

	// defines the line for a shift-constrained projection
	FVector ShiftLockOrigin = FVector::ZeroVector;
	FVector ShiftLockDirection = FVector::ZeroVector;

	// Almost DEPRECATED: most tools use Location mouse mode, but the select tool still uses Object for single object selection
	EMouseMode MouseMode = EMouseMode::Location;	
	
	// TODO: Tools currently decide for themselves whether to create an affordance line to the sketch plane
	// make this a simple switch that can be turned on and off rather than having each tool build the affordance line
	bool TryMakeAffordanceLineFromCursorToSketchPlane(FAffordanceLine &outAffordance, FVector &outHitPoint) const;

	// Used by controller to create sketch plane under cursor hit, returns false on direction parallel to sketch plane
	bool TryGetRaySketchPlaneIntersection(const FVector &origin, const FVector &direction, FVector &outputPosition) const;

	bool operator ==(const FSnappedCursor &other) const
	{
		return (bValid == other.bValid) &&
			(ScreenPosition == other.ScreenPosition) &&
			(AxisOrigin == other.AxisOrigin) &&
			(SnapType == other.SnapType) &&
			(Visible == other.Visible) &&
			(WorldPosition == other.WorldPosition) &&
			(HasProjectedPosition == other.HasProjectedPosition) &&
			(ProjectedPosition == other.ProjectedPosition) &&
			(Actor == other.Actor) &&
			(HitNormal == other.HitNormal) &&
			(CP1 == other.CP1) &&
			(CP2 == other.CP2);
	}
};
