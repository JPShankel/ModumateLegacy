// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "VertexBase.generated.h"

class AVertexActor;

UCLASS()
class MODUMATE_API AMOIVertexBase : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIVertexBase();

	UPROPERTY(EditAnywhere)
	class UStaticMeshComponent* VertexMeshComp;

	virtual FQuat GetRotation() const override { return FQuat::Identity; }
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual void GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual AActor *CreateActor(const FVector &loc, const FQuat &rot) override;
	virtual bool OnHovered(AEditModelPlayerController* controller, bool bNewHovered) override;
	virtual bool OnSelected(bool bIsSelected) override;
	virtual bool ShowStructureOnSelection() const override { return false; }
	virtual bool UseStructureDataForCollision() const override { return true; }
	virtual void GetTangents(TArray<FVector>& OutTangents) const;

protected:
	virtual void BeginPlay() override;
	void UpdateVertexMesh();

	UPROPERTY()
	TArray<AModumateObjectInstance*> CachedConnectedMOIs;

	FColor SelectedColor, BaseColor, CurColor;
	float DefaultScreenSize, HoveredScreenSize, CurScreenSize;
};
