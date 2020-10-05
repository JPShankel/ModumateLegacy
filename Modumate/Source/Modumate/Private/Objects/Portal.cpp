// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Portal.h"

#include "Algo/Unique.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/ModumateObjectInstanceParts_CPP.h"


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

void FMOIPortalImpl::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = InstanceData.StaticStruct();
	OutStructPtr = &InstanceData;
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
					FVector nativeSize = assembly.GetRiggedAssemblyNativeSize();
					scale.X = planeSize.X / nativeSize.X;
					scale.Z = planeSize.Y / nativeSize.Z;
					bResult = true;
				}
			}
		}

		cma->MakeFromAssembly(MOI->GetAssembly(), scale, InstanceData.bLateralInverted, true);
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
	bHaveValidTransform = true;

	return true;
}

void FMOIPortalImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	FModumateObjectInstance* parentObject = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObject && (parentObject->GetObjectType() == EObjectType::OTMetaPlane))
	{
		parentObject->GetStructuralPointsAndLines(outPoints, outLines, bForSnapping, bForSelection);
	}
}

FQuat FMOIPortalImpl::GetRotation() const
{
	return CachedRelativeRot;
}

FVector FMOIPortalImpl::GetCorner(int32 index) const
{
	FModumateObjectInstance* parentObject = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObject && (parentObject->GetObjectType() == EObjectType::OTMetaPlane))
	{
		return parentObject->GetCorner(index);
	}

	return FVector::ZeroVector;
}

FVector FMOIPortalImpl::GetLocation() const
{
	return FVector(CachedRelativePos, 0.0f);
}

FTransform FMOIPortalImpl::GetWorldTransform() const
{
	return FTransform(CachedWorldRot, CachedWorldPos);
}

void FMOIPortalImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	FModumateObjectInstance *parent = MOI->GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	// Make the polygon adjustment handles, for modifying the parent plane's polygonal shape
	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetAdjustPolyEdge(true);
		edgeHandle->SetTargetMOI(parent);
	}
}

