// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstancePortal.h"

#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Drafting/ModumateDraftingElements.h"
#include "UnrealClasses/ModumateObjectInstanceParts_CPP.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Algo/ForEach.h"
#include "Algo/Unique.h"


class AEditModelPlayerController_CPP;

namespace Modumate
{

	FMOIPortalImpl::FMOIPortalImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, Controller(nullptr)
		, CachedRelativePos(ForceInitToZero)
		, CachedWorldPos(ForceInitToZero)
		, CachedRelativeRot(ForceInit)
		, CachedWorldRot(ForceInit)
		, bHaveValidTransform(false)
	{
		SetControlPointsFromAssembly();
	}

	void FMOIPortalImpl::GetControlPointsFromAssembly(const FBIMAssemblySpec &ObjectAssembly, TArray<FVector> &ControlPoints)
	{
	}

	void FMOIPortalImpl::SetControlPointsFromAssembly()
	{
		TArray<FVector> controlPoints;
		GetControlPointsFromAssembly(MOI->GetAssembly(), controlPoints);
		MOI->SetControlPoints(controlPoints);
	}

	void FMOIPortalImpl::UpdateAssemblyFromControlPoints()
	{
		if (MOI->GetControlPoints().Num() < 4)
		{
			return;
		}
	}

	FMOIPortalImpl::~FMOIPortalImpl()
	{}

