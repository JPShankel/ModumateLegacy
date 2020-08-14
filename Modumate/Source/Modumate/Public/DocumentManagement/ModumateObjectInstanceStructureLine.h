// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "DocumentManagement/ModumateDynamicObjectBase.h"
#include "ModumateCore/ModumateObjectStatics.h"

class AEditModelPlayerController_CPP;
class ADynamicMeshActor;

namespace Modumate
{
	class FModumateObjectInstance;

	class MODUMATE_API FMOIStructureLine : public FModumateObjectInstanceImplBase
	{
	protected:
		TWeakObjectPtr<ADynamicMeshActor> DynamicMeshActor;
		TWeakObjectPtr<UWorld> World;

		FVector LineStartPos, LineEndPos, LineDir, LineNormal, LineUp;
		FVector2D UpperExtensions, OuterExtensions;

		void InternalUpdateGeometry(bool bRecreate, bool bCreateCollision);

	private:
		bool GetPerimeterPoints(TArray<FVector>& outPerimeterPoints) const;
		TArray<FEdge> GetBeyondLinesFromMesh(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
			const FVector& AxisX, const FVector& AxisY, const FVector& Origin) const;

	public:
		FMOIStructureLine(FModumateObjectInstance *moi);
		virtual ~FMOIStructureLine();

		virtual FQuat GetRotation() const override;
		virtual FVector GetLocation() const override;

		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;

		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;

		virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane,
			const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
			TArray<TArray<FVector>> &OutPerimeters) const override;

		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	};
}
