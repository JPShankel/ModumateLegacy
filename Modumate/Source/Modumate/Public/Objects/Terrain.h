// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "Terrain.generated.h"


USTRUCT()
struct MODUMATE_API FMOITerrainData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TMap<int32, double> Heights;
};


class ADynamicTerrainActor;

UCLASS()
class MODUMATE_API AMOITerrain : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOITerrain();

	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual FVector GetNormal() const override;
	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const override;
	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot) override;
	virtual void PostCreateObject(bool bNewObject) override;
	virtual void PreDestroy() override;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual bool OnSelected(bool bIsSelected) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines,
		bool bForSnapping = false, bool bForSelection = false) const override;

	virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;

	void SetIsTranslucent(bool NewIsTranslucent);
	bool GetIsTranslucent() const { return bIsTranslucent; };

	virtual bool FromWebMOI(const FString& InJson) override;
	virtual bool ToWebMOI(FWebMOI& OutMOI) const override;

	UPROPERTY()
	FMOITerrainData InstanceData;

protected:
	void UpdateTerrainActor();
	void UpdateSiteMaterials(bool bForceUpdate = false);
	void UpdateEditTerrainList();

	FVector GraphToWorldPosition(FVector2D GraphPos, double Height = 0.0, bool bRelative = false) const;
	TMap<int32, int32> PolyIDToMeshSection;

	bool bIsTranslucent = false;
	TMap<int32, FGuid> CachedMaterials;
};
