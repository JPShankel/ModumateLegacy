// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateDynamicObjectBase.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "DocumentManagement/ModumateLayeredObjectInterface.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{
	class FModumateObjectInstance;

	class MODUMATE_API FMOIPlaneHostedObjImpl : public FDynamicModumateObjectInstanceImpl, ILayeredObject
	{
	public:
		FMOIPlaneHostedObjImpl(FModumateObjectInstance *InMOI);
		virtual ~FMOIPlaneHostedObjImpl();
		virtual FQuat GetRotation() const override;
		virtual FVector GetLocation() const override;
		virtual FVector GetCorner(int32 index) const override;
		virtual FVector GetNormal() const override;
		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover) override {};
		virtual void OnSelected(bool bNewSelected) override;

		virtual const ILayeredObject* GetLayeredInterface() const override { return this; }
		virtual const FCachedLayerDimsByType &GetCachedLayerDims() const override { return CachedLayerDims; }
		virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override;
		// TODO: may want to integrate this back into GetDrafting lines to help cache some more variables
		virtual bool GetRangesForHolesOnPlane(TArray<TPair<float, float>> &OutRanges, TPair<FVector, FVector> &Intersection, const FLayerGeomDef &Layer, const float CurrentThickness, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin) const;

		virtual void SetFromDataRecordAndRotation(const FMOIDataRecord &dataRec, const FVector &origin, const FQuat &rotation) override;
		virtual void SetFromDataRecordAndDisplacement(const FMOIDataRecord &dataRec, const FVector &displacement) override;

	protected:
		void InternalUpdateGeometry();
		void UpdateMeshWithLayers(bool bRecreateMesh, bool bRecalculateEdgeExtensions);
		// TODO: this should be replaced by FGraph3DFace::IntersectsPlane, and that code needs to be generalized first
		void GetPlaneIntersections(TArray<FVector> &OutIntersections, const TArray<FVector> &InPoints, const FPlane &InPlane) const;
		void UpdateConnectedEdges();

		bool bGeometryDirty = false;
		FCachedLayerDimsByType CachedLayerDims;
		TArray<FLayerGeomDef> LayerGeometries;
		TArray<FPolyHole3D> CachedHoles;
		mutable TArray<FVector> TempHoleRelativePoints;

		TArray<FVector2D> CachedLayerEdgeExtensions;
		TArray<FModumateObjectInstance *> CachedParentConnectedMOIs;
		TArray<FModumateObjectInstance *> CachedConnectedEdges;
	};
}
