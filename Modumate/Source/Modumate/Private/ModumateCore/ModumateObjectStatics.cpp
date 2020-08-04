// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateObjectStatics.h"

#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Graph/Graph3D.h"
#include "BIMKernel/BIMProperties.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "Database/ModumateSimpleMesh.h"
#include "DocumentManagement/ModumateSnappingView.h"

using namespace Modumate;

ETrimMiterOptions UModumateObjectStatics::GetMiterOptionFromAdjacentTrim(ETrimMiterOptions OtherMiterOption)
{
	switch (OtherMiterOption)
	{
	case ETrimMiterOptions::RunPast:
		return ETrimMiterOptions::RunUntil;
	case ETrimMiterOptions::RunUntil:
		return ETrimMiterOptions::RunPast;
	case ETrimMiterOptions::None:
	case ETrimMiterOptions::MiterAngled:
	default:
		return ETrimMiterOptions::MiterAngled;
	}
}

bool UModumateObjectStatics::GetPolygonProfile(const FBIMAssemblySpec *TrimAssembly, const FSimplePolygon*& OutProfile)
{
	if (!ensure(TrimAssembly && (TrimAssembly->CachedAssembly.Layers.Num() == 1)))
	{
		return false;
	}

	const FModumateObjectAssemblyLayer &trimLayer = TrimAssembly->CachedAssembly.Layers[0];
	if (!ensure(trimLayer.SimpleMeshes.Num() == 1))
	{
		return false;
	}

	const FSimpleMeshRef &simpleMesh = trimLayer.SimpleMeshes[0];
	if (!ensure(simpleMesh.Asset.IsValid() && (simpleMesh.Asset->Polygons.Num() == 1)))
	{
		return false;
	}

	OutProfile = &simpleMesh.Asset->Polygons[0];
	return true;
}

bool UModumateObjectStatics::GetTrimValuesFromControls(const TArray<FVector> &ControlPoints, const TArray<int32> &ControlIndices,
	float &OutTrimStartLength, float &OutTrimEndLength,
	int32 &OutEdgeStartIndex, int32 &OutEdgeEndIndex, int32 &OutMountIndex,
	bool &bOutUseLengthAsPercent, ETrimMiterOptions &OutMiterOptionStart, ETrimMiterOptions &OutMiterOptionEnd)
{
	int32 numIndices = ControlIndices.Num();
	if ((ControlPoints.Num() == 2) && (numIndices >= 4))
	{
		OutTrimStartLength = ControlPoints[0].X;
		OutTrimEndLength = ControlPoints[1].X;

		OutEdgeStartIndex = ControlIndices[0];
		OutEdgeEndIndex = ControlIndices[1];
		OutMountIndex = ControlIndices[2];
		bOutUseLengthAsPercent = (ControlIndices[3] != 0);

		OutMiterOptionStart = (numIndices >= 5) ? (ETrimMiterOptions)ControlIndices[4] : ETrimMiterOptions::None;
		OutMiterOptionEnd = (numIndices >= 6) ? (ETrimMiterOptions)ControlIndices[5] : ETrimMiterOptions::None;

		return true;
	}

	return false;
}

bool UModumateObjectStatics::GetTrimControlsFromValues(float TrimStartLength, float TrimEndLength,
	int32 EdgeStartIndex, int32 EdgeEndIndex, int32 MountIndex,
	bool bUseLengthAsPercent, ETrimMiterOptions MiterOptionStart, ETrimMiterOptions MiterOptionEnd,
	TArray<FVector> &OutControlPoints, TArray<int32> &OutControlIndices)
{
	OutControlPoints.Reset(2);
	OutControlPoints.Add(FVector(TrimStartLength, 0.0f, 0.0f));
	OutControlPoints.Add(FVector(TrimEndLength, 0.0f, 0.0f));

	OutControlIndices.Reset(6);
	OutControlIndices.Add(EdgeStartIndex);
	OutControlIndices.Add(EdgeEndIndex);
	OutControlIndices.Add(MountIndex);
	OutControlIndices.Add(bUseLengthAsPercent ? 1 : 0);
	OutControlIndices.Add((int32)MiterOptionStart);
	OutControlIndices.Add((int32)MiterOptionEnd);

	return true;
}

bool UModumateObjectStatics::GetTrimEdgePosition(const Modumate::FModumateObjectInstance* TargetObject,
	int32 EdgeStartIndex, int32 EdgeEndIndex, FVector &OutEdgeStart, FVector &OutEdgeEnd)
{
	if (TargetObject == nullptr)
	{
		return false;
	}

	// trims can only be located on certain object types
	switch (TargetObject->GetObjectType())
	{
	case EObjectType::OTCabinet:
	case EObjectType::OTWallSegment:
	case EObjectType::OTWindow:
	case EObjectType::OTDoor:
		break;
	default:
		return false;
	}

	OutEdgeStart = TargetObject->GetCorner(EdgeStartIndex);
	OutEdgeEnd = TargetObject->GetCorner(EdgeEndIndex);

	if (!ensureAlways(!OutEdgeStart.Equals(OutEdgeEnd)))
	{
		return false;
	}

	return true;
}