	AActor *FMOIPortalImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		return world->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
	}

	void FMOIPortalImpl::OnAssemblyChanged()
	{
		SetControlPointsFromAssembly();
		SetupCompoundActor();
		CacheCorners();
	}

	FVector FMOIPortalImpl::GetNormal() const
	{
		AActor *portalActor = MOI ? MOI->GetActor() : nullptr;
		if (!ensure(portalActor != nullptr))
		{
			return FVector::ZeroVector;
		}

		FTransform portalTransform = portalActor->GetActorTransform();
		return portalTransform.GetUnitAxis(EAxis::Y);
	}

	bool FMOIPortalImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
	{
		if (!FModumateObjectInstanceImplBase::CleanObject(DirtyFlag, OutSideEffectDeltas))
		{
			return false;
		}

		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
		{
			auto *parentObj = MOI ? MOI->GetParentObject() : nullptr;
			if (parentObj)
			{
				parentObj->MarkDirty(EObjectDirtyFlags::Visuals);
			}
			else
			{
				return false;
			}
		}
		break;
		}

		return true;
	}

	void FMOIPortalImpl::SetupDynamicGeometry()
	{
		// If the MOI's control points got reset, then we need to recreate them here before they're used.
		if (MOI->GetControlPoints().Num() == 0)
		{
			SetControlPointsFromAssembly();
		}

		UpdateAssemblyFromControlPoints();
		SetupCompoundActor();
		SetRelativeTransform(CachedRelativePos, CachedRelativeRot);
	}

	void FMOIPortalImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOIPortalImpl::SetupCompoundActor()
	{
		if (ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(MOI->GetActor()))
		{
			cma->MakeFromAssembly(MOI->GetAssembly(), FVector::OneVector, MOI->GetObjectInverted(), true);
		}
	}

	bool FMOIPortalImpl::SetRelativeTransform(const FVector2D &InRelativePos, const FQuat &InRelativeRot)
	{
		CachedRelativePos = InRelativePos;
		CachedRelativeRot = InRelativeRot;

		auto *parentObj = MOI ? MOI->GetParentObject() : nullptr;
		AActor *portalActor = MOI ? MOI->GetActor() : nullptr;
		if ((parentObj == nullptr) || (portalActor == nullptr))
		{
			return false;
		}

		if (!UModumateObjectStatics::GetWorldTransformOnPlaneHostedObj(parentObj,
			CachedRelativePos, CachedRelativeRot, CachedWorldPos, CachedWorldRot))
		{
			return false;
		}

		portalActor->SetActorLocationAndRotation(CachedWorldPos, CachedWorldRot);
		CacheCorners();
		bHaveValidTransform = true;

		return true;
	}

	bool FMOIPortalImpl::CacheCorners()
	{
		CachedCorners.Reset(CachedCorners.Num());

		FModumateObjectInstance *parentMOI = MOI ? MOI->GetParentObject() : nullptr;
		if (parentMOI == nullptr)
		{
			return false;
		}

		FTransform portalTransform = MOI->GetActor()->GetActorTransform();
		float parentThickness = MOI->GetParentObject()->CalculateThickness();
		FVector portalNormal = portalTransform.GetUnitAxis(EAxis::Y);
		FVector oppositeSideDelta = -parentThickness * portalNormal;
		int32 numCP = MOI->GetControlPoints().Num();

		for (int32 i = 0; i < 2 * numCP; ++i)
		{
			const FVector &cp = MOI->GetControlPoint(i % numCP);
			FVector corner = portalTransform.TransformPosition(cp);
			if (i >= numCP)
			{
				corner += oppositeSideDelta;
			}

			CachedCorners.Add(MoveTemp(corner));
		}

		return true;
	}

	void FMOIPortalImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		int32 numCP = MOI->GetControlPoints().Num();
		if (!ensure(CachedCorners.Num() == (2 * numCP)))
		{
			return;
		}

		FTransform portalTransform = MOI->GetActor()->GetActorTransform();
		FVector portalNormal = portalTransform.GetUnitAxis(EAxis::Y);

		for (int32 i = 0; i < 2 * numCP; ++i)
		{
			int32 cpIdx = i % numCP;
			int32 nextCPIdx = (cpIdx + 1) % numCP;
			int32 cpOffset = i - cpIdx;
			int32 nextI = nextCPIdx + cpOffset;
			float normalSign = (i < numCP) ? 1.0f : -1.0f;

			outPoints.Add(FStructurePoint(CachedCorners[i], normalSign * portalNormal, i));
			outLines.Add(FStructureLine(CachedCorners[i], CachedCorners[nextI], i, nextI));
		}
	}

	void FMOIPortalImpl::SetRotation(const FQuat &r)
	{
		SetRelativeTransform(CachedRelativePos, r);
	}

	FQuat FMOIPortalImpl::GetRotation() const
	{
		return CachedRelativeRot;
	}

	void FMOIPortalImpl::SetLocation(const FVector &p)
	{
		SetRelativeTransform(FVector2D(p), CachedRelativeRot);
	}

	FVector FMOIPortalImpl::GetCorner(int32 index) const
	{
		int32 numCP = MOI->GetControlPoints().Num();
		if (!ensureAlways(CachedCorners.Num() == (2 * numCP)))
		{
			return CachedWorldPos;
		}

		return CachedCorners[index];
	}

	FVector FMOIPortalImpl::GetLocation() const
	{
		return FVector(CachedRelativePos, 0.0f);
	}

	void FMOIPortalImpl::SetWorldTransform(const FTransform &NewTransform)
	{
		FModumateObjectInstance *parentObject = MOI ? MOI->GetParentObject() : nullptr;
		if (parentObject)
		{
			FVector2D newRelativePos(ForceInitToZero);
			FQuat newRelativeRot(ForceInit);
			if (UModumateObjectStatics::GetRelativeTransformOnPlaneHostedObj(parentObject,
				NewTransform.GetLocation(), NewTransform.GetRotation().GetAxisY(),
				0.0f, false, newRelativePos, newRelativeRot))
			{
				SetRelativeTransform(newRelativePos, newRelativeRot);
			}
		}
	}

	FTransform FMOIPortalImpl::GetWorldTransform() const
	{
		return FTransform(CachedWorldRot, CachedWorldPos);
	}

	void FMOIPortalImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
	}

	TArray<FModelDimensionString> FMOIPortalImpl::GetDimensionStrings() const
	{
		TArray<FModelDimensionString> ret;

		if (MOI->GetControlPoints().Num() > 3)
		{
			FModelDimensionString ds;
			ds.AngleDegrees = 0;
			ds.Point1 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->GetControlPoint(2));
			ds.Point2 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->GetControlPoint(1));
			ds.Functionality = EEnterableField::EditableText_ImperialUnits_UserInput;
			ds.Offset = 50;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ds.Owner = MOI->GetActor();
			ret.Add(ds);

			ds.Point1 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->GetControlPoint(3));
			ds.Point2 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->GetControlPoint(0));
			ds.Functionality = EEnterableField::EditableText_ImperialUnits_UserInput;
			ds.Offset = 100;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ret.Add(ds);

		}

		return ret;
	}

	void FMOIPortalImpl::SetFromDataRecordAndRotation(const FMOIDataRecord &dataRec, const FVector &origin, const FQuat &rotation)
	{
		// Don't apply any rotation, we can't meaningfully rotate portals
		SetFromDataRecordAndDisplacement(dataRec, FVector::ZeroVector);
	}

	void FMOIPortalImpl::SetFromDataRecordAndDisplacement(const FMOIDataRecord &dataRec, const FVector &displacement)
	{
		FVector2D recordRelativePos(dataRec.Location);
		FQuat recordRelativeRot(dataRec.Rotation);
		FModumateObjectInstance *parentObj = MOI->GetParentObject();

		FVector displacedWorldPos = CachedWorldPos;
		FQuat displacedWorldRot = CachedWorldRot;
		if (UModumateObjectStatics::GetWorldTransformOnPlaneHostedObj(parentObj,
			recordRelativePos, recordRelativeRot, displacedWorldPos, displacedWorldRot))
		{
			displacedWorldPos += displacement;

			FVector2D displacedRelativePos = CachedRelativePos;
			FQuat displacedRelativeRot = CachedRelativeRot;
			if (UModumateObjectStatics::GetRelativeTransformOnPlaneHostedObj(parentObj,
				displacedWorldPos, displacedWorldRot.GetAxisY(), 0.0f, false, displacedRelativePos, displacedRelativeRot))
			{
				SetRelativeTransform(displacedRelativePos, displacedRelativeRot);
				parentObj->MarkDirty(EObjectDirtyFlags::Visuals);
			}
		}
	}

	void FMOIPortalImpl::TransverseObject()
	{
		FModumateObjectInstance *parent = MOI ? MOI->GetParentObject() : nullptr;
		if (parent)
		{
			FQuat parentRot = parent->GetObjectRotation();
			FVector parentAxisX = parentRot.GetAxisX();

			FVector portalOriginOffset = MOI->GetControlPoint(0);
			FVector portalSize = MOI->GetControlPoint(2) - MOI->GetControlPoint(0);
			FVector portalXDir = CachedWorldRot.GetAxisX();
			float translationSign = parentAxisX | portalXDir;
			float flipXOffset = 2.0f * portalOriginOffset.X + portalSize.X;
			CachedRelativePos.X += translationSign * flipXOffset;

			bool bPortalRotated = !CachedRelativeRot.IsIdentity(KINDA_SMALL_NUMBER);
			CachedRelativeRot = FQuat(FVector::UpVector, bPortalRotated ? 0.0f : PI);

			MOI->InvertObject();
			MOI->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	FModumateWallMount FMOIPortalImpl::GetWallMountForSelf(int32 originIndex) const
	{
		FModumateWallMount ret;
		ret.OriginIndex = originIndex;
		ret.RelativePosition = FVector::ZeroVector;
		ret.OrientationDelta = FQuat::Identity;

		if (MOI == nullptr)
		{
			return ret;
		}

		const FModumateObjectInstance *parent = MOI->GetParentObject();
		if (parent == nullptr)
		{
			return ret;
		}

		// Store the object's position relative to a corner of the parent that isn't expected to change.
		FVector originCornerWorldPos = parent->GetCorner(originIndex);
		FTransform wallTransform(parent->GetObjectRotation(), parent->GetObjectLocation());
		FVector originCornerRelPos = wallTransform.InverseTransformPosition(originCornerWorldPos);

		FVector objectWorldPos = MOI->GetObjectLocation();
		FVector objectRelPos = wallTransform.InverseTransformPosition(objectWorldPos);
		ret.RelativePosition = objectRelPos - originCornerRelPos;

		/*ret.OrientationDelta = MOI->GetObjectRotation() - parent->GetObjectRotation();
		ret.OrientationDelta.Normalize();*/
		ret.OrientationDelta = MOI->GetObjectRotation().Inverse() * parent->GetObjectRotation();
		ret.OriginalControlPoints = MOI->GetControlPoints();

		return ret;
	}

	void FMOIPortalImpl::SetWallMountForSelf(const FModumateWallMount &wm)
	{
		if (MOI)
		{
			FModumateObjectInstance *parent = MOI->GetParentObject();
			if (parent == nullptr || parent->GetObjectType() != EObjectType::OTWallSegment)
			{
				return;
			}

			// First, restore original control points before applying the relative wall mount,
			// in case they are used or modified during location/rotation setting.
			MOI->SetControlPoints(wm.OriginalControlPoints);

			// Use the stored position that was relative to the corner that wasn't supposed to change
			// to set the new position for the wall mounted object.
			FVector originCornerWorldPos = parent->GetCorner(wm.OriginIndex);
			FTransform wallTransform(parent->GetObjectRotation(), parent->GetObjectLocation());
			FVector originCornerRelPos = wallTransform.InverseTransformPosition(originCornerWorldPos);

			FVector objectRelPos = wm.RelativePosition + originCornerRelPos;
			FVector objectWorldPos = wallTransform.TransformPosition(objectRelPos);

			// For doors, constrain the mounted position to be at the wall's base, to prevent unnecessary hole boring errors
			if (MOI->GetObjectType() == EObjectType::OTDoor)
			{
				FVector baseCornerWorldPos = parent->GetCorner(0);
				objectWorldPos.Z = baseCornerWorldPos.Z;
			}

			MOI->SetObjectLocation(objectWorldPos);

			MOI->SetObjectRotation(wm.OrientationDelta * parent->GetObjectRotation());
			MOI->UpdateGeometry();
		}
	}

	void FMOIPortalImpl::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
	{
		bool bGetFarLines = ParentPage->lineClipping.IsValid();
		if (bGetFarLines)
		{
				GetFarDraftingLines(ParentPage, Plane, BoundingBox);
		}
		else
		{
			ACompoundMeshActor* actor = Cast<ACompoundMeshActor>(MOI->GetActor());

			TArray<TPair<FVector, FVector>> OutEdges;
			actor->ConvertStaticMeshToLinesOnPlane(Origin, FVector(Plane), OutEdges);

			Units::FThickness defaultThickness = Units::FThickness::Points(0.125f);
			FMColor defaultColor = FMColor::Gray64;
			FMColor swingColor = FMColor(0.0f, 0.0f, 0.0f);
			float defaultDoorOpeningDegrees = 90;

			for (auto& edge : OutEdges)
			{
				FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Key);
				FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Value);

				TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(
					Units::FCoordinates2D::WorldCentimeters(start),
					Units::FCoordinates2D::WorldCentimeters(end),
					defaultThickness, defaultColor));
				line->SetLayerTypeRecursive(FModumateLayerType::kOpeningSystemCutLine);
				ParentPage->Children.Add(line);
			}

			// draw door swing lines if the cut plane intersects the door's mesh
			// TODO: door swings for DDL 2.0 openings
#if 0 
			if (MOI->GetObjectType() == EObjectType::OTDoor && OutEdges.Num() > 0)
			{
				auto assembly = MOI->GetAssembly();
				auto portalConfig = assembly.CachedAssembly.PortalConfiguration;
				auto portalFunction = portalConfig.PortalFunction;

				// we only draw arcs for swing doors, where the cut plane's normal is vertical and the door is vertical
				bool bFloorplanPlane = FVector::Coincident(FVector::DownVector, FVector(Plane));
				FVector rotationAxis = CachedWorldRot.GetRotationAxis();
				bool bWallVertical = FVector::Parallel(FVector::UpVector, rotationAxis) || CachedWorldRot.IsIdentity(KINDA_SMALL_NUMBER);
				if (portalFunction == EPortalFunction::Swing && bFloorplanPlane && bWallVertical)
				{
					auto parent = MOI->GetParentObject();

					// Get amount of panels
					TArray<int32> panelSlotIndices;

					for (int32 idx = 0; idx < portalConfig.Slots.Num(); idx++)
					{
						auto& slot = portalConfig.Slots[idx];
						if (slot.Type == EPortalSlotType::Panel)
						{
							panelSlotIndices.Add(idx);
						}
					}

					int32 numPanels = panelSlotIndices.Num();
					FVector windowDiagonal = MOI->GetControlPoint(2) - MOI->GetControlPoint(0);

					FVector p1 = FVector::PointPlaneProject(GetCorner(0), Plane);
					FVector p2 = FVector::PointPlaneProject(GetCorner(2), Plane);

					float angle = -CachedWorldRot.GetAngle();
					angle *= FVector::DotProduct(FVector::UpVector, rotationAxis);
					// TODO: enable if door swing arcs should show when plans are facing the ceiling
					//angle *= FVector::DotProduct(FVector::DownVector, FVector(Plane));

					for (int32 panelIdx : panelSlotIndices)
					{
						auto& panel = portalConfig.Slots[panelIdx];

						// angle offset represents how much the arc needs to be rotated to accommodate for -
						// the panel being flipped in the assembly, whether it is positioned with RXLeft or RXRight, 
						// and whether the moi is inverted (for portals, transverse flips MOI->GetObjectInverted())
						// angle offset is used when the door swing arc is rotated
						float angleOffset = panel.FlipX ? 0.5f : 0.0f;
						if (MOI->GetObjectInverted())
						{
							angleOffset -= panel.FlipX ? -0.5f : 0.5f;
						}

						// TODO: this logic has already been solved in CompoundMeshActor.  Given an understanding of 2D
						// sub-graphs, make sure that the portal transforms work the same way for drafting and for the models
						FVector hingeLocation;
						if (panel.LocationX == FPortalConfiguration::RefPlaneNameMinX.ToString())
						{
							hingeLocation = !MOI->GetObjectInverted() ? p1 : p2;
						}
						else if (panel.LocationX == FPortalConfiguration::RefPlaneNameMaxX.ToString())
						{
							hingeLocation = !MOI->GetObjectInverted() ? p2 : p1;

							angleOffset += 1.0f;
						}
						// TODO: handle panel positions other than RXLeft (RefPlaneNameMinX) and RXRight (RefPlaneNameMaxX)
						else
						{
							continue;
						}

						// TODO: this is only correct as long as each panel is the same size
						float width = windowDiagonal.X / numPanels;

						// create arc object
						TSharedPtr<FDraftingArc> doorSwing = MakeShareable(new FDraftingArc(
							Units::FRadius::WorldCentimeters(width),
							Units::FAngle::Degrees(defaultDoorOpeningDegrees),
							defaultThickness,
							swingColor));
						doorSwing->SetLayerTypeRecursive(FModumateLayerType::kOpeningSystemOperatorLine);

						FVector2D hingeLocation2D = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, hingeLocation);
						doorSwing->SetLocalPosition(Units::FCoordinates2D::WorldCentimeters(hingeLocation2D));
						doorSwing->SetLocalOrientation(Units::FAngle::Radians(angle + (PI*(-0.5f + angleOffset))));

						ParentPage->Children.Add(doorSwing);

						TSharedPtr<FDraftingLine> doorLine = MakeShareable(new FDraftingLine(Units::FLength::WorldCentimeters(width), defaultThickness, swingColor));
						doorLine->SetLocalPosition(Units::FCoordinates2D::WorldCentimeters(hingeLocation2D));
						doorLine->SetLocalOrientation(Units::FAngle::Radians(angle + (PI*-0.5f)));
						doorLine->SetLayerTypeRecursive(FModumateLayerType::kOpeningSystemOperatorLine);

						ParentPage->Children.Add(doorLine);
					}
				}
			}
