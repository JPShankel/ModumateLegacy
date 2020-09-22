// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Portal.h"

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
#include "ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "DocumentManagement//ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Algo/ForEach.h"
#include "Algo/Unique.h"


class AEditModelPlayerController_CPP;


FMOIPortalImpl::FMOIPortalImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, Controller(nullptr)
	, CachedRelativePos(ForceInitToZero)
	, CachedWorldPos(ForceInitToZero)
	, CachedRelativeRot(ForceInit)
	, CachedWorldRot(ForceInit)
	, bHaveValidTransform(false)
{
}

FMOIPortalImpl::~FMOIPortalImpl()
{}

AActor *FMOIPortalImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	return world->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
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

bool FMOIPortalImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
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
			const Modumate::FGraph3DFace * parentFace = MOI->GetDocument()->GetVolumeGraph().FindFace(parentObj->ID);

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
	SetupCompoundActorGeometry();
	SetRelativeTransform(CachedRelativePos, CachedRelativeRot);
}

void FMOIPortalImpl::UpdateDynamicGeometry()
{
	SetupDynamicGeometry();
}

bool FMOIPortalImpl::SetupCompoundActorGeometry()
{
	bool bResult = false;
	if (ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(MOI->GetActor()))
	{
		FVector scale(FVector::OneVector);
		int32 parentID = MOI->GetParentID();
		if (parentID != MOD_ID_NONE)
		{
			const Modumate::FGraph3DFace * parentFace = MOI->GetDocument()->GetVolumeGraph().FindFace(parentID);
			// Get size of parent metaplane.
			if (parentFace != nullptr)
			{
				FBox2D faceSize(parentFace->Cached2DPositions);
				FVector2D planeSize = faceSize.GetSize();
				FVector2D localPosition(-faceSize.GetExtent().X, faceSize.GetExtent().Y);
				SetRelativeTransform(localPosition, CachedRelativeRot);

				const FBIMAssemblySpec& assembly = MOI->GetAssembly();
				if (assembly.Parts.Num() > 0 && !assembly.Parts[0].Mesh.NativeSize.IsZero())
				{	// Assume first part for native size.
					FVector nativeSize = assembly.Parts[0].Mesh.NativeSize * Modumate::InchesToCentimeters;
					scale.X = planeSize.X / nativeSize.X;
					scale.Z = planeSize.Y / nativeSize.Z;
					bResult = true;
				}
			}
		}

		cma->MakeFromAssembly(MOI->GetAssembly(), scale, MOI->GetObjectInverted(), true);
	}
	return bResult;
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

	if (!UModumateObjectStatics::GetWorldTransformOnPlanarObj(parentObj,
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
		if (UModumateObjectStatics::GetRelativeTransformOnPlanarObj(parentObject,
			NewTransform.GetLocation(),
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

void FMOIPortalImpl::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
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

		Modumate::Units::FThickness defaultThickness = Modumate::Units::FThickness::Points(0.125f);
		Modumate::FMColor defaultColor = Modumate::FMColor::Gray64;
		Modumate::FMColor swingColor = Modumate::FMColor(0.0f, 0.0f, 0.0f);
		float defaultDoorOpeningDegrees = 90;

		for (auto& edge : OutEdges)
		{
			FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Key);
			FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Value);

			TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
				Modumate::Units::FCoordinates2D::WorldCentimeters(start),
				Modumate::Units::FCoordinates2D::WorldCentimeters(end),
				defaultThickness, defaultColor);
			line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kOpeningSystemCutLine);
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
					TSharedPtr<FDraftingArc> doorSwing = MakeShared<FDraftingArc>(
						Units::FRadius::WorldCentimeters(width),
						Units::FAngle::Degrees(defaultDoorOpeningDegrees),
						defaultThickness,
						swingColor);
					doorSwing->SetLayerTypeRecursive(FModumateLayerType::kOpeningSystemOperatorLine);

					FVector2D hingeLocation2D = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, hingeLocation);
					doorSwing->SetLocalPosition(Units::FCoordinates2D::WorldCentimeters(hingeLocation2D));
					doorSwing->SetLocalOrientation(Units::FAngle::Radians(angle + (PI*(-0.5f + angleOffset))));

					ParentPage->Children.Add(doorSwing);

					TSharedPtr<FDraftingLine> doorLine = MakeShared<FDraftingLine>(Units::FLength::WorldCentimeters(width), defaultThickness, swingColor);
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

void FMOIPortalImpl::SetIsDynamic(bool bIsDynamic)
{
	auto meshActor = Cast<ACompoundMeshActor>(MOI->GetActor());
	if (meshActor)
	{
		meshActor->SetIsDynamic(bIsDynamic);
	}
}

bool FMOIPortalImpl::GetIsDynamic() const
{
	auto meshActor = Cast<ACompoundMeshActor>(MOI->GetActor());
	if (meshActor)
	{
		return meshActor->GetIsDynamic();
	}
	return false;
}

void FMOIPortalImpl::GetFarDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane &Plane, const FBox2D& BoundingBox) const
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
		UStaticMesh* staticMesh = meshComp->GetStaticMesh();
		if (staticMesh == nullptr)
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

		const int levelOfDetailIndex = 0;
		const FStaticMeshLODResources& meshResources = staticMesh->GetLODForExport(levelOfDetailIndex);

		TArray<FVector> positions;
		TArray<int32> indices;
		TArray<FVector> normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> tangents;

		int32 numSections = meshResources.Sections.Num();
		for (int32 section = 0; section < numSections; ++section)
		{
			UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(staticMesh, levelOfDetailIndex, section, positions, indices, normals, UVs, tangents);
			ensure(indices.Num() % 3 == 0);
			int32 numTriangles = indices.Num() / 3;
			FVector verts[3];
			for (int32 triangle = 0; triangle < numTriangles; ++triangle)
			{

				FVector vert0 = positions[indices[triangle * 3]];
				FVector vert1 = positions[indices[triangle * 3 + 1]];
				FVector vert2 = positions[indices[triangle * 3 + 2]];
				FVector triNormal = ((vert1 - vert0) ^ (vert2 - vert0)).GetSafeNormal();

				edges.Add({ vert0, vert1, triNormal });
				edges.Add({ vert1, vert2, triNormal });
				edges.Add({ vert2, vert0, triNormal });
			}

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

			TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
				Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped0),
				Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped1),
				Modumate::Units::FThickness::Points(0.125f), Modumate::FMColor::Gray64);
			ParentPage->Children.Add(line);
			line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kOpeningSystemBeyond);
		}
	}
}