bool FMOIPortalImpl::GetInvertedState(FMOIStateData& OutState) const
{
	OutState = MOI->GetStateData();

	FMOIPortalData modifiedTrimData = InstanceData;
	modifiedTrimData.bNormalInverted = !modifiedTrimData.bNormalInverted;

	return OutState.CustomData.SaveStructData(modifiedTrimData);
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

		const int32 numComponents = actor->StaticMeshComps.Num();
		for (int32 component = 0; component < numComponents; ++component)
		{
			UStaticMeshComponent* staticMeshComponent = actor->StaticMeshComps[component];
			if (staticMeshComponent == nullptr)
			{
				continue;
			}

			if (actor->UseSlicedMesh[component])
			{   // Component has been nine-sliced.
				for (int32 slice = 9 * component; slice < 9 * (component + 1); ++slice)
				{
					UModumateGeometryStatics::ConvertProcMeshToLinesOnPlane(actor->NineSliceLowLODComps[slice], Origin, Plane, OutEdges);
				}
			}
			else
			{
				UModumateGeometryStatics::ConvertStaticMeshToLinesOnPlane(staticMeshComponent, Origin, Plane, OutEdges);
			}

		}

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
	const FTransform& actorToWorld = actor->ActorToWorld();
	const FVector viewNormal = Plane;

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

	struct FLocalEdge
	{
		FVector A;
		FVector B;
		FVector N;
		bool bIsValid{ true };
		operator bool() const { return bIsValid; }
		void Normalize() { if (lexicalVectorCompare(B, A)) { Swap(A, B); } }
		bool operator==(const FLocalEdge& rhs) const
		{
			return A == rhs.A && B == rhs.B || A == rhs.B && B == rhs.A;
		}
		bool operator<(const FLocalEdge& rhs) const
		{
			return A == rhs.A ? lexicalVectorCompare(B, rhs.B) : lexicalVectorCompare(A, rhs.A);
		}
	};

	const int32 numComponents = actor->StaticMeshComps.Num();
	for (int32 component = 0; component < numComponents; ++component)
	{
		const UStaticMeshComponent* staticMeshComponent = actor->StaticMeshComps[component];
		if (staticMeshComponent == nullptr)
		{
			continue;
		}

		if (actor->UseSlicedMesh[component])
		{   // Component has been nine-sliced.
			for (int32 slice = 9 * component; slice < 9 * (component + 1); ++slice)
			{
				UProceduralMeshComponent* meshComponent = actor->NineSliceLowLODComps[slice];

				if (meshComponent == nullptr)
				{
					continue;
				}

				TArray<FLocalEdge> edges;

				const FTransform localToWorld = meshComponent->GetRelativeTransform() * actorToWorld;
				int numSections = meshComponent->GetNumSections();
				for (int section = 0; section < numSections; ++section)
				{
					const FProcMeshSection* meshSection = meshComponent->GetProcMeshSection(section);
					if (meshSection == nullptr)
					{
						continue;
					}
					const auto& vertices = meshSection->ProcVertexBuffer;
					const auto& indices = meshSection->ProcIndexBuffer;
					const int32 numIndices = indices.Num();
					ensure(numIndices % 3 == 0);

					const int32 numTriangles = numIndices / 3;
					for (int32 triangle = 0; triangle < numTriangles; ++triangle)
					{

						FVector vert0 = localToWorld.TransformPosition(vertices[indices[triangle * 3]].Position);
						FVector vert1 = localToWorld.TransformPosition(vertices[indices[triangle * 3 + 1]].Position);
						FVector vert2 = localToWorld.TransformPosition(vertices[indices[triangle * 3 + 2]].Position);
						FVector triNormal = ((vert2 - vert0) ^ (vert1 - vert0)).GetSafeNormal();
						if ((viewNormal | triNormal) <= 0.0f)
						{
							edges.Add({ vert0, vert1, triNormal });
							edges.Add({ vert1, vert2, triNormal });
							edges.Add({ vert2, vert0, triNormal });
						}
					}

				}

				const int numEdges = edges.Num();
				for (auto& edge : edges)
				{
					edge.Normalize();
				}

				static constexpr float distDeltaSquare = 0.5f * 0.5f;  // 5 mm
				static constexpr float angleThreshold = 0.996f;  // 5 degrees

				// Brute-force search for internal lines.
				for (int32 e1 = 0; e1 < numEdges; ++e1)
				{
					for (int32 e2 = e1 + 1; e2 < numEdges; ++e2)
					{
						auto& edge1 = edges[e1];
						auto& edge2 = edges[e2];
						if (FVector::DistSquared(edge1.A, edge2.A) < distDeltaSquare && FVector::DistSquared(edge1.B, edge2.B) < distDeltaSquare
							&& FMath::Abs(edge1.N | edge2.N) > angleThreshold)
						{
							edge1.bIsValid = false;
							edge2.bIsValid = false;
						}
					}
				}

				for (auto& edge : edges)
				{
					if (edge)
					{
						portalEdges.Emplace(edge.A, edge.B);
					}
				}

			}
		}
		else
		{
			const FTransform localToWorld = staticMeshComponent->GetRelativeTransform() * actorToWorld;
			UStaticMesh* staticMesh = staticMeshComponent->GetStaticMesh();
			if (staticMesh == nullptr)
			{
				continue;
			}

			TArray<FLocalEdge> edges;

			const int levelOfDetailIndex = staticMesh->GetNumLODs() - 1;
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
				for (int32 triangle = 0; triangle < numTriangles; ++triangle)
				{
					FVector pos[3];
					for (int32 v = 0; v < 3; ++v)
					{
						int32 index = indices[triangle * 3 + v];
						pos[v] = localToWorld.TransformPosition(positions[index]);
					}

					// Verts are CW from front.
					FVector geomNormal = (pos[2] - pos[0]) ^ (pos[1] - pos[0]);
					bool geomFrontFacing = (geomNormal | viewNormal) <= 0.0f;
					if (geomFrontFacing)
					{
						geomNormal.Normalize();
						edges.Add({ pos[0], pos[1], geomNormal });
						edges.Add({ pos[1], pos[2], geomNormal });
						edges.Add({ pos[2], pos[0], geomNormal });
					}
				}

			}

			for (auto& edge : edges)
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

			for (auto& edge : edges)
			{
				if (edge)
				{
					portalEdges.Emplace(edge.A, edge.B);
				}
			}

		}
	}

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