#endif
		}

	}

	void Modumate::FMOIPortalImpl::SetIsDynamic(bool bIsDynamic)
	{
		auto meshActor = Cast<ACompoundMeshActor>(MOI->GetActor());
		if (meshActor)
		{
			meshActor->SetIsDynamic(bIsDynamic);
		}
	}

	bool Modumate::FMOIPortalImpl::GetIsDynamic() const
	{
		auto meshActor = Cast<ACompoundMeshActor>(MOI->GetActor());
		if (meshActor)
		{
			return meshActor->GetIsDynamic();
		}
		return false;
	}

	void FMOIPortalImpl::GetFarDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane &Plane, const FBox2D& BoundingBox) const
	{
		const ACompoundMeshActor* actor = Cast<ACompoundMeshActor>(MOI->GetActor());
		const FTransform& localToWorld = actor->ActorToWorld();
		const FVector viewNormal = Plane;
		const FVector localViewNormal(localToWorld.ToMatrixWithScale().GetTransposed().TransformVector(viewNormal));

		TArray<FEdge> portalEdges;

		static auto lexicalEdgeCompare = [](const FEdge& a, const FEdge& b)
		{   // Compare lexicographically.
			for (int v = 0; v < 2; ++v)
			{
				if (a.Vertex[v].X < b.Vertex[v].X)
				{
					return true;
				}
				else if (a.Vertex[v].X > b.Vertex[v].X)
				{
					return false;
				}
				else if (a.Vertex[v].Y < b.Vertex[v].Y)
				{
					return true;
				}
				else if (a.Vertex[v].Y > b.Vertex[v].Y)
				{
					return false;
				}
			}
			return false;  // False if equal.
		};
		static auto lexicalVectorCompare = [](const FVector& a, const FVector& b)
		{
			if (a.X == b.X)
			{
				if (a.Y == b.Y)
				{
					return a.Z < b.Z;
				}
				return a.Y < b.Y;
			}
			return a.X < b.X;
		};

#if 0	// Code for extracting from nine-slice ProcMeshes:
		for (auto meshComp : actor->NineSliceLowLODComps)
		{
			if (meshComp == nullptr)
			{
				continue;
			}
			int numSections = meshComp->GetNumSections();
			for (int section = 0; section < numSections; ++section)
			{
				const FProcMeshSection* meshSection = meshComp->GetProcMeshSection(section);
				if (meshSection == nullptr)
				{
					continue;
				}
				const auto& vertices = meshSection->ProcVertexBuffer;
				const auto& indices = meshSection->ProcIndexBuffer;
				const int numIndices = indices.Num();
				ensure(numIndices % 3 == 0);

				if (numIndices > 0)
				{
					// Delete any edge that's in neighbouring triangles on assumption it's a quad.
					static const int Invalid = -1;
					struct LineIndices
					{
						int A{ Invalid };
						int B{ Invalid };
						void Normalize() { if (A > B) { Swap(A, B); } }
						bool operator==(const LineIndices& rhs) const { return A == rhs.A && B == rhs.B; }
					} lineIndices[6];

					for (int triangle = 0; triangle < numIndices; triangle += 3)
					{
						bool bBackFacing = true;
						for (int vert = 0; bBackFacing && vert < 3; ++vert)
						{
							bBackFacing &= (vertices[indices[triangle + vert]].Normal | localViewNormal) > 0;
						}
						if (bBackFacing)
						{
							continue;
						}

						for (int edge = 0; edge < 3; ++edge)
						{
							lineIndices[edge + 3] = { (int)indices[triangle + edge], (int)indices[triangle + (edge + 1) % 3] };
							lineIndices[edge + 3].Normalize();
						}

						for (int edge = 0; edge < 3; ++edge)
						{
							if (lineIndices[edge] == lineIndices[3])
							{
								lineIndices[edge].A = -1; lineIndices[3].A = -1;
							}
							else if (lineIndices[edge] == lineIndices[4])
							{
								lineIndices[edge].A = -1; lineIndices[4].A = -1;
							}
							else if (lineIndices[edge] == lineIndices[5])
							{
								lineIndices[edge].A = -1; lineIndices[5].A = -1;
							}
						}

						for (int edge = 0; edge < 3; ++edge)
						{
							if (lineIndices[edge].A != Invalid)
							{
								portalEdges.Emplace(vertices[lineIndices[edge].A].Position, vertices[lineIndices[edge].B].Position);
							}
							lineIndices[edge] = lineIndices[3 + edge];
						}

					}
					for (int edge = 0; edge < 3; ++edge)
					{
						if (lineIndices[edge].A != -1)
						{
							portalEdges.Emplace(vertices[lineIndices[edge].A].Position, vertices[lineIndices[edge].B].Position);
						}
					}

				}
			}
		}

#else


		for (auto meshComp : actor->StaticMeshComps)
		{
			if (meshComp == nullptr)
			{
				continue;
			}
			const FName& positionName = MeshAttribute::Vertex::Position;  // This is "Position "!
			const UStaticMesh* meshSection = meshComp->GetStaticMesh();
			if (meshSection == nullptr)
			{
				continue;
			}

			struct FLocalEdge
			{
				FVector A;
				FVector B;
				FVector N;
				bool bIsValid { true };
				operator bool() const { return bIsValid; }
				void Normalize() { if (lexicalVectorCompare(B, A)) { Swap(A, B); } }
				bool operator==(const FLocalEdge& rhs) const
					{ return A == rhs.A && B == rhs.B || A == rhs.B && B == rhs.A; }
				bool operator<(const FLocalEdge& rhs) const
				    { return A == rhs.A ? lexicalVectorCompare(B, rhs.B) : lexicalVectorCompare(A, rhs.A); }
			};
			TArray<FLocalEdge> edges;

			const FMeshDescription* meshDescription = meshSection->GetMeshDescription(0);
			const FTriangleArray& triangleArray = meshDescription->Triangles();
			const auto triangleIDs = triangleArray.GetElementIDs();
			const auto& attribs = meshDescription->VertexAttributes();
			for (const auto triangleID: triangleIDs)
			{
				auto vertexIDs = meshDescription->GetTriangleVertices(triangleID);
				FVector vert0 = attribs.GetAttribute<FVector>(vertexIDs[0], positionName);
				FVector vert1 = attribs.GetAttribute<FVector>(vertexIDs[1], positionName);
				FVector vert2 = attribs.GetAttribute<FVector>(vertexIDs[2], positionName);
				FVector triNormal = ((vert1 - vert0) ^ (vert2 - vert0)).GetSafeNormal();

				edges.Add({ vert0, vert1, triNormal });
				edges.Add({ vert1, vert2, triNormal });
				edges.Add({ vert2, vert0, triNormal });
			}

			for (auto& edge: edges)
			{
				edge.Normalize();
			}
			const int numEdges = edges.Num();
			Algo::Sort(edges);
			// Remove all common edges with common directions. 
			for (int e1 = 0; e1 < numEdges; ++e1)
			{
				FLocalEdge& edge1 = edges[e1];
				for (int e2 = e1 + 1; e2 < numEdges; ++e2)
				{
					FLocalEdge& edge2 = edges[e2];
					if (!(edge1 == edge2))
					{
						break;
					}
					if (edge2 && FMath::Abs(edge1.N | edge2.N) > THRESH_NORMALS_ARE_PARALLEL)
					{
						edge1.bIsValid = false;
						edge2.bIsValid = false;
						break;
					}
				}
			}

			for (auto& edge: edges)
			{
				if (edge)
				{
					portalEdges.Emplace(edge.A, edge.B);
				}
			}

		}

#endif

		Algo::ForEach(portalEdges, [localToWorld](FEdge& edge) {edge.Vertex[0] = localToWorld.TransformPosition(edge.Vertex[0]);
		    edge.Vertex[1] = localToWorld.TransformPosition(edge.Vertex[1]); });

		TArray<FEdge> clippedLines;
		for (const auto& edge: portalEdges)
		{
			clippedLines.Append(ParentPage->lineClipping->ClipWorldLineToView(edge));
		}

		// Eliminate identical lines in 2D...
		// Superfluous now that we eliminate over the whole draft, but more efficient.

		for (auto& edge: clippedLines)
		{   // Canonical form:
			if (edge.Vertex[0].X > edge.Vertex[1].X ||
				(edge.Vertex[0].X == edge.Vertex[1].X && edge.Vertex[0].Y > edge.Vertex[1].Y))
			{
				Swap(edge.Vertex[0], edge.Vertex[1]);
			}
		}
		Algo::Sort(clippedLines, lexicalEdgeCompare);

		int32 eraseIndex = Algo::Unique(clippedLines, [](const FEdge& a, const FEdge& b)
			{
				return a.Vertex[0].X == b.Vertex[0].X && a.Vertex[0].Y == b.Vertex[0].Y
					&& a.Vertex[1].X == b.Vertex[1].X && a.Vertex[1].Y == b.Vertex[1].Y;
			});
		clippedLines.RemoveAt(eraseIndex, clippedLines.Num() - eraseIndex);

		FVector2D boxClipped0;
		FVector2D boxClipped1;
		for (const auto& clippedLine: clippedLines)
		{
			FVector2D vert0(clippedLine.Vertex[0]);
			FVector2D vert1(clippedLine.Vertex[1]);

			if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
			{

				TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(
					Units::FCoordinates2D::WorldCentimeters(boxClipped0),
					Units::FCoordinates2D::WorldCentimeters(boxClipped1),
					Units::FThickness::Points(0.125f), FMColor::Gray64));
				ParentPage->Children.Add(line);
				line->SetLayerTypeRecursive(FModumateLayerType::kOpeningSystemBeyond);
			}
		}
	}

}