bool UModumateObjectStatics::GetTrimGeometryOnEdge(const FModumateObjectInstance* TargetObject,
	const FBIMAssemblySpec *TrimAssembly, int32 EdgeStartIndex, int32 EdgeEndIndex,
	float TrimStartLength, float TrimEndLength, bool bUseLengthAsPercent,
	ETrimMiterOptions TrimMiterStart, ETrimMiterOptions TrimMiterEnd,
	FVector &OutTrimStart, FVector &OutTrimEnd, FVector &OutTrimNormal, FVector &OutTrimUp, int32 &OutMountIndex,
	FVector2D &OutTrimUpperExtensions, FVector2D &OutTrimOuterExtensions,
	const FVector &HintNormal, int32 HintMountIndex, bool bDoMitering)
{
	// First, find the start and end positions of the trim
	FVector edgeStartPos, edgeEndPos;
	if (!GetTrimEdgePosition(TargetObject, EdgeStartIndex, EdgeEndIndex, edgeStartPos, edgeEndPos))
	{
		return false;
	}

	FVector edgeDelta = edgeEndPos - edgeStartPos;
	float edgeLength = edgeDelta.Size();
	if (FMath::IsNearlyZero(edgeLength))
	{
		return false;
	}

	FVector trimDir = edgeDelta / edgeLength;

	if (bUseLengthAsPercent)
	{
		TrimStartLength *= edgeLength;
		TrimEndLength *= edgeLength;
	}

	float trimLength = TrimEndLength - TrimStartLength;
	if (trimLength < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutTrimStart = edgeStartPos + (TrimStartLength * trimDir);
	OutTrimEnd = edgeStartPos + (TrimEndLength * trimDir);
	OutMountIndex = INDEX_NONE;
	OutTrimUpperExtensions = OutTrimOuterExtensions = FVector2D::ZeroVector;

	const FSimplePolygon *trimProfile = nullptr;
	if (!UModumateObjectStatics::GetPolygonProfile(TrimAssembly, trimProfile))
	{
		return false;
	}
	FVector2D trimProfileSize = trimProfile->Extents.GetSize();

	// Helper function to figure out which up and normal vectors to use, based on which face is hit or the view normal,
	// or use the saved mounting index.
	auto chooseNormalAndUp = [HintNormal, HintMountIndex, TargetObject, edgeStartPos, edgeEndPos, &OutTrimNormal, &OutTrimUp, &OutMountIndex]
	(int32 indexA, int32 indexB, const FVector &normalA, const FVector &normalB)
	{
		int32 choiceIndex = HintMountIndex;
		ensureAlways((choiceIndex != INDEX_NONE) || HintNormal.IsNormalized());

		// We haven't supplied a previous mounting index, so determine the normal based on our best guess
		if (choiceIndex == INDEX_NONE)
		{
			if ((HintNormal | normalA) > (HintNormal | normalB) && (indexA != INDEX_NONE))
			{
				OutTrimNormal = normalA;
				choiceIndex = indexA;
			}
			else if (indexB != INDEX_NONE)
			{
				OutTrimNormal = normalB;
				choiceIndex = indexB;
			}
		}
		// Otherwise use the supplied mounting index
		else
		{
			if (choiceIndex == indexA)
			{
				OutTrimNormal = normalA;
			}
			else if (choiceIndex == indexB)
			{
				OutTrimNormal = normalB;
			}
		}

		if (choiceIndex != INDEX_NONE)
		{
			FVector choicePoint = TargetObject->GetCorner(choiceIndex);
			OutTrimUp = (choicePoint - edgeStartPos).GetSafeNormal();
			OutMountIndex = choiceIndex;
		}
	};

	// Find the wall that this trim is mounted to; indirectly (for portals) or directly.
	const FModumateObjectInstance *parentWall = nullptr;
	int32 wallFaceIdx = INDEX_NONE;

	// Prepare to get info from neighboring trims for mitering
	float otherTrimStartLength, otherTrimEndLength;
	int32 otherEdgeStartIndex, otherEdgeEndIndex, otherMountIndex;
	bool bOtherLengthAsPercent;
	ETrimMiterOptions otherMiterOptionStart, otherMiterOptionEnd;
	const FSimplePolygon *otherProfile;
	FVector otherTrimStart, otherTrimEnd, otherTrimNormal, otherTrimUp;
	FVector2D otherTrimUpperExtensions, otherTrimOuterExtensions;

	// Based on the object type and edge indices, choose the trim orientation
	switch (TargetObject->GetObjectType())
	{
	case EObjectType::OTCabinet:
	{
		int32 numCP = TargetObject->GetControlPoints().Num();

		int32 frontFaceIdx = (TargetObject->GetControlPointIndices().Num()== 1) ? TargetObject->GetControlPointIndex(0) : INDEX_NONE;
		FVector2D toeKickDims;
		UModumateFunctionLibrary::GetCabinetToeKickDimensions(TargetObject->GetAssembly(), toeKickDims);

		// If the points are on the same side of the prism (top/bottom), and next to each other
		if (((EdgeStartIndex < numCP) == (EdgeEndIndex < numCP)) &&
			(((EdgeStartIndex + 1) % numCP) == (EdgeEndIndex % numCP)))
		{
			// Don't allow placing trim on the top of the front face of a cabinet
			if ((frontFaceIdx == INDEX_NONE) || ((EdgeStartIndex - numCP) != frontFaceIdx))
			{
				FVector startPointOtherSide = (EdgeStartIndex < numCP) ?
					TargetObject->GetCorner(EdgeStartIndex + numCP) :
					TargetObject->GetCorner(EdgeStartIndex - numCP);
				OutTrimUp = (startPointOtherSide - edgeStartPos).GetSafeNormal();
				TargetObject->GetTriInternalNormalFromEdge(EdgeStartIndex % numCP, EdgeEndIndex % numCP, OutTrimNormal);
				OutTrimNormal *= -1.0f;

				// If the trim would go along the toe gap, offset the whole thing back
				if (frontFaceIdx == EdgeStartIndex)
				{
					OutTrimStart -= toeKickDims.X * OutTrimNormal;
					OutTrimEnd -= toeKickDims.X * OutTrimNormal;
				}
				// If the trim ends at the toe gap, retract the end
				else if (frontFaceIdx == EdgeEndIndex)
				{
					OutTrimEnd -= toeKickDims.X * trimDir;
				}
				// If the trim starts at the toe gap, offset the start
				else if ((frontFaceIdx == ((EdgeStartIndex + numCP - 1) % numCP)) && (EdgeStartIndex < numCP))
				{
					OutTrimStart += toeKickDims.X * trimDir;
				}
			}
		}
		// Vertical edges on cabinets
		else if ((EdgeStartIndex + numCP) == EdgeEndIndex)
		{
			int32 nextBottomIndex = (EdgeStartIndex + 1) % numCP;
			int32 prevBottomIndex = (EdgeStartIndex + numCP - 1) % numCP;
			int32 otherIndex = EdgeStartIndex;
			FVector normalWithNext, normalWithPrev;
			TargetObject->GetTriInternalNormalFromEdge(EdgeStartIndex, nextBottomIndex, normalWithNext);
			TargetObject->GetTriInternalNormalFromEdge(EdgeStartIndex, prevBottomIndex, normalWithPrev);
			normalWithNext *= -1.0f;
			normalWithPrev *= -1.0f;

			// If the edge touches the front face, then don't select that side
			if (EdgeStartIndex == frontFaceIdx)
			{
				nextBottomIndex = INDEX_NONE;
				OutTrimStart += toeKickDims.Y * trimDir;
			}
			else if (((EdgeStartIndex + numCP - 1) % numCP) == frontFaceIdx)
			{
				prevBottomIndex = INDEX_NONE;
				OutTrimStart += toeKickDims.Y * trimDir;
			}

			chooseNormalAndUp(nextBottomIndex, prevBottomIndex, normalWithNext, normalWithPrev);
		}
	}
	break;
	case EObjectType::OTWallSegment:
	{
		const FModumateObjectInstance *parent = TargetObject->GetParentObject();
		if (parent == nullptr)
		{
			return false;
		}

		// If the points are on the same side of the prism , and next to each other
		int32 numCP = parent->GetControlPoints().Num();

		if (((EdgeStartIndex < numCP) == (EdgeEndIndex < numCP)) &&
			(((EdgeStartIndex + 1) % numCP) == (EdgeEndIndex % numCP)))
		{
			FVector segmentDirection = (TargetObject->GetCorner(EdgeEndIndex) - TargetObject->GetCorner(EdgeStartIndex)).GetSafeNormal();

			OutTrimStart = TargetObject->GetCorner(EdgeStartIndex) + segmentDirection * TrimStartLength;
			OutTrimEnd = TargetObject->GetCorner(EdgeStartIndex) + segmentDirection * TrimEndLength;

			float windingDir = (EdgeEndIndex < numCP) ? -1 : 1;
			FVector wallDir = TargetObject->GetNormal();
			OutTrimNormal = windingDir*wallDir;
			OutTrimUp = windingDir * (OutTrimNormal ^ segmentDirection).GetSafeNormal();
		}
		// If the points are across the prism
		else if ((EdgeStartIndex + numCP) == EdgeEndIndex)
		{
			int32 nextIndex = (EdgeStartIndex + 1) % numCP;
			int32 previousIndex = (EdgeStartIndex + numCP - 1) % numCP;

			FVector nextDirection = TargetObject->GetCorner(nextIndex) - TargetObject->GetCorner(EdgeStartIndex);
			FVector previousDirection = TargetObject->GetCorner(previousIndex) - TargetObject->GetCorner(EdgeStartIndex);

			OutTrimStart = TargetObject->GetCorner(EdgeStartIndex);
			OutTrimEnd = TargetObject->GetCorner(EdgeEndIndex);

			FVector segmentDirection = OutTrimEnd - OutTrimStart;

			FVector nextNormal = (nextDirection ^ segmentDirection).GetSafeNormal();
			FVector previousNormal = (previousDirection ^ segmentDirection).GetSafeNormal();

			// TODO: use mouse hint to disambiguate edge hits along a straight path of colinear points, for now just pick the first direction
			float normalDir = 1.0f;
			if (!FMath::IsNearlyZero(nextNormal | HintNormal, 0.01f))
			{
				OutTrimUp = previousNormal;
				OutMountIndex = nextIndex;
			}
			else
			{
				OutTrimUp = -nextNormal;
				normalDir = -1.0f;
				OutMountIndex = previousIndex;
			}
			OutTrimNormal = normalDir * (OutTrimUp ^ segmentDirection).GetSafeNormal();
		}
	}
	break;
	case EObjectType::OTWindow:
	case EObjectType::OTDoor:
	{
		int32 numSides = TargetObject->GetControlPoints().Num();
		if (!ensureMsgf(numSides == 4, TEXT("Only portals with 4 sides are supported for now!")))
		{
			return false;
		}

		const AActor *targetActor = TargetObject->GetActor();
		if (!ensure(targetActor != nullptr))
		{
			return false;
		}

		FTransform targetTransform = targetActor->GetActorTransform();
		FVector portalDirAlongWall = targetTransform.GetUnitAxis(EAxis::X);
		FVector portalNormal = targetTransform.GetUnitAxis(EAxis::Y);
		FVector portalUp = targetTransform.GetUnitAxis(EAxis::Z);
		bool bOnFrontOfPortal = (EdgeStartIndex < numSides);
		float normalSign = bOnFrontOfPortal ? 1.0f : -1.0f;

		OutTrimNormal = normalSign * portalNormal;

		int32 sideIndex = EdgeStartIndex % numSides;
		switch (sideIndex)
		{
		case 0:	// left side of portal
			OutTrimUp = -portalDirAlongWall;
			break;
		case 1:	// top of portal
			OutTrimUp = portalUp;
			break;
		case 2:	// right side of portal
			OutTrimUp = portalDirAlongWall;
			break;
		case 3:	// bottom of portal
			// Don't allow trims on the bottoms of doors that would extend below the floor
			if (TargetObject->GetObjectType() == EObjectType::OTDoor)
			{
				return false;
			}
			else
			{
				OutTrimUp = -portalUp;
			}
			break;
		default:
			return false;
		}

		parentWall = TargetObject->GetParentObject();

		if (bDoMitering)
		{
			auto siblings = TargetObject->GetChildObjects();
			for (const FModumateObjectInstance *sibling : siblings)
			{
				// Find each sibling trim and their geometry values so that they can be mitered
				if (sibling && (sibling->GetObjectType() == EObjectType::OTTrim) &&
					UModumateObjectStatics::GetTrimValuesFromControls(sibling->GetControlPoints(), sibling->GetControlPointIndices(),
						otherTrimStartLength, otherTrimEndLength, otherEdgeStartIndex, otherEdgeEndIndex, otherMountIndex,
						bOtherLengthAsPercent, otherMiterOptionStart, otherMiterOptionEnd) &&
					(otherEdgeStartIndex != EdgeStartIndex) &&
					UModumateObjectStatics::GetTrimGeometryOnEdge(TargetObject, &sibling->GetAssembly(),
						otherEdgeStartIndex, otherEdgeEndIndex, otherTrimStartLength, otherTrimEndLength, bOtherLengthAsPercent,
						otherMiterOptionStart, otherMiterOptionEnd, otherTrimStart, otherTrimEnd, otherTrimNormal, otherTrimUp,
						otherMountIndex, otherTrimUpperExtensions, otherTrimOuterExtensions,
						FVector::ZeroVector, otherMountIndex, false))
				{
					bool bOtherOnFrontOfPortal = (otherEdgeStartIndex < numSides);
					if ((bOnFrontOfPortal == bOtherOnFrontOfPortal) && !FVector::Orthogonal(trimDir, otherTrimUp))
					{
						int32 otherSideIndex = otherEdgeStartIndex % numSides;
						if (!UModumateObjectStatics::GetPolygonProfile(&sibling->GetAssembly(), otherProfile))
						{
							continue;
						}

						FVector2D otherTrimSize = otherProfile->Extents.GetSize();

						// sibling is the next one, clockwise
						if ((otherSideIndex == ((sideIndex + 1) % numSides)) &&
							FMath::IsNearlyZero(otherTrimStartLength) && (FMath::IsNearlyEqual(TrimEndLength, edgeLength)))
						{
							ETrimMiterOptions miterOption = (TrimMiterEnd != ETrimMiterOptions::None) ? TrimMiterEnd :
								GetMiterOptionFromAdjacentTrim(otherMiterOptionStart);

							// TODO: handle other miter cases
							if (miterOption == ETrimMiterOptions::MiterAngled)
							{
								FVector miterRayOrigin = OutTrimEnd + (trimProfileSize.X * OutTrimUp);
								FVector otherMiterBase = otherTrimStart + (otherTrimSize.X * otherTrimUp);
								FPlane otherMiterPlane(otherMiterBase, -otherTrimUp);
								FVector miterHit = FMath::RayPlaneIntersection(miterRayOrigin, trimDir, otherMiterPlane);
								float miterHitLength = (miterHit - OutTrimStart) | trimDir;
								if (miterHitLength > trimLength)
								{
									OutTrimUpperExtensions.Y = (miterHitLength - trimLength);
								}
							}
						}
						// sibling is previous one, counter clockwise
						else if ((otherSideIndex == ((sideIndex + numSides - 1) % numSides)) &&
							FMath::IsNearlyZero(TrimStartLength) && (FMath::IsNearlyEqual(otherTrimEndLength, 1.0f)))
						{
							ETrimMiterOptions miterOption = (TrimMiterStart != ETrimMiterOptions::None) ? TrimMiterStart :
								GetMiterOptionFromAdjacentTrim(otherMiterOptionEnd);

							// TODO: handle other miter cases
							if (miterOption == ETrimMiterOptions::MiterAngled)
							{
								FVector miterRayOrigin = OutTrimStart + (trimProfileSize.X * OutTrimUp);
								FVector otherMiterBase = otherTrimEnd + (otherTrimSize.X * otherTrimUp);
								FPlane otherMiterPlane(otherMiterBase, -otherTrimUp);
								FVector miterHit = FMath::RayPlaneIntersection(miterRayOrigin, trimDir, otherMiterPlane);
								float miterHitLength = (miterHit - OutTrimStart) | trimDir;
								if (miterHitLength < 0.0f)
								{
									OutTrimUpperExtensions.X = -miterHitLength;
								}
							}
						}
					}
				}
			}
		}
	}
	break;
	default:
		return false;
	}

	if (parentWall && (wallFaceIdx != INDEX_NONE))
	{
		// TODO: improve this when finishes participate in mitering, and have lengths that might differ from the wall
		auto wallChildren = parentWall->GetChildObjects();
		for (const FModumateObjectInstance *wallChild : wallChildren)
		{
			int32 finishFaceIdx = UModumateObjectStatics::GetParentFaceIndex(wallChild);

			if (finishFaceIdx == wallFaceIdx)
			{
				float finishThickness = wallChild->CalculateThickness();
				FVector finishOffset = OutTrimNormal * finishThickness;
				OutTrimStart += finishOffset;
				OutTrimEnd += finishOffset;
				break;
			}
		}
	}

	return true;
}

bool UModumateObjectStatics::GetMoiHoleVertsWorld(const ::FBIMAssemblySpec *HoleAssembly, const FTransform &HoleActorTransform, TArray<FVector> &OutWorldHoleVerts)
{
	OutWorldHoleVerts.Reset();

	TArray<FVector> localHoleVerts;
	if (UModumateObjectStatics::GetMoiHoleVerts(HoleAssembly, localHoleVerts))
	{
		for (FVector& localHoleVert : localHoleVerts)
		{
			OutWorldHoleVerts.Add(HoleActorTransform.TransformPosition(localHoleVert));
		}

		return true;
	}

	return false;
}

TArray<FVector> UModumateObjectStatics::GetMoiActorHoleVertsWorldLocations(AActor *MoiActor)
{
	TArray<FVector> worldHoleVerts;

	if (MoiActor)
	{
		TArray<FVector> localHoleVerts;
		if (UModumateObjectStatics::GetMoiHoleVerts(MoiActor, localHoleVerts))
		{
			FTransform holeActorTransform = MoiActor->GetActorTransform();
			for (const FVector& localHoleVert : localHoleVerts)
			{
				worldHoleVerts.Add(holeActorTransform.TransformPosition(localHoleVert));
			}
		}
	}

	return worldHoleVerts;
}

bool UModumateObjectStatics::GetMoiHoleVerts(const ::FBIMAssemblySpec *HoleAssembly, TArray<FVector> &OutLocalHoleVerts)
{
	OutLocalHoleVerts.Reset();
	return false;
}

bool UModumateObjectStatics::GetMoiHoleVerts(AActor *TargetActor, TArray<FVector> &OutLocalHoleVerts)
{
	OutLocalHoleVerts.Reset();

	if (TargetActor != nullptr)
	{
		const FBIMAssemblySpec* holeAssembly = nullptr;

		UWorld *world = TargetActor->GetWorld();
		AEditModelGameState_CPP *gameState = world->GetGameState<AEditModelGameState_CPP>();
		FModumateObjectInstance *moi = gameState->Document.ObjectFromActor(TargetActor);

		// If this actor belongs to a real MOI, then we can get its real assembly directly.
		if (moi)
		{
			holeAssembly = &moi->GetAssembly();
		}
		// Otherwise, the actor might be a temporary pending MOI, whose geometry we'll generate.
		else if (auto *compoundMeshActor = Cast<ACompoundMeshActor>(TargetActor))
		{
			if ((compoundMeshActor->TempObjectToolMode != EToolMode::VE_NONE) &&
				!compoundMeshActor->TempAssemblyKey.IsNone())
			{
				holeAssembly = gameState->Document.PresetManager.GetAssemblyByKey(compoundMeshActor->TempObjectToolMode, compoundMeshActor->TempAssemblyKey);
			}
		}

		return UModumateObjectStatics::GetMoiHoleVerts(holeAssembly, OutLocalHoleVerts);
	}

	return false;
}

bool UModumateObjectStatics::FindPortalTrimOverlaps(FModumateObjectInstance *wall, const FBIMAssemblySpec &portalAssembly, const FTransform &portalTransform,
	TArray<FModumateObjectInstance *> &outTrimsToDelete,
	TMap<FModumateObjectInstance *, TPair<FVector2D, FVector2D>> &outTrimsToSplit,
	TMap<FModumateObjectInstance *, FVector2D> &outTrimsToModify)
{
	outTrimsToDelete.Reset();
	outTrimsToSplit.Reset();
	outTrimsToModify.Reset();

	if ((wall == nullptr) || (wall->GetControlPoints().Num() < 2))
	{
		return false;
	}

	// calculate common wall geometric properties
	const FVector &wallStart = wall->GetControlPoint(0);
	const FVector &wallEnd = wall->GetControlPoint(1);
	FVector wallDelta = wallEnd - wallStart;
	float wallLength = wallDelta.Size();
	if (FMath::IsNearlyZero(wallLength))
	{
		return false;
	}
	FVector wallDir = wallDelta / wallLength;
	FVector wallUp = FVector::UpVector;

	TArray<FVector> portalHoleVerts;
	if (UModumateObjectStatics::GetMoiHoleVertsWorld(&portalAssembly, portalTransform, portalHoleVerts))
	{
		float minVertexAlongWall = wallLength, maxVertexAlongWall = 0.0f;
		for (const FVector &holeVert : portalHoleVerts)
		{
			FVector holeVertRelative = holeVert - wallStart;
			float vertexAboveBase = holeVertRelative | wallUp;
			if (FMath::IsNearlyZero(vertexAboveBase))
			{
				float vertexAlongWall = holeVertRelative | wallDir;

				if (vertexAlongWall < minVertexAlongWall)
				{
					minVertexAlongWall = vertexAlongWall;
				}
				if (vertexAlongWall > maxVertexAlongWall)
				{
					maxVertexAlongWall = vertexAlongWall;
				}
			}
		}

		if (minVertexAlongWall < maxVertexAlongWall)
		{
			TArray<FModumateObjectInstance *> wallChildren = wall->GetChildObjects();
			for (FModumateObjectInstance *wallChild : wallChildren)
			{
				if (wallChild && (wallChild->GetObjectType() == EObjectType::OTTrim) &&
					(wallChild->GetControlPoints().Num() >= 2) && (wallChild->GetControlPointIndices().Num() >= 2))
				{
					FModumateObjectInstance *trimObj = wallChild;

					// Check if the trim is along one of the bottom long edges of the wall
					int32 trimStartIdx = trimObj->GetControlPointIndex(0);
					int32 trimEndIdx = trimObj->GetControlPointIndex(1);
					if ((trimStartIdx + 1 == trimEndIdx) && (trimStartIdx % 2 == 0) && (trimStartIdx < 4))
					{
						float trimStartAlongWall = trimObj->GetControlPoint(0).X;
						float trimEndAlongWall = trimObj->GetControlPoint(1).X;

						// if the trim is on either side of the hole, skip it
						if (((trimStartAlongWall < minVertexAlongWall) && (trimEndAlongWall < minVertexAlongWall)) ||
							((trimStartAlongWall > maxVertexAlongWall) && (trimEndAlongWall > maxVertexAlongWall)))
						{
							continue;
						}
						// if the trim is fully contained inside the hole, then delete it
						if ((trimStartAlongWall > minVertexAlongWall) && (trimEndAlongWall < maxVertexAlongWall))
						{
							outTrimsToDelete.Add(trimObj);
						}
						// if the trim fully contains the hole, then split it
						else if ((trimStartAlongWall < minVertexAlongWall) && (trimEndAlongWall > maxVertexAlongWall))
						{
							outTrimsToSplit.Add(trimObj, TPair<FVector2D, FVector2D>(
								FVector2D(trimStartAlongWall, minVertexAlongWall),
								FVector2D(maxVertexAlongWall, trimEndAlongWall)
								));
						}
						// if the trim's end overlaps with the hole, cut off the end
						else if ((trimStartAlongWall < minVertexAlongWall) && (trimEndAlongWall > minVertexAlongWall))
						{
							outTrimsToModify.Add(trimObj, FVector2D(trimStartAlongWall, minVertexAlongWall));
						}
						// if the trim's start overlaps with the hole, cut off the start
						else if ((trimStartAlongWall < maxVertexAlongWall) && (trimEndAlongWall > maxVertexAlongWall))
						{
							outTrimsToModify.Add(trimObj, FVector2D(maxVertexAlongWall, trimEndAlongWall));
						}
					}
				}
			}

			return (outTrimsToDelete.Num() > 0) || (outTrimsToSplit.Num() > 0) || (outTrimsToModify.Num() > 0);
		}
	}

	return false;
}

bool UModumateObjectStatics::GetRelativeTransformOnPlaneHostedObj(
	const Modumate::FModumateObjectInstance *PlaneHostedObj,
	const FVector &WorldPos, const FVector &WorldNormal,
	float DistanceFromBottom, bool bUseDistanceFromBottom,
	FVector2D &OutRelativePos, FQuat &OutRelativeRot)
{
	const FModumateObjectInstance *hostParent = PlaneHostedObj->GetParentObject();
	if (!(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return false;
	}

	FVector hostOrigin = hostParent->GetObjectLocation();
	FVector hostNormal = hostParent->GetNormal();
	FQuat hostRot = hostParent->GetObjectRotation();

	FVector pointToProject = WorldPos;

	if (bUseDistanceFromBottom)
	{
		const FGraph3DFace *parentFace = PlaneHostedObj->GetDocument()->GetVolumeGraph().FindFace(hostParent->ID);
		FVector2D faceRelativePos = parentFace->ProjectPosition2D(WorldPos);

		FVector2D faceRelativeRayEnd = parentFace->ProjectPosition2D(WorldPos - FVector::UpVector);
		if (!faceRelativeRayEnd.Equals(faceRelativePos))
		{
			FVector2D faceBottomRayDir = (faceRelativeRayEnd - faceRelativePos).GetSafeNormal();
			float minRayDist = 0.0f;
			bool bHaveRayHit = false;

			const TArray<FVector2D> &facePoints = parentFace->Cached2DPositions;
			int32 numPoints = facePoints.Num();
			for (int32 pointIdx1 = 0; pointIdx1 < numPoints; ++pointIdx1)
			{
				int32 pointIdx2 = (pointIdx1 + 1) % numPoints;
				const FVector2D &point1 = facePoints[pointIdx1];
				const FVector2D &point2 = facePoints[pointIdx2];
				FVector2D edgeDelta = point2 - point1;
				float edgeLen = edgeDelta.Size();
				if (!ensureAlways(!FMath::IsNearlyZero(edgeLen)))
				{
					continue;
				}
				FVector2D edgeDir = edgeDelta / edgeLen;

				FVector2D edgeIntersection, rayDists;
				bool bRaysColinear;
				bool bIntersection = UModumateGeometryStatics::RayIntersection2D(faceRelativePos, faceBottomRayDir, point1, edgeDir,
					edgeIntersection, rayDists.X, rayDists.Y, bRaysColinear);

				if (bIntersection && !bRaysColinear && (rayDists.Y < edgeLen) &&
					((rayDists.X < minRayDist) || !bHaveRayHit))
				{
					minRayDist = rayDists.X;
					bHaveRayHit = true;
				}
			}

			if (bHaveRayHit)
			{
				FVector worldRayStart = parentFace->DeprojectPosition(faceRelativePos);
				FVector worldRayEnd = parentFace->DeprojectPosition(faceRelativeRayEnd);
				FVector worldRay = (worldRayEnd - worldRayStart).GetSafeNormal();

				pointToProject += (minRayDist - DistanceFromBottom) * worldRay;
			}
		}
	}

	OutRelativePos = UModumateGeometryStatics::ProjectPoint2D(pointToProject, hostRot.GetAxisX(), hostRot.GetAxisY(), hostOrigin);

	// TODO: support more than just portal-style "flipped or not" relative rotation about Z
	bool bSameNormals = FVector::Coincident(WorldNormal, hostNormal);
	OutRelativeRot = FQuat(FVector::UpVector, bSameNormals ? 0.0f : PI);

	return true;
}

bool UModumateObjectStatics::GetWorldTransformOnPlaneHostedObj(
	const Modumate::FModumateObjectInstance *PlaneHostedObj,
	const FVector2D &RelativePos, const FQuat &RelativeRot,
	FVector &OutWorldPos, FQuat &OutWorldRot)
{
	const FModumateObjectInstance *hostParent = PlaneHostedObj->GetParentObject();
	if (!(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return false;
	}

	FVector hostOrigin = hostParent->GetObjectLocation();
	FVector hostNormal = hostParent->GetNormal();
	FQuat hostRot = hostParent->GetObjectRotation();

	// TODO: support more than just portal-style "flipped or not" relative rotation about Z
	bool bSameNormals = RelativeRot.IsIdentity(KINDA_SMALL_NUMBER);

	FVector startExtrusionDelta, endExtrusionDelta;
	UModumateObjectStatics::GetExtrusionDeltas(PlaneHostedObj, startExtrusionDelta, endExtrusionDelta);
	const FVector &extrusionDelta = bSameNormals ? endExtrusionDelta : startExtrusionDelta;

	OutWorldPos = hostOrigin +
		(hostRot.GetAxisX() * RelativePos.X) +
		(hostRot.GetAxisY() * RelativePos.Y) +
		extrusionDelta;
	
	// Portal rotation should be +Z up, +Y away from the wall, and +X along the wall.
	OutWorldRot = RelativeRot * FRotationMatrix::MakeFromYZ(hostNormal, -hostRot.GetAxisY()).ToQuat();

	return true;
}

int32 UModumateObjectStatics::GetParentFaceIndex(const FModumateObjectInstance *FaceMountedObj)
{
	if (FaceMountedObj == nullptr)
	{
		return INDEX_NONE;
	}

	switch (FaceMountedObj->GetObjectType())
	{
	case EObjectType::OTSurfaceGraph:
	case EObjectType::OTFurniture:
		if (FaceMountedObj->GetControlPointIndices().Num() >= 1)
		{
			return FaceMountedObj->GetControlPointIndex(0);
		}
	default:
		break;
	}

	return INDEX_NONE;
}

int32 UModumateObjectStatics::GetFaceIndexFromTargetHit(const FModumateObjectInstance *HitObject, const FVector &HitLocation, const FVector &HitNormal)
{
	if (HitObject == nullptr)
	{
		return INDEX_NONE;
	}

	const static float edgeTolerance = 0.01f;

	switch (HitObject->GetObjectType())
	{
	case EObjectType::OTRoofFace:
	case EObjectType::OTWallSegment:
	case EObjectType::OTFloorSegment:
	{
		const Modumate::FModumateObjectInstance *hostParent = HitObject->GetParentObject();
		if (!ensure(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			return INDEX_NONE;
		}

		FVector planeUp = hostParent->GetNormal();
		float planeHitNormalDot = planeUp | HitNormal;
		if (planeHitNormalDot >= THRESH_NORMALS_ARE_PARALLEL)
		{
			return 0;
		}
		else if (planeHitNormalDot <= -THRESH_NORMALS_ARE_PARALLEL)
		{
			return 1;
		}
		else
		{
			TArray<FVector> sidePoints;
			TArray<FVector2D> sidePoints2D;
			int32 numCorners = hostParent->GetControlPoints().Num();
			for (int32 curBottomIdx = 0; curBottomIdx < numCorners; ++curBottomIdx)
			{
				int32 nextBottomIdx = (curBottomIdx + 1) % numCorners;
				int32 curTopIdx = curBottomIdx + numCorners;
				int32 nextTopIdx = nextBottomIdx + numCorners;

				FVector curBottomCorner = HitObject->GetCorner(curBottomIdx);
				FVector nextBottomCorner = HitObject->GetCorner(nextBottomIdx);
				FVector curTopCorner = HitObject->GetCorner(curTopIdx);
				FVector nextTopCorner = HitObject->GetCorner(nextTopIdx);

				sidePoints = { curBottomCorner, nextBottomCorner, nextTopCorner, curTopCorner };
				FPlane sidePlane(ForceInitToZero);
				if (!ensure(UModumateGeometryStatics::GetPlaneFromPoints(sidePoints, sidePlane)) ||
					!FVector::Parallel(HitNormal, sidePlane) ||
					!FMath::IsNearlyZero(sidePlane.PlaneDot(HitLocation), PLANAR_DOT_EPSILON))
				{
					continue;
				}

				FVector sidePlaneBasisX, sidePlaneBasisY;
				UModumateGeometryStatics::FindBasisVectors(sidePlaneBasisX, sidePlaneBasisY, sidePlane);
				FVector2D projectedHitPoint = UModumateGeometryStatics::ProjectPoint2D(HitLocation, sidePlaneBasisX, sidePlaneBasisY, curBottomCorner);

				sidePoints2D.Reset(sidePoints.Num());
				for (const FVector &sidePoint : sidePoints)
				{
					sidePoints2D.Add(UModumateGeometryStatics::ProjectPoint2D(sidePoint, sidePlaneBasisX, sidePlaneBasisY, curBottomCorner));
				}

				if (UModumateGeometryStatics::IsPointInPolygon(projectedHitPoint, sidePoints2D))
				{
					return 2 + curBottomIdx;
				}
			}

			return INDEX_NONE;
		}
	}
	break;
	default:
		return INDEX_NONE;
	}
}

bool UModumateObjectStatics::GetGeometryFromFaceIndex(const Modumate::FModumateObjectInstance *Host, int32 FaceIndex,
	TArray<FVector> &OutFacePoints, FVector &OutNormal, FVector &OutFaceAxisX, FVector &OutFaceAxisY)
{
	OutFacePoints.Reset();

	if ((Host == nullptr) || (FaceIndex == INDEX_NONE))
	{
		return false;
	}

	switch (Host->GetObjectType())
	{
	case EObjectType::OTRoofFace:
	case EObjectType::OTWallSegment:
	case EObjectType::OTFloorSegment:
	{
		const Modumate::FModumateObjectInstance *hostParent = Host->GetParentObject();
		if (!ensure(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			return false;
		}

		FVector hostNormal = hostParent->GetNormal();
		const auto &cps = hostParent->GetControlPoints();
		int32 numCorners = cps.Num();
		if (numCorners < 3)
		{
			return false;
		}

		if (FaceIndex < 2)
		{
			bool bTopFace = (FaceIndex == 0);
			int32 cornerIdxOffset = bTopFace ? numCorners : 0;
			for (int32 i = 0; i < numCorners; ++i)
			{
				int32 cornerIdx = i + cornerIdxOffset;
				OutFacePoints.Add(Host->GetCorner(cornerIdx));
			}

			OutNormal = hostNormal * (bTopFace ? 1.0f : -1.0f);
			UModumateGeometryStatics::FindBasisVectors(OutFaceAxisX, OutFaceAxisY, OutNormal);

			return true;
		}
		else if (FaceIndex < (numCorners + 2))
		{
			int32 bottomStartCornerIdx = FaceIndex - 2;
			int32 bottomEndCornerIdx = (bottomStartCornerIdx + 1) % numCorners;
			int32 topStartCornerIdx = bottomStartCornerIdx + numCorners;
			int32 topEndCornerIdx = bottomEndCornerIdx + numCorners;

			FVector bottomStartCorner = Host->GetCorner(bottomStartCornerIdx);
			FVector bottomEndCorner = Host->GetCorner(bottomEndCornerIdx);
			FVector topStartCorner = Host->GetCorner(topStartCornerIdx);
			FVector topEndCorner = Host->GetCorner(topEndCornerIdx);

			FVector bottomEdgeDir = (bottomEndCorner - bottomStartCorner).GetSafeNormal();
			FVector topEdgeDir = (topEndCorner - topStartCorner).GetSafeNormal();
			if (!FVector::Parallel(bottomEdgeDir, topEdgeDir) || !bottomEdgeDir.IsNormalized() || !topEdgeDir.IsNormalized())
			{
				return false;
			}

			FVector startEdgeDir = (topStartCorner - bottomStartCorner).GetSafeNormal();
			FVector endEdgeDir = (topEndCorner - bottomEndCorner).GetSafeNormal();
			FVector startNormal = (bottomEdgeDir ^ startEdgeDir).GetSafeNormal();
			FVector endNormal = (bottomEdgeDir ^ endEdgeDir).GetSafeNormal();
			if (!FVector::Coincident(startNormal, endNormal) || !startNormal.IsNormalized() || !endNormal.IsNormalized())
			{
				return false;
			}

			OutFacePoints.Add(bottomStartCorner);
			OutFacePoints.Add(bottomEndCorner);
			OutFacePoints.Add(topEndCorner);
			OutFacePoints.Add(topStartCorner);

			OutNormal = startNormal;
			OutFaceAxisX = bottomEdgeDir;
			OutFaceAxisY = OutNormal ^ OutFaceAxisX;

			return true;
		}
		else
		{
			return false;
		}
	}
	break;
	default:
		return false;
	}
}

void UModumateObjectStatics::EdgeConnectedToValidPlane(const Modumate::FGraph3DEdge *GraphEdge, const FModumateDocument *Doc,
	bool &bOutConnectedToEmptyPlane, bool &bOutConnectedToSelectedPlane)
{
	bOutConnectedToEmptyPlane = false;
	bOutConnectedToSelectedPlane = false;

	if ((GraphEdge == nullptr) || (Doc == nullptr))
	{
		return;
	}

	for (const auto &edgeFaceConn : GraphEdge->ConnectedFaces)
	{
		const auto *connectedFaceMOI = Doc->GetObjectById(FMath::Abs(edgeFaceConn.FaceID));
		if (connectedFaceMOI && (connectedFaceMOI->GetObjectType() == EObjectType::OTMetaPlane))
		{
			const TArray<int32> &faceMOIChildIDs = connectedFaceMOI->GetChildIDs();
			int32 numChildren = faceMOIChildIDs.Num();

			// Connected to empty plane
			if (numChildren == 0)
			{
				bOutConnectedToEmptyPlane = true;
			}

			if (connectedFaceMOI->IsSelected())
			{
				bOutConnectedToSelectedPlane = true;
			}
			else if (numChildren == 1)
			{
				// Connected to plane that is selectd or whose child is selected
				const auto *faceChild = Doc->GetObjectById(faceMOIChildIDs[0]);
				if (faceChild && faceChild->IsSelected())
				{
					bOutConnectedToSelectedPlane = true;
				}
			}
		}
	}
}

void UModumateObjectStatics::ShouldMetaObjBeEnabled(const Modumate::FModumateObjectInstance *MetaMOI,
	bool &bOutShouldBeVisible, bool &bOutShouldCollisionBeEnabled, bool &bOutIsConnected)
{
	bOutShouldBeVisible = bOutShouldCollisionBeEnabled = bOutIsConnected = false;

	if (MetaMOI == nullptr)
	{
		return;
	}

	const AActor *metaActor = MetaMOI->GetActor();
	const FModumateDocument *doc = MetaMOI->GetDocument();
	UWorld *world = metaActor ? metaActor->GetWorld() : nullptr;
	AEditModelPlayerController_CPP *playerController = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;

	if (playerController && doc)
	{
		// If we're in room-viewing mode, we don't show any objects besides Rooms
		if (playerController->EMPlayerState->SelectedViewMode == EEditViewModes::Rooms)
		{
			return;
		}

		EObjectType objectType = MetaMOI->GetObjectType();
		int32 objID = MetaMOI->ID;
		const FGraph3D &volumeGraph = doc->GetVolumeGraph();

		bool bInMetaToolMode = false;
		EToolMode curToolMode = playerController->GetToolMode();
		switch (curToolMode)
		{
		case EToolMode::VE_WALL:
		case EToolMode::VE_FLOOR:
		case EToolMode::VE_ROOF_FACE:
		case EToolMode::VE_STAIR:
		case EToolMode::VE_METAPLANE:
		case EToolMode::VE_STRUCTURELINE:
			bInMetaToolMode = true;
			break;
		default:
			break;
		}

		bool bEnabledByViewMode = playerController->EMPlayerState->IsObjectTypeEnabledByViewMode(objectType);
		bool bConnectedToEmptyPlane = false;
		bool bConnectedToSelectedPlane = false;

		switch (objectType)
		{
		case EObjectType::OTMetaVertex:
		{
			auto *graphVertex = volumeGraph.FindVertex(objID);
			if (graphVertex == nullptr)
			{
				bOutShouldBeVisible = false;
				bOutShouldCollisionBeEnabled = false;
				break;
			}

			bOutIsConnected = (graphVertex->ConnectedEdgeIDs.Num() > 0);
			for (FGraphSignedID connectedEdgeID : graphVertex->ConnectedEdgeIDs)
			{
				const auto *connectedEdge = volumeGraph.FindEdge(connectedEdgeID);
				EdgeConnectedToValidPlane(connectedEdge, doc, bConnectedToEmptyPlane, bConnectedToSelectedPlane);
				if (bConnectedToSelectedPlane)
				{
					break;
				}
			}

			bOutShouldBeVisible = (bEnabledByViewMode || bConnectedToSelectedPlane || MetaMOI->IsSelected());
			bOutShouldCollisionBeEnabled = bOutShouldBeVisible || bConnectedToEmptyPlane || bInMetaToolMode;
			break;
		}
		case EObjectType::OTMetaEdge:
		{
			auto *graphEdge = volumeGraph.FindEdge(objID);
			if (graphEdge == nullptr)
			{
				bOutShouldBeVisible = false;
				bOutShouldCollisionBeEnabled = false;
				break;
			}

			bool bEdgeHasChild = false;
			bool bEdgeHasSelectedChild = false;
			const TArray<int32> &edgeChildIDs = MetaMOI->GetChildIDs();
			for (int32 edgeChildID : edgeChildIDs)
			{
				const FModumateObjectInstance *edgeChildObj = MetaMOI->GetDocument()->GetObjectById(edgeChildID);
				if (edgeChildObj)
				{
					bEdgeHasChild = true;

					if (edgeChildObj->IsSelected())
					{
						bEdgeHasSelectedChild = true;
					}
				}
			}

			bOutIsConnected = (graphEdge->ConnectedFaces.Num() > 0);
			EdgeConnectedToValidPlane(graphEdge, doc, bConnectedToEmptyPlane, bConnectedToSelectedPlane);

			// Edges with GroupIDs should be hidden outside of meta-plane view mode
			// TODO: add more sophistication to this, based on how GroupIDs are used besides Roof Perimeters
			if (!bEnabledByViewMode && (graphEdge->GroupIDs.Num() > 0))
			{
				bOutShouldBeVisible = false;
				bOutShouldCollisionBeEnabled = false;
			}
			else
			{
				bOutShouldBeVisible = (bConnectedToSelectedPlane || bEdgeHasSelectedChild ||
					bConnectedToEmptyPlane || bEnabledByViewMode || MetaMOI->IsSelected() ||
					(!bOutIsConnected && !bEdgeHasChild));
				bOutShouldCollisionBeEnabled = bOutShouldBeVisible || bInMetaToolMode;
			}
			break;
		}
		case EObjectType::OTMetaPlane:
		{
			auto *graphFace = volumeGraph.FindFace(objID);

			// TODO: this should only be the case when the object is about to be deleted, there should be a flag for that.
			if (graphFace == nullptr)
			{
				bOutShouldBeVisible = false;
				bOutShouldCollisionBeEnabled = false;
				break;
			}

			bOutIsConnected = true;
			bool bPlaneHasChildren = (MetaMOI->GetChildIDs().Num() > 0);

			bOutShouldBeVisible = (bEnabledByViewMode || !bPlaneHasChildren);
			bOutShouldCollisionBeEnabled = bOutShouldBeVisible || bInMetaToolMode;
			break;
		}
		default:
			break;
		}
	}
}

void UModumateObjectStatics::GetGraphIDsFromMOIs(const TArray<Modumate::FModumateObjectInstance *> &MOIs, TSet<FTypedGraphObjID> &OutGraphObjIDs)
{
	OutGraphObjIDs.Reset();

	for (FModumateObjectInstance *obj : MOIs)
	{
		EObjectType objectType = obj ? obj->GetObjectType() : EObjectType::OTNone;
		EGraph3DObjectType graphObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);
		int32 objectID = obj ? obj->ID : MOD_ID_NONE;

		if (graphObjType != EGraph3DObjectType::None)
		{
			OutGraphObjIDs.Add(FTypedGraphObjID(objectID, graphObjType));
		}
	}
}

void UModumateObjectStatics::GetPlaneHostedValues(const Modumate::FModumateObjectInstance *PlaneHostedObj, float &OutThickness, float &OutStartOffset, FVector &OutNormal)
{
	OutThickness = PlaneHostedObj->CalculateThickness();
	float offsetPCT = PlaneHostedObj->GetExtents().X;
	OutStartOffset = -offsetPCT * OutThickness;
	OutNormal = PlaneHostedObj->GetNormal();
}

void UModumateObjectStatics::GetExtrusionDeltas(const FModumateObjectInstance *PlaneHostedObj, FVector &OutStartDelta, FVector &OutEndDelta)
{
	float thickness, startOffset;
	FVector normal;
	GetPlaneHostedValues(PlaneHostedObj, thickness, startOffset, normal);

	OutStartDelta = startOffset * normal;
	OutEndDelta = (startOffset + thickness) * normal;
}

bool UModumateObjectStatics::GetMountedTransform(const FVector &MountOrigin, const FVector &MountNormal,
	const FVector &LocalDesiredNormal, const FVector &LocalDesiredTangent, FTransform &OutTransform)
{
	OutTransform.SetIdentity();

	// If the mounting normal isn't vertical, then align the tangent with world Z+
	FVector mountAxisY(0.0f, 1.0f, 0.0f);
	if (!FVector::Parallel(MountNormal, FVector::UpVector))
	{
		FVector mountAxisX = (MountNormal ^ FVector::UpVector).GetSafeNormal();
		mountAxisY = mountAxisX ^ MountNormal;
	}

	if (!ensure(!FVector::Parallel(mountAxisY, MountNormal) &&
		!FVector::Parallel(LocalDesiredTangent, LocalDesiredNormal)))
	{
		return false;
	}

	// Make the rotations that represents the mount, the local desired rotation,
	// and then the actual desired mount rotation.
	FQuat mountRot = FRotationMatrix::MakeFromYZ(mountAxisY, MountNormal).ToQuat();
	FQuat localRot = FRotationMatrix::MakeFromYZ(LocalDesiredTangent, LocalDesiredNormal).ToQuat();
	FQuat worldRot = mountRot * localRot;

	OutTransform.SetLocation(MountOrigin);
	OutTransform.SetRotation(worldRot);

	return true;
}

bool UModumateObjectStatics::GetFFEMountedTransform(
	const FBIMAssemblySpec *Assembly, const FSnappedCursor &SnappedCursor, FTransform &OutTransform)
{
	FVector localDesiredNormal, localDesiredTangent;
	if (!SnappedCursor.HitNormal.IsNearlyZero() && ensure(Assembly &&
		Assembly->CachedAssembly.TryGetProperty(BIMPropertyNames::Normal, localDesiredNormal) &&
		Assembly->CachedAssembly.TryGetProperty(BIMPropertyNames::Tangent, localDesiredTangent)))
	{
		return GetMountedTransform(SnappedCursor.WorldPosition, SnappedCursor.HitNormal,
			localDesiredNormal, localDesiredTangent, OutTransform);
	}

	return false;
}

bool UModumateObjectStatics::GetFFEBoxSidePoints(AActor *Actor, const FVector &AssemblyNormal, TArray<FVector> &OutPoints)
{
	OutPoints.Reset();

	if ((Actor == nullptr) || !AssemblyNormal.IsNormalized())
	{
		return false;
	}

	// Copied from FMOIObjectImpl::GetStructuralPointsAndLines, so that we can supply
	// an override vector for the bounding box points, to get only the side that corresponds
	// to the FF&E's mounting normal vector.
	FVector actorOrigin, actorBoxExtent;
	Actor->GetActorBounds(false, actorOrigin, actorBoxExtent);
	FQuat actorRot = Actor->GetActorQuat();
	FVector actorLoc = Actor->GetActorLocation();

	// This calculates the extent more accurately since it's unaffected by actor rotation
	actorBoxExtent = Actor->CalculateComponentsBoundingBoxInLocalSpace(true).GetSize() * 0.5f;

	// Supply the opposite of the FF&E's mounting normal vector, to get the "bottom" points of the bounding box.
	TArray<FStructurePoint> structurePoints;
	TArray<FStructureLine> structureLines;
	FModumateSnappingView::GetBoundingBoxPointsAndLines(actorOrigin, actorRot, actorBoxExtent,
		structurePoints, structureLines, -AssemblyNormal);

	if (structurePoints.Num() != 4)
	{
		return false;
	}

	for (const FStructurePoint &point : structurePoints)
	{
		OutPoints.Add(point.Point);
	}

	return true;
}
