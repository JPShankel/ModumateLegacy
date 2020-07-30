// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateFunctionLibrary.h"

#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DocumentManagement/ModumateObjectInstancePortal.h"
#include "DrawDebugHelpers.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/UserInterfaceSettings.h"
#include "EngineGlobals.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "KismetProceduralMeshLibrary.h"
#include "UnrealClasses/Modumate.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Runtime/Core/Public/Math/RandomStream.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Algo/Accumulate.h"

#include <algorithm>
#include <queue>
#include <iostream>
#include <regex>

using namespace std;
#include "../../poly2tri/poly2tri.h"
using namespace p2t;



using namespace Modumate;

FVector UModumateFunctionLibrary::V3dToV2d(TArray<FVector2D> & OutV2d, TArray<FVector> InV3d)
{
	TArray<FVector2D> V;

	if (InV3d.Num() >= 3)
	{
		FVector VectorNormalize = (FVector::CrossProduct((InV3d[1] - InV3d[0]), (InV3d[2] - InV3d[1]))).GetSafeNormal();
		FRotator VectorRot = UKismetMathLibrary::MakeRotFromZ(VectorNormalize);

		for (FVector& CurV3d : InV3d)
		{
			FVector TempVector = VectorRot.UnrotateVector(CurV3d - InV3d[0]);
			V.Add(FVector2D(TempVector.X, TempVector.Y));
		}

		OutV2d = V;
		return VectorNormalize;
	}
	else
	{
		for (FVector& CurV3d : InV3d)
		{
			V.Add(FVector2D(CurV3d.X, CurV3d.Y));
		}
		OutV2d = V;
		return FVector::ZeroVector;
	}
}

bool UModumateFunctionLibrary::IsWithEditor()
{
	return WITH_EDITOR;
}

bool UModumateFunctionLibrary::PointIsOnSegment(const FVector2D &p, const FVector2D &s1, const FVector2D &s2, float tolerance)
{
	float plen = FVector2D::Distance(p, s1) + FVector2D::Distance(p, s2);
	return FMath::IsNearlyEqual(FVector2D::Distance(s1, s2), plen, tolerance);
}

FVector2D UModumateFunctionLibrary::ProjectPointToLine2D(const FVector2D &lineP1, const FVector2D &lineP2, const FVector2D &inPoint)
{
	FVector2D dl = (lineP2 - lineP1).GetSafeNormal();
	FVector2D dp = (inPoint - lineP1);
	return lineP1 + FVector2D::DotProduct(dl, dp) * dl;
}

FVector UModumateFunctionLibrary::ProjectPointToLine(const FVector &lineP1, const FVector &lineP2, const FVector &inPoint)
{
	FVector dl = (lineP2 - lineP1).GetSafeNormal();
	FVector dp = (inPoint - lineP1);
	return lineP1 + FVector::DotProduct(dl, dp) * dl;
}

bool UModumateFunctionLibrary::PointInPoly2D(const FVector2D &p,const TArray<FVector2D> &poly)
{
	float angle = 0;

	for (int32 i = 0; i < poly.Num(); ++i)
	{
		FVector2D d1 = (poly[i] - p).GetSafeNormal();
		FVector2D d2 = (poly[(i + 1) % poly.Num()] - p).GetSafeNormal();

		if (d1.X * d2.Y - d1.Y * d2.X > 0)
		{
			angle += FMath::Acos(FMath::Clamp(FVector2D::DotProduct(d1, d2), -1.0f, 1.0f));
		}
		else
		{
			angle -= FMath::Acos(FMath::Clamp(FVector2D::DotProduct(d1, d2), -1.0f, 1.0f));
		}
	}
	return FMath::Abs(angle) > 2 * PI - 0.01f;
}


bool UModumateFunctionLibrary::LinesIntersect2D(const FVector2D &p1, const FVector2D &p2, const FVector2D &p3, const FVector2D &p4, FVector2D &intersection)
{
	FMatrix2x2 w1m(p1.X, p1.Y, p2.X, p2.Y);
	FMatrix2x2 w2m(p3.X, p3.Y, p4.X, p4.Y);

	FMatrix2x2 w1mx(p1.X, 1, p2.X, 1);
	FMatrix2x2 w2mx(p3.X, 1, p4.X, 1);
	FMatrix2x2 w1my(p1.Y, 1, p2.Y, 1);
	FMatrix2x2 w2my(p3.Y, 1, p4.Y, 1);

	float w1md = w1m.Determinant();
	float w1mxd = w1mx.Determinant();
	float w1myd = w1my.Determinant();

	float w2md = w2m.Determinant();
	float w2mxd = w2mx.Determinant();
	float w2myd = w2my.Determinant();

	FMatrix2x2 pxn(w1md, w1mxd, w2md, w2mxd);
	FMatrix2x2 pxd(w1mxd, w1myd, w2mxd, w2myd);

	FMatrix2x2 pyn(w1md, w1myd, w2md, w2myd);
	FMatrix2x2 pyd(w1mxd, w1myd, w2mxd, w2myd);

	float fpxn = pxn.Determinant();
	float fpxd = pxd.Determinant();
	float fpyn = pyn.Determinant();
	float fpyd = pyd.Determinant();

	if (FMath::IsNearlyEqual(fpxd, 0.0f) || FMath::IsNearlyEqual(fpyd, 0.0f))
	{
		return false;
	}

	intersection = FVector2D(fpxn / fpxd, fpyn / fpyd);

	return PointIsOnSegment(intersection, p1, p2) && PointIsOnSegment(intersection, p3, p4);
}

TArray<FVector2D> UModumateFunctionLibrary::GetPolygonUV(TArray<FVector2D> Verts, FVector2D Size)
{
	TArray<float> X;
	TArray<float> Y;
	TArray<FVector2D> UVs;

	for (FVector2D& curVectorLoc : Verts)
	{
		X.Add(curVectorLoc.X);
		Y.Add(curVectorLoc.Y);
	}

	for (FVector2D& curVectorUV : Verts)
	{
		UVs.Add(curVectorUV / Size);
	}

	return UVs;
}

TArray<FVector2D> UModumateFunctionLibrary::GetPolygonUVWith3DXY(FVector Origin, TArray<FVector> Verts, FVector2D Size, FRotator UvRotation)
{
	TArray<FVector2D> returnUV;
	for (int32 i = 0; i < Verts.Num(); i++)
	{
		FVector newUV3D = UvRotation.RotateVector(FVector((Verts[i] + Origin).X, (Verts[i] + Origin).Y, 0.f));
		returnUV.Add(FVector2D(newUV3D.X, newUV3D.Y) / Size);
	}
	return returnUV;
}

TArray<int32> UModumateFunctionLibrary::IntegerArrayReplaceHighest(TArray<int32> ExistingInt, TArray<int32> NewInt)
{
	int32 maxExistingInt = FMath::Max(ExistingInt);
	TArray<int32> resultInt;
	for (int32& curInt : NewInt)
	{
		resultInt.Add(curInt + (maxExistingInt + 1));
	}

	return resultInt;
}

FVector UModumateFunctionLibrary::GetClosestLocationToTargetFromArray(FVector Target, TArray<FVector> Array, int32 & ClosestIndex, int32 & FarestIndex)
{
	TArray<float> distances;

	for (FVector& curLocation : Array)
	{
		distances.Add((curLocation - Target).Size());
	}

	int32 minIndex = 0, maxIndex = 0;
	float minValue = 0, maxValue = 0;
	UKismetMathLibrary::MinOfFloatArray(distances, minIndex, minValue);
	UKismetMathLibrary::MaxOfFloatArray(distances, maxIndex, maxValue);

	ClosestIndex = minIndex;
	FarestIndex = maxIndex;

	return Array[minIndex];
}

int32 UModumateFunctionLibrary::LoopArrayGetNextIndex(int32 CurrentIndex, int32 LengthOfArray)
{
	int32 result;
	(CurrentIndex + 1) > (LengthOfArray - 1) ? result = 0 : result = CurrentIndex + 1;
	return result;
}

int32 UModumateFunctionLibrary::LoopArrayGetPreviousIndex(int32 CurrentIndex, int32 LengthOfArray)
{
	int32 result;
	(CurrentIndex - 1) < 0 ? result = LengthOfArray - 1 : result = CurrentIndex - 1;
	return result;
}

bool UModumateFunctionLibrary::IsVectorInArray(const TArray<FVector>& Array, const FVector& TargetVector, float Tolerance)
{
	bool bResult = false;
	for (auto curVector : Array)
	{
		if (curVector.Equals(TargetVector, Tolerance))
		{
			bResult = true;
			break;
		}
	}
	return bResult;
}

TArray<int32> UModumateFunctionLibrary::ConformTriangleClockwise(TArray<FVector> Vertices, TArray<int32> Triangles)
{
	int32 loopLastIndex = (Triangles.Num() / 3) - 1;
	int32 tri1 = 0, tri2 = 0, tri3 = 0;
	FVector p1, p2, p3, norm1, norm2;
	bool bConform = false;
	TArray<int32> outTriangles;

	for (int32 i = 0; i < loopLastIndex + 1; i++)
	{
		tri1 = Triangles[i * 3];
		tri2 = Triangles[(i * 3) + 1];
		tri3 = Triangles[(i * 3) + 2];

		p1 = Vertices[tri1];
		p2 = Vertices[tri2];
		p3 = Vertices[tri3];

		norm1 = (p3 - p1).GetSafeNormal();
		norm2 = (p2 - p1).GetSafeNormal();
		bConform = (FVector::CrossProduct(norm1, norm2).GetSafeNormal()).Equals(FVector(0.0, 0.0, 1.0), 0.1);

		if (bConform)
		{
			outTriangles.Add(tri1);
			outTriangles.Add(tri2);
			outTriangles.Add(tri3);
		}
		else
		{
			outTriangles.Add(tri3);
			outTriangles.Add(tri2);
			outTriangles.Add(tri1);
		}
	}
	return outTriangles;
}

TArray<FVector2D> UModumateFunctionLibrary::FloorUVCalculateWithAnchor(TArray<FVector> faceVertsRelative, FVector anchorRelative, FVector faceDirection, FVector horizontalDirection)
{
	TArray<FVector> verts3D;
	TArray<FVector2D> vertsUVs;
	if ((faceDirection.Equals(FVector(0.f, 0.f, 1.f), 0.1f)) || (faceDirection.Equals(FVector(0.f, 0.f, -1.f), 0.1f)))
	{
		FRotator faceRot = UKismetMathLibrary::MakeRotFromX(horizontalDirection);
		for (int32 i = 0; i < faceVertsRelative.Num(); i++)
		{
			verts3D.Add(UKismetMathLibrary::LessLess_VectorRotator(faceVertsRelative[i], faceRot));
		}
		FVector unRotatedAnchor = UKismetMathLibrary::LessLess_VectorRotator(anchorRelative, faceRot);
		for (int32 i = 0; i < verts3D.Num(); i++)
		{
			FVector unRotatedVert = verts3D[i] + unRotatedAnchor;
			vertsUVs.Add(FVector2D(unRotatedVert.X * 0.01f, unRotatedVert.Y * 0.01f));
		}
	}
	else
	{
		FRotator faceRot = UKismetMathLibrary::MakeRotFromX(faceDirection);
		for (int32 i = 0; i < faceVertsRelative.Num(); i++)
		{
			verts3D.Add(UKismetMathLibrary::LessLess_VectorRotator(faceVertsRelative[i], faceRot));
		}
		FVector unRotatedAnchor = UKismetMathLibrary::LessLess_VectorRotator(anchorRelative, faceRot);
		for (int32 i = 0; i < verts3D.Num(); i++)
		{
			FVector unRotatedVert = verts3D[i] - unRotatedAnchor;
			vertsUVs.Add(FVector2D(unRotatedVert.Y * -0.01f, unRotatedVert.Z * -0.01f));
		}
	}
	return vertsUVs;
}

void UModumateFunctionLibrary::CalculateFloorParam(FVector Origin, TArray<FVector> anchorUV, FVector topAnchorUV, FRotator topUvRotation, TArray<FVector> InTopVerts, TArray<FVector> InBottomVerts, TArray<FVector>& ReturnVerts, TArray<int32>& ReturnTris, TArray<FVector>& ReturnNormals, TArray<FVector2D>& ReturnUV, TArray<int32>& ReturnTopTris)
{
	float bottomHeight = InBottomVerts[0].Z;
	TArray<FVector> topVerts, topVertsOut, topNormals, sideVertsPending, sideVerts, sideNormals, bottomVerts, bottomNormals, inAnchorUV;
	TArray<int32> topTris, conflictID, intersectID, sideTris, bottomTris;
	TArray<FVector2D> topUVs, verts2D, sideUVsPending, sideUVs, bottomUVs;
	FVector topVert1(ForceInitToZero);
	FVector topVert2(ForceInitToZero);
	FVector sideFaceDirection(ForceInitToZero);
	TArray<int32> squareTris = { 3, 0, 1, 3, 1, 2 };
	ReturnVerts.Reset();
	ReturnTris.Reset();
	ReturnNormals.Reset();
	ReturnUV.Reset();

	FPlane plane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(InTopVerts, plane))
	{
		return;
	}
	if (!FVector::Parallel(plane, FVector::UpVector))
	{
		return;
	}

	topVerts = InTopVerts;
	inAnchorUV = anchorUV;
	if (!FVector::Coincident(plane, FVector::UpVector))
	{
		Algo::Reverse(topVerts);
		Algo::Reverse(inAnchorUV);
	}
	// top face
	TArray<FVector2D> topVerts2D, topVerts2DOut, outPerimeter;
	TArray<FPolyHole2D> holes2D;
	TArray<bool> outMergedHoles;
	TArray<int32> outPerimeterVertexHoleIndices;
	for (FVector& curVert : topVerts)
	{
		topVerts2D.Add(FVector2D(curVert));
	}
	UModumateGeometryStatics::TriangulateVerticesPoly2Tri(topVerts2D, holes2D, topVerts2DOut, topTris, outPerimeter, outMergedHoles, outPerimeterVertexHoleIndices);
	for (int32 i = 0; i < topVerts2DOut.Num(); i++)
	{
		topVertsOut.Add(FVector(topVerts2DOut[i], topVerts[i].Z));
	}
	topTris = ConformTriangleClockwise(topVertsOut, topTris);
	V3dToV2d(verts2D, topVertsOut);
	topUVs = GetPolygonUVWith3DXY(Origin - topAnchorUV, topVertsOut, FVector2D(100.0, 100.0), topUvRotation);
	for (int32 i = 0; i < topVertsOut.Num(); i++)
	{
		topNormals.Add(FVector(0.0, 0.0, 1.0));
	}
	// side faces
	for (int32 i = 0; i < topVerts.Num(); i++)
	{
		topVert1 = topVerts[i];
		topVert2 = topVerts[LoopArrayGetNextIndex(i, topVerts.Num())];
		sideVertsPending = { topVert1, topVert2, FVector(topVert2.X, topVert2.Y, bottomHeight), FVector(topVert1.X, topVert1.Y, bottomHeight) };

		sideVerts.Append(sideVertsPending);
		if (i == 0)
		{
			sideTris = squareTris;
		}
		else
		{
			sideTris.Append(IntegerArrayReplaceHighest(sideTris, squareTris));
		}
		sideFaceDirection = UKismetMathLibrary::RotateAngleAxis(UKismetMathLibrary::GetDirectionUnitVector(topVert1, topVert2), -90.0, FVector(0.0, 0.0, 1.0));
		for (int32 j = 0; j < 4; j++)
		{
			sideNormals.Add(sideFaceDirection);
		}

		// check if sideUV contain anchor
		if (anchorUV.Num() > 0)
		{
			int32 curIndex = LoopArrayGetNextIndex(i, topVerts.Num());
			FVector UVAnchorRelative = inAnchorUV[curIndex] + Origin;

			TArray<FVector> sideVertsPendingUnOrigin;
			for (int32 j = 0; j < sideVertsPending.Num(); j++)
			{
				sideVertsPendingUnOrigin.Add(sideVertsPending[j] + Origin);
			}
			sideUVsPending = FloorUVCalculateWithAnchor(sideVertsPendingUnOrigin, UVAnchorRelative, sideFaceDirection, sideFaceDirection);
			sideUVs.Append(sideUVsPending);
		}
		else
		{
			V3dToV2d(verts2D, sideVertsPending);
			sideUVsPending = GetPolygonUV(verts2D, FVector2D(100.0, 100.0));
			sideUVs.Append(sideUVsPending);
		}
	}
	// bottom faces
	for (FVector& curVector : topVertsOut)
	{
		bottomVerts.Add(FVector(curVector.X, curVector.Y, bottomHeight));
		bottomNormals.Add(FVector(0.0, 0.0, -1.0));
	}
	bottomTris = topTris;
	Algo::Reverse(bottomTris);
	bottomUVs = topUVs;

	// Append all procedural mesh faces parameters
	ReturnVerts.Append(topVertsOut);
	ReturnVerts.Append(sideVerts);
	ReturnVerts.Append(bottomVerts);

	ReturnTris.Append(topTris);
	ReturnTris.Append(IntegerArrayReplaceHighest(ReturnTris, sideTris));
	ReturnTris.Append(IntegerArrayReplaceHighest(ReturnTris, bottomTris));

	ReturnUV.Append(topUVs);
	ReturnUV.Append(sideUVs);
	ReturnUV.Append(bottomUVs);

	ReturnNormals.Append(topNormals);
	ReturnNormals.Append(sideNormals);
	ReturnNormals.Append(bottomNormals);

	ReturnTopTris = topTris;
}

void UModumateFunctionLibrary::GetFloorAssemblyLayerControlPoints(const TArray<FVector>& points, const TArray<FModumateObjectAssemblyLayer>& fal, TArray<FFloorAssemblyLayerControlPoints> &OutLayersCPs, bool bLayersReversed, bool bManualLayerCPs)
{
	OutLayersCPs.Reset();
	FFloorAssemblyLayerControlPoints curLayerCPs;

	FVector extrusionDir = FVector::UpVector;
	float curTotalThickness = 0.0f;
	int32 startIndex = bLayersReversed ? (fal.Num() - 1) : 0;
	int32 di = bLayersReversed ? -1 : 1;
	for (int32 i = startIndex; bLayersReversed ? (i >= 0) : (i < fal.Num()); i += di)
	{
		float curLayerThickness = fal[i].Thickness.AsWorldCentimeters();
		curLayerCPs.BottomLayerControlPoints.Reset(points.Num());
		curLayerCPs.TopLayerControlPoints.Reset(points.Num());

		float curLayerOffset = bManualLayerCPs ? 0.0f : curTotalThickness;
		FVector bottomOffset = curLayerOffset * extrusionDir;
		FVector topOffset = (curLayerOffset + curLayerThickness) * extrusionDir;

		for (const FVector& point : points)
		{
			curLayerCPs.BottomLayerControlPoints.Add(point + bottomOffset);
			curLayerCPs.TopLayerControlPoints.Add(point + topOffset);
		}

		OutLayersCPs.Add(curLayerCPs);
		curTotalThickness += curLayerThickness;
	}
}

FVector UModumateFunctionLibrary::GetWorldComponentToScreenSizeScale(UStaticMeshComponent * Target, FVector DesiredScreenSize)
{
	APlayerCameraManager* cameraActor = UGameplayStatics::GetPlayerCameraManager(Target, 0);
	float fovD = UKismetMathLibrary::DegSin(cameraActor->GetFOVAngle());
	float distance = FVector::Dist(cameraActor->GetCameraLocation(), Target->GetComponentLocation());
	float relativeScreenSize = FMath::Clamp(50.0f / (distance * fovD), 0.0f, 1.0f);

	float rFOV = FMath::DegreesToRadians(cameraActor->GetFOVAngle());
	float fovScale = 2 * FMath::Tan(rFOV / 2.f);

	return (DesiredScreenSize / relativeScreenSize) * fovScale;
}

void UModumateFunctionLibrary::ComponentAsBillboard(UStaticMeshComponent * Target, FVector DesiredScreenSize)
{
	Target->SetWorldScale3D(GetWorldComponentToScreenSizeScale(Target, DesiredScreenSize));
	FRotator camRot = UGameplayStatics::GetPlayerCameraManager(Target, 0)->GetCameraRotation();
	FTransform transformRot = FTransform(camRot, FVector::ZeroVector, FVector::OneVector);
	FRotator newRot = UKismetMathLibrary::TransformRotation(transformRot, FRotator(90.0, 0.0, 0.0));
	Target->SetWorldRotation(newRot);
}

void UModumateFunctionLibrary::ComponentToUIScreenPosition(UStaticMeshComponent * Target, FVector WorldLocation, FVector ScreenOffset)
{
	APlayerController* playerController = UGameplayStatics::GetPlayerController(Target, 0);
	FVector2D screenLocation;
	playerController->ProjectWorldLocationToScreen(WorldLocation, screenLocation, false);

	screenLocation = screenLocation + FVector2D(ScreenOffset.X, ScreenOffset.Y * -1);
	FVector convertWorldLocation, convertWorldDirection;
	playerController->DeprojectScreenPositionToWorld(screenLocation.X, screenLocation.Y, convertWorldLocation, convertWorldDirection);
	float distanceFromCamera = 1.0f;
	FVector newLocation = convertWorldLocation + convertWorldDirection * distanceFromCamera;
	Target->SetWorldLocation(newLocation, false, 0, ETeleportType::None);
}

void UModumateFunctionLibrary::ComponentToUIScreenPositionMoiOffset(UStaticMeshComponent * Target, FVector WorldLocation, FVector Direction, float OffsetScreenDistance, FVector ScreenOffset)
{
	//const float offsetScreenDistance = 12.0f;
	APlayerController* playerController = UGameplayStatics::GetPlayerController(Target, 0);
	APlayerCameraManager* playerCamera = UGameplayStatics::GetPlayerCameraManager(Target, 0);

	// set min size for projection
	float screenMinSize = (playerCamera->GetCameraLocation() - WorldLocation).Size();

	FVector2D from2D, to2D;
	playerController->ProjectWorldLocationToScreen(WorldLocation, from2D, false);
	playerController->ProjectWorldLocationToScreen(WorldLocation + (Direction * screenMinSize), to2D, false);
	FVector screenVector = UKismetMathLibrary::GetDirectionUnitVector(FVector(from2D.X, from2D.Y, 0.0f), FVector(to2D.X, to2D.Y, 0.0f));
	FVector offsetScreenSpace = (screenVector * OffsetScreenDistance) + FVector(from2D.X, from2D.Y, 0.0f);

	FVector convertLocationStart, convertDirection;
	playerController->DeprojectScreenPositionToWorld(offsetScreenSpace.X, offsetScreenSpace.Y, convertLocationStart, convertDirection);

	FVector convertLocationEnd = convertLocationStart + (convertDirection * screenMinSize * 2.0f);
	FVector planeNormal = UKismetMathLibrary::Conv_RotatorToVector(playerCamera->GetCameraRotation());

	float interectionT;
	FVector intersectionLocation;
	UKismetMathLibrary::LinePlaneIntersection_OriginNormal(convertLocationStart, convertLocationEnd, WorldLocation, planeNormal, interectionT, intersectionLocation);

	ComponentToUIScreenPosition(Target, intersectionLocation, ScreenOffset);
}


void UModumateFunctionLibrary::ScreenSizeComponentFreezeDirection(UStaticMeshComponent * Target, FVector WorldDirection)
{
	APlayerController* playerController = UGameplayStatics::GetPlayerController(Target, 0);
	FVector2D start2D, end2D;
	FRotator lookAtRotation;
	if (WorldDirection.Equals(FVector(0.0, 0.0, 1.0), 0.01))
	{
		//lookAtRotation = FRotator(0.0f, 90.0f, 0.0f);
		playerController->ProjectWorldLocationToScreen(Target->GetComponentLocation(), start2D, false);
		playerController->ProjectWorldLocationToScreen(Target->GetComponentLocation() + (WorldDirection * 1.0), end2D, false);
		lookAtRotation = UKismetMathLibrary::FindLookAtRotation(FVector(end2D.X, end2D.Y, 0.0f), FVector(start2D.X, start2D.Y, 0.0f));
	}
	else if (WorldDirection.Equals(FVector(0.0, 0.0, -1.0), 0.01))
	{
		//lookAtRotation = FRotator(0.0f, -90.0f, 0.0f);
		playerController->ProjectWorldLocationToScreen(Target->GetComponentLocation(), start2D, false);
		playerController->ProjectWorldLocationToScreen(Target->GetComponentLocation() + (WorldDirection * 1.0), end2D, false);
		lookAtRotation = UKismetMathLibrary::FindLookAtRotation(FVector(end2D.X, end2D.Y, 0.0f), FVector(start2D.X, start2D.Y, 0.0f));
	}
	else
	{
		playerController->ProjectWorldLocationToScreen(Target->GetComponentLocation(), start2D, false);
		playerController->ProjectWorldLocationToScreen(Target->GetComponentLocation() + (WorldDirection * 5.0), end2D, false);
		lookAtRotation = UKismetMathLibrary::FindLookAtRotation(FVector(end2D.X, end2D.Y, 0.0f), FVector(start2D.X, start2D.Y, 0.0f));
	}

	FRotator makeRot = FRotator(0.0f, lookAtRotation.Yaw, 0.0f);
	FRotator newRot = UKismetMathLibrary::TransformRotation(Target->GetComponentTransform(), makeRot);
	Target->SetWorldRotation(newRot, false, 0, ETeleportType::None);
}

bool UModumateFunctionLibrary::MoiPortalSetNewWidth(AActor* targetActor, float newWidth)
{
	AEditModelGameState_CPP * gameState = targetActor->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	AEditModelPlayerController_CPP *controller = Cast<AEditModelPlayerController_CPP>(targetActor->GetWorld()->GetFirstPlayerController());
	FModumateObjectInstance *moi = gameState->Document.ObjectFromActor(targetActor);
	if (moi == nullptr)
	{
		return false;
	}
	TArray<FVector> proxyCPs = moi->GetControlPoints();
	float midPoint = (proxyCPs[0].X + proxyCPs[3].X) / 2.f;
	if (moi->GetObjectType() == EObjectType::OTDoor || moi->GetObjectType() == EObjectType::OTWindow)
	{
		proxyCPs[0].X = midPoint - newWidth * 0.5f;
		proxyCPs[1].X = midPoint - newWidth * 0.5f;
		proxyCPs[2].X = midPoint + newWidth * 0.5f;
		proxyCPs[3].X = midPoint + newWidth * 0.5f;

		// Replace the one above if want to keep one side of CP
		//proxyCPs[2].Y = proxyCPs[1].Y - newWidth;
		//proxyCPs[3].Y = proxyCPs[0].Y - newWidth;

		controller->ModumateCommand(
			FModumateCommand(Commands::kUpdateMOIHoleParams)
			.Param(Parameters::kObjectID, moi->ID)
			.Param(Parameters::kLocation, moi->GetObjectLocation())
			.Param(Parameters::kControlPoints, proxyCPs));

		return true;
	}
	return false; // Only set width of doors and windows
}

bool UModumateFunctionLibrary::MoiPortalSetNewHeight(AActor* targetActor, float newHeight)
{
	AEditModelGameState_CPP * gameState = targetActor->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	AEditModelPlayerController_CPP *controller = Cast<AEditModelPlayerController_CPP>(targetActor->GetWorld()->GetFirstPlayerController());
	FModumateObjectInstance *moi = gameState->Document.ObjectFromActor(targetActor);
	if (moi == nullptr)
	{
		return false;
	}
	TArray<FVector> proxyCPs = moi->GetControlPoints();
	if ((moi->GetObjectType() == EObjectType::OTDoor) || (moi->GetObjectType() == EObjectType::OTWindow))
	{
		proxyCPs[1].Z = proxyCPs[0].Z + newHeight;
		proxyCPs[2].Z = proxyCPs[3].Z + newHeight;
	}
	else
	{
		return false;
	}
	controller->ModumateCommand(
		FModumateCommand(Commands::kUpdateMOIHoleParams)
		.Param(Parameters::kObjectID, moi->ID)
		.Param(Parameters::kLocation, moi->GetObjectLocation())
		.Param(Parameters::kControlPoints, proxyCPs));

	return true;
}

float SignCheckIfPointInTri(FVector p1, FVector p2, FVector p3)
{
	return (p1.X - p3.X) * (p2.Y - p3.Y) - (p2.X - p3.X) * (p1.Y - p3.Y);
}

bool UModumateFunctionLibrary::IsPointInTri(FVector pt, FVector v1, FVector v2, FVector v3)
{
	bool b1, b2, b3;

	b1 = SignCheckIfPointInTri(pt, v1, v2) < 0.0f;
	b2 = SignCheckIfPointInTri(pt, v2, v3) < 0.0f;
	b3 = SignCheckIfPointInTri(pt, v3, v1) < 0.0f;

	return ((b1 == b2) && (b2 == b3));
}

// Reimplemented in 2D based on FMath::LineBoxIntersection in 3D
bool UModumateFunctionLibrary::LineBoxIntersection(const FBox2D &box, const FVector2D &start, const FVector2D &end)
{
	if (start.Equals(end))
	{
		return box.IsInside(start);
	}

	FVector2D time;
	FVector2D startToEnd = (end - start);
	FVector2D oneOverStartToEnd(1.0f / startToEnd.X, 1.0f / startToEnd.Y);
	bool bStartIsOutside = false;

	if (start.X < box.Min.X)
	{
		bStartIsOutside = true;
		if (end.X >= box.Min.X)
		{
			time.X = (box.Min.X - start.X) * oneOverStartToEnd.X;
		}
		else
		{
			return false;
		}
	}
	else if (start.X > box.Max.X)
	{
		bStartIsOutside = true;
		if (end.X <= box.Max.X)
		{
			time.X = (box.Max.X - start.X) * oneOverStartToEnd.X;
		}
		else
		{
			return false;
		}
	}
	else
	{
		time.X = 0.0f;
	}

	if (start.Y < box.Min.Y)
	{
		bStartIsOutside = true;
		if (end.Y >= box.Min.Y)
		{
			time.Y = (box.Min.Y - start.Y) * oneOverStartToEnd.Y;
		}
		else
		{
			return false;
		}
	}
	else if (start.Y > box.Max.Y)
	{
		bStartIsOutside = true;
		if (end.Y <= box.Max.Y)
		{
			time.Y = (box.Max.Y - start.Y) * oneOverStartToEnd.Y;
		}
		else
		{
			return false;
		}
	}
	else
	{
		time.Y = 0.0f;
	}

	if (bStartIsOutside)
	{
		const float	maxTime = time.GetMax();

		if (maxTime >= 0.0f && maxTime <= 1.0f)
		{
			const FVector2D hit = start + startToEnd * maxTime;
			const float BOX_SIDE_THRESHOLD = 0.1f;
			if (hit.X > box.Min.X - BOX_SIDE_THRESHOLD && hit.X < box.Max.X + BOX_SIDE_THRESHOLD &&
				hit.Y > box.Min.Y - BOX_SIDE_THRESHOLD && hit.Y < box.Max.Y + BOX_SIDE_THRESHOLD)
			{
				return true;
			}
		}

		return false;
	}
	else
	{
		return true;
	}
}

void UModumateFunctionLibrary::ScreenLineExtend(FVector2D & ReturnStart, FVector2D & ReturnEnd, FVector2D Start, FVector2D End, float StartExtendDistance, float EndExtendDistance)
{
	FVector start3D = FVector(Start.X, Start.Y, 0.0f);
	FVector end3D = FVector(End.X, End.Y, 0.0f);

	FVector startVector = (UKismetMathLibrary::GetDirectionUnitVector(start3D, end3D) * StartExtendDistance * -1.0f) + start3D;
	FVector endVector = (UKismetMathLibrary::GetDirectionUnitVector(start3D, end3D) * EndExtendDistance) + end3D;

	ReturnStart = FVector2D(startVector.X, startVector.Y);
	ReturnEnd = FVector2D(endVector.X, endVector.Y);
}



bool UModumateFunctionLibrary::ProjectWorldToScreenBidirectional(APlayerController const * Player, const FVector & WorldPosition, FVector2D & ScreenPosition, bool & bTargetBehindCamera, bool bPlayerViewportRelative)
{
	FVector Projected;
	bTargetBehindCamera = false;

	// Custom Projection Function
	ULocalPlayer* const LP = Player->GetLocalPlayer();
	if (LP && LP->ViewportClient)
	{
		FSceneViewProjectionData NewProjectionData;
		if (LP->GetProjectionData(LP->ViewportClient->Viewport, EStereoscopicPass::eSSP_FULL, NewProjectionData))
		{
			const FMatrix ViewProjectionMatrix = NewProjectionData.ComputeViewProjectionMatrix();
			const FIntRect ViewRectangle = NewProjectionData.GetConstrainedViewRect();

			FPlane Result = ViewProjectionMatrix.TransformFVector4(FVector4(WorldPosition, 1.f));
			// Small Result.W gives huge result
			if (Result.W < 0.f) { bTargetBehindCamera = true; }
			if (Result.W == 0.f) { Result.W = 1.f; } // Prevent Divide By Zero

			const float RHW = 1.f / FMath::Abs(Result.W);
			Projected = FVector(Result.X, Result.Y, Result.Z) * RHW;

			// Normalize to 0..1 UI Space
			const float NormX = (Projected.X / 2.f) + 0.5f;
			const float NormY = 1.f - (Projected.Y / 2.f) - 0.5f;

			Projected.X = (float)ViewRectangle.Min.X + (NormX * (float)ViewRectangle.Width());
			Projected.Y = (float)ViewRectangle.Min.Y + (NormY * (float)ViewRectangle.Height());
		}
	}
	ScreenPosition = FVector2D(Projected.X, Projected.Y);
	return true;
}

bool UModumateFunctionLibrary::ProjectLineToCameraPlane(APlayerController const* player, const FVector point1, const FVector point2, FVector& newPoint1, FVector& newPoint2)
{
	float intersectT = 0.f;
	FVector intersection;

	bool hasIntersect = UKismetMathLibrary::LinePlaneIntersection_OriginNormal(point1, point2,
		player->PlayerCameraManager->GetCameraLocation(),
		player->PlayerCameraManager->GetActorForwardVector(),
		intersectT, intersection);

	if (hasIntersect)
	{
		FVector2D screenPos;
		bool behindcamera = false;
		ProjectWorldToScreenBidirectional(player, point1, screenPos, behindcamera, false);
		if (behindcamera)
		{
			newPoint1 = intersection;
			newPoint2 = point2;
		}
		else
		{
			newPoint1 = point1;
			newPoint2 = intersection;
		}
		return false;
	}
	else
	{
		newPoint1 = point1;
		newPoint2 = point2;
		return true;
	}
}

FModumateSunPositionData UModumateFunctionLibrary::ModumateGetSunPosition(float Latitude, float Longitude, float TimeZone, bool bIsDaylightSavingTime, int32 Year, int32 Month, int32 Day, int32 Hours, int32 Minutes, int32 Seconds)
{
	FDateTime CalcTime(Year, Month, Day, Hours, Minutes, Seconds);

	float TimeOffset = TimeZone;
	if (bIsDaylightSavingTime)
	{
		TimeOffset += 1.0f;
	}

	double LatitudeRad = FMath::DegreesToRadians(Latitude);

	// Get the julian day (number of days since Jan 1st of the year 4713 BC)
	double JulianDay = CalcTime.GetJulianDay() + (CalcTime.GetTimeOfDay().GetTotalHours() - TimeOffset) / 24.0;
	double JulianCentury = (JulianDay - 2451545.0) / 36525.0;

	// Get the sun's mean longitude , referred to the mean equinox of julian date
	double GeomMeanLongSunDeg = FMath::Fmod(280.46646 + JulianCentury * (36000.76983 + JulianCentury * 0.0003032), 360.0f);
	double GeomMeanLongSunRad = FMath::DegreesToRadians(GeomMeanLongSunDeg);

	// Get the sun's mean anomaly
	double GeomMeanAnomSunDeg = 357.52911 + JulianCentury * (35999.05029 - 0.0001537*JulianCentury);
	double GeomMeanAnomSunRad = FMath::DegreesToRadians(GeomMeanAnomSunDeg);

	// Get the earth's orbit eccentricity
	double EccentEarthOrbit = 0.016708634 - JulianCentury * (0.000042037 + 0.0000001267*JulianCentury);

	// Get the sun's equation of the center
	double SunEqOfCtr = FMath::Sin(GeomMeanAnomSunRad)*(1.914602 - JulianCentury * (0.004817 + 0.000014*JulianCentury))
		+ FMath::Sin(2.0 * GeomMeanAnomSunRad)*(0.019993 - 0.000101*JulianCentury)
		+ FMath::Sin(3.0 * GeomMeanAnomSunRad)*0.000289;

	// Get the sun's true longitude
	double SunTrueLongDeg = GeomMeanLongSunDeg + SunEqOfCtr;

	// Get the sun's true anomaly
	//	double SunTrueAnomDeg = GeomMeanAnomSunDeg + SunEqOfCtr;
	//	double SunTrueAnomRad = FMath::DegreesToRadians(SunTrueAnomDeg);

	// Get the earth's distance from the sun
	//	double SunRadVectorAUs = (1.000001018*(1.0 - EccentEarthOrbit*EccentEarthOrbit)) / (1.0 + EccentEarthOrbit*FMath::Cos(SunTrueAnomRad));

	// Get the sun's apparent longitude
	double SunAppLongDeg = SunTrueLongDeg - 0.00569 - 0.00478*FMath::Sin(FMath::DegreesToRadians(125.04 - 1934.136*JulianCentury));
	double SunAppLongRad = FMath::DegreesToRadians(SunAppLongDeg);

	// Get the earth's mean obliquity of the ecliptic
	double MeanObliqEclipticDeg = 23.0 + (26.0 + ((21.448 - JulianCentury * (46.815 + JulianCentury * (0.00059 - JulianCentury * 0.001813)))) / 60.0) / 60.0;

	// Get the oblique correction
	double ObliqCorrDeg = MeanObliqEclipticDeg + 0.00256*FMath::Cos(FMath::DegreesToRadians(125.04 - 1934.136*JulianCentury));
	double ObliqCorrRad = FMath::DegreesToRadians(ObliqCorrDeg);

	// Get the sun's right ascension
	double SunRtAscenRad = FMath::Atan2(FMath::Cos(ObliqCorrRad)*FMath::Sin(SunAppLongRad), FMath::Cos(SunAppLongRad));
	double SunRtAscenDeg = FMath::RadiansToDegrees(SunRtAscenRad);

	// Get the sun's declination
	double SunDeclinRad = FMath::Asin(FMath::Sin(ObliqCorrRad)*FMath::Sin(SunAppLongRad));
	double SunDeclinDeg = FMath::RadiansToDegrees(SunDeclinRad);

	double VarY = FMath::Pow(FMath::Tan(ObliqCorrRad / 2.0), 2.0);

	// Get the equation of time
	double EqOfTimeMinutes = 4.0 * FMath::RadiansToDegrees(VarY*FMath::Sin(2.0 * GeomMeanLongSunRad) - 2.0 * EccentEarthOrbit*FMath::Sin(GeomMeanAnomSunRad) + 4.0 * EccentEarthOrbit*VarY*FMath::Sin(GeomMeanAnomSunRad)*FMath::Cos(2.0 * GeomMeanLongSunRad) - 0.5*VarY*VarY*FMath::Sin(4.0 * GeomMeanLongSunRad) - 1.25*EccentEarthOrbit*EccentEarthOrbit*FMath::Sin(2.0 * GeomMeanAnomSunRad));

	// Get the hour angle of the sunrise
	double HASunriseDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Cos(FMath::DegreesToRadians(90.833)) / (FMath::Cos(LatitudeRad)*FMath::Cos(SunDeclinRad)) - FMath::Tan(LatitudeRad)*FMath::Tan(SunDeclinRad)));
	//	double SunlightDurationMinutes = 8.0 * HASunriseDeg;

	// Get the local time of the sun's rise and set
	double SolarNoonLST = (720.0 - 4.0 * Longitude - EqOfTimeMinutes + TimeOffset * 60.0) / 1440.0;
	double SunriseTimeLST = SolarNoonLST - HASunriseDeg * 4.0 / 1440.0;
	double SunsetTimeLST = SolarNoonLST + HASunriseDeg * 4.0 / 1440.0;

	// Get the true solar time
	double TrueSolarTimeMinutes = FMath::Fmod(CalcTime.GetTimeOfDay().GetTotalMinutes() + EqOfTimeMinutes + 4.0 * Longitude - 60.0 * TimeOffset, 1440.0);

	// Get the hour angle of current time
	double HourAngleDeg = TrueSolarTimeMinutes < 0 ? TrueSolarTimeMinutes / 4.0 + 180 : TrueSolarTimeMinutes / 4.0 - 180.0;
	double HourAngleRad = FMath::DegreesToRadians(HourAngleDeg);

	// Get the solar zenith angle
	double SolarZenithAngleRad = FMath::Acos(FMath::Sin(LatitudeRad)*FMath::Sin(SunDeclinRad) + FMath::Cos(LatitudeRad)*FMath::Cos(SunDeclinRad)*FMath::Cos(HourAngleRad));
	double SolarZenithAngleDeg = FMath::RadiansToDegrees(SolarZenithAngleRad);

	// Get the sun elevation
	double SolarElevationAngleDeg = 90.0 - SolarZenithAngleDeg;
	double SolarElevationAngleRad = FMath::DegreesToRadians(SolarElevationAngleDeg);
	double TanOfSolarElevationAngle = FMath::Tan(SolarElevationAngleRad);

	// Get the approximated atmospheric refraction
	double ApproxAtmosphericRefractionDeg = 0.0;
	if (SolarElevationAngleDeg <= 85.0)
	{
		if (SolarElevationAngleDeg > 5.0)
		{
			ApproxAtmosphericRefractionDeg = 58.1 / TanOfSolarElevationAngle - 0.07 / FMath::Pow(TanOfSolarElevationAngle, 3) + 0.000086 / FMath::Pow(TanOfSolarElevationAngle, 5) / 3600.0;
		}
		else
		{
			if (SolarElevationAngleDeg > -0.575)
			{
				ApproxAtmosphericRefractionDeg = 1735.0 + SolarElevationAngleDeg * (-518.2 + SolarElevationAngleDeg * (103.4 + SolarElevationAngleDeg * (-12.79 + SolarElevationAngleDeg * 0.711)));
			}
			else
			{
				ApproxAtmosphericRefractionDeg = -20.772 / TanOfSolarElevationAngle;
			}
		}
		ApproxAtmosphericRefractionDeg /= 3600.0;
	}

	// Get the corrected solar elevation
	double SolarElevationcorrectedforatmrefractionDeg = SolarElevationAngleDeg + ApproxAtmosphericRefractionDeg;

	// Get the solar azimuth
	double tmp = FMath::RadiansToDegrees(FMath::Acos(((FMath::Sin(LatitudeRad)*FMath::Cos(SolarZenithAngleRad)) - FMath::Sin(SunDeclinRad)) / (FMath::Cos(LatitudeRad)*FMath::Sin(SolarZenithAngleRad))));
	double SolarAzimuthAngleDegcwfromN = HourAngleDeg > 0.0f ? FMath::Fmod(tmp + 180.0f, 360.0f) : FMath::Fmod(540.0f - tmp, 360.0f);

	// offset elevation angle to fit with UE coords system
	FModumateSunPositionData SunPositionData;

	SunPositionData.Elevation = 180.0f + SolarElevationAngleDeg;
	SunPositionData.CorrectedElevation = 180.0f + SolarElevationcorrectedforatmrefractionDeg;
	SunPositionData.Azimuth = SolarAzimuthAngleDegcwfromN;
	SunPositionData.SolarNoon = FTimespan::FromDays(SolarNoonLST);
	SunPositionData.SunriseTime = FTimespan::FromDays(SunriseTimeLST);
	SunPositionData.SunsetTime = FTimespan::FromDays(SunsetTimeLST);

	return SunPositionData;
}

void UModumateFunctionLibrary::SetSkylightCubemapAngle(USkyLightComponent* SkyLight, float Angle)
{
	SkyLight->SourceCubemapAngle = Angle;
}

float UModumateFunctionLibrary::GetViewportDPIScale()
{
	FVector2D viewportSize;
	GEngine->GameViewport->GetViewportSize(viewportSize);
	int32 X = FGenericPlatformMath::FloorToInt(viewportSize.X);
	int32 Y = FGenericPlatformMath::FloorToInt(viewportSize.Y);
	return GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(X, Y));
}

static FString FormatDateString(const FDateTime &dt, const TCHAR *monthNames[])
{
	return monthNames[dt.GetMonth() - 1] + FString(TEXT(" ")) + FString::FromInt(dt.GetDay()) + TEXT(", ") + FString::FromInt(dt.GetYear());
}

EObjectType UModumateFunctionLibrary::GetMOITypeFromActor(AActor * MOIActor)
{
	if (MOIActor != nullptr)
	{
		AEditModelGameState_CPP *gameState = MOIActor->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		FModumateObjectInstance *moi = gameState->Document.ObjectFromActor(MOIActor);
		if (moi == nullptr)
		{
			return EObjectType::OTUnknown;
		}
		else
		{
			return moi->GetObjectType();
		}
	}
	else
	{
		return EObjectType::OTUnknown;
	}
}

void UModumateFunctionLibrary::AddNewDimensionString(const AEditModelPlayerController_CPP *controller, const FVector &p1, const FVector &p2, const FVector &offsetDir, const FName &groupID, const FName &uniqueID, const int32 groupIndex, const AActor* owner, EDimStringStyle style, EEnterableField functionality, const float offset, EAutoEditableBox autoTextBox, const bool alwaysVisible, const FLinearColor &color)
{
	if (owner == nullptr)
	{
		return;
	}
	FModelDimensionString newDimensionString;
	newDimensionString.Point1 = p1;
	newDimensionString.Point2 = p2;
	newDimensionString.Owner = const_cast<AActor*>(owner);
	newDimensionString.Style = style;
	newDimensionString.OffsetDirection = offsetDir;
	newDimensionString.GroupID = groupID;
	newDimensionString.GroupIndex = groupIndex;

	AEditModelPlayerController_CPP* playerController = const_cast<AEditModelPlayerController_CPP*>(controller);
	switch (autoTextBox)
	{
	case EAutoEditableBox::Never:
		newDimensionString.Functionality = functionality;
		break;
	case EAutoEditableBox::UponUserInput:
		if (playerController->HasPendingTextBoxUserInput())
		{
			newDimensionString.Functionality = EEnterableField::EditableText_ImperialUnits_UserInput;
		}
		else
		{
			newDimensionString.Functionality = functionality;
		}
		break;
	case EAutoEditableBox::UponUserInput_SameGroupIndex:
		if (playerController->HasPendingTextBoxUserInput() && playerController->EMPlayerState->CurrentDimensionStringGroupIndex == groupIndex)
		{
			newDimensionString.Functionality = EEnterableField::EditableText_ImperialUnits_UserInput;
		}
		else
		{
			newDimensionString.Functionality = functionality;
		}
		break;
	default:
		break;
	}
	newDimensionString.Offset = offset;
	newDimensionString.UniqueID = uniqueID;
	newDimensionString.bAlwaysVisible = alwaysVisible;
	newDimensionString.Color = color;
	controller->EMPlayerState->DimensionStrings.Add(newDimensionString);
}

void UModumateFunctionLibrary::AddNewDegreeString(const AEditModelPlayerController_CPP *controller, const FVector &location, const FVector &start, const FVector &end, const bool clockwise, const FName &groupID, const FName &uniqueID, const int32 groupIndex, const AActor* owner, EDimStringStyle style, EEnterableField functionality /*= EEnterableField::NonEditableText*/, EAutoEditableBox autoTextBox /*= EAutoEditableBox::UponUserInput*/, const bool alwaysVisible /*= false*/, const FLinearColor &color /*= FLinearColor::White*/)
{
	if (owner == nullptr)
	{
		return;
	}
	FModelDimensionString newDimensionString;
	newDimensionString.bDegreeClockwise = clockwise;
	newDimensionString.DegreeDirectionStart = start;
	newDimensionString.DegreeDirectionEnd = end;
	newDimensionString.DegreeLocation = location;
	newDimensionString.Owner = const_cast<AActor*>(owner);
	newDimensionString.Style = style;
	newDimensionString.GroupID = groupID;
	newDimensionString.GroupIndex = groupIndex;

	//TODO: assume angle range 0-180 with direction indicated by "clockwise." 
	newDimensionString.AngleDegrees = FVector::DotProduct((start-location).GetSafeNormal(), (end-location).GetSafeNormal());
	newDimensionString.AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(newDimensionString.AngleDegrees));

	AEditModelPlayerController_CPP* playerController = const_cast<AEditModelPlayerController_CPP*>(controller);
	switch (autoTextBox)
	{
	case EAutoEditableBox::Never:
		newDimensionString.Functionality = functionality;
		break;
	case EAutoEditableBox::UponUserInput:
		if (playerController->HasPendingTextBoxUserInput())
		{
			newDimensionString.Functionality = EEnterableField::EditableText_ImperialUnits_UserInput;
		}
		else
		{
			newDimensionString.Functionality = functionality;
		}
		break;
	case EAutoEditableBox::UponUserInput_SameGroupIndex:
		if (playerController->HasPendingTextBoxUserInput() && playerController->EMPlayerState->CurrentDimensionStringGroupIndex == groupIndex)
		{
			newDimensionString.Functionality = EEnterableField::EditableText_ImperialUnits_UserInput;
		}
		else
		{
			newDimensionString.Functionality = functionality;
		}
		break;
	default:
		break;
	}

	newDimensionString.UniqueID = uniqueID;
	newDimensionString.bAlwaysVisible = alwaysVisible;
	newDimensionString.Color = color;
	controller->EMPlayerState->DimensionStrings.Add(newDimensionString);
}

FBoxSphereBounds UModumateFunctionLibrary::GetSelectedExtents(const AEditModelPlayerController_CPP * Controller)
{
	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	AEditModelPlayerState_CPP *playerState = Controller->EMPlayerState;
	const FModumateDocument &doc = gameState->Document;

	TArray<FVector> selectedMOIPoints;
	TArray<FStructurePoint> curMOIPoints;
	TArray<FStructureLine> curMOILines;

	for (const FModumateObjectInstance* moi : playerState->SelectedObjects)
	{
		curMOIPoints.Reset();
		curMOILines.Reset();
		moi->GetStructuralPointsAndLines(curMOIPoints, curMOILines);

		for (const FStructurePoint &point : curMOIPoints)
		{
			selectedMOIPoints.Add(point.Point);
		}
	}

	FBoxSphereBounds selectionBounds(selectedMOIPoints.GetData(), selectedMOIPoints.Num());
	return selectionBounds;
}

void UModumateFunctionLibrary::SetWindowTitle(const FString& ProjectName)
{
	FText AppNameText = FText::FromString(FApp::GetProjectName());
	FText WindowTitle = AppNameText;

	if (!ProjectName.IsEmpty())
	{
		static const FText WindowTitleFormat = FText::FromString(TEXT("{0} - {1}"));
		WindowTitle = FText::Format(WindowTitleFormat, FText::FromString(ProjectName), AppNameText);
	}

	UKismetSystemLibrary::SetWindowTitle(WindowTitle);
}

void UModumateFunctionLibrary::DocAddHideMoiActors(const TArray<AActor*> Actors)
{
	if (Actors.Num() > 0)
	{
		AEditModelGameState_CPP *gameState = Actors[0]->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		FModumateDocument *doc = &gameState->Document;

		// First, find all descendents of the selected actor objects
		TSet<const FModumateObjectInstance *> objectsAndDescendents;
		for (auto curActor : Actors)
		{
			FModumateObjectInstance *moi = gameState->Document.ObjectFromActor(curActor);
			if (moi)
			{
				objectsAndDescendents.Add(moi);

				TArray<FModumateObjectInstance *> descendents = moi->GetAllDescendents();
				for (FModumateObjectInstance *descendent : descendents)
				{
					if (descendent)
					{
						objectsAndDescendents.Add(descendent);
					}
				}
			}
		}

		// Now, gather their IDs, and any parent IDs that are in the graph
		TSet<int32> objectIDsToHide;
		for (const FModumateObjectInstance *object : objectsAndDescendents)
		{
			objectIDsToHide.Add(object->ID);

			int32 parentID = object->GetParentID();
			EGraph3DObjectType parentGraphObjectType;
			if (doc->IsObjectInVolumeGraph(parentID, parentGraphObjectType))
			{
				objectIDsToHide.Add(parentID);
			}
		}

		doc->AddHideObjectsById(Actors[0]->GetWorld(), objectIDsToHide.Array());
	}
}

void UModumateFunctionLibrary::DocUnHideAllMoiActors(const AActor* Owner)
{
	if (Owner != nullptr)
	{
		AEditModelGameState_CPP *gameState = Owner->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		FModumateDocument *doc = &gameState->Document;
		doc->UnhideAllObjects(Owner->GetWorld());
	}
}

FName UModumateFunctionLibrary::GetShopItemFromActor(AActor* TargetActor, bool& bSuccess)
{
	if (TargetActor != nullptr)
	{
		AEditModelGameState_CPP *gameState = TargetActor->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		FModumateDocument *doc = &gameState->Document;
		FModumateObjectInstance *moi = doc->ObjectFromActor(TargetActor);
		
		if (moi != nullptr)
		{
			bSuccess = true;
			return moi->GetAssembly().UniqueKey();
		}
	}
	bSuccess = false;
	return NAME_None;
}

using namespace Modumate;
bool UModumateFunctionLibrary::GetCabinetToeKickDimensions(const FBIMAssemblySpec &obAsm, FVector2D &outToeKickDims)
{
	outToeKickDims.Set(0.0f, 0.0f);

	FString depth, height;
	if (obAsm.CachedAssembly.Properties.TryGetProperty(BIM::EScope::ToeKick,BIM::Parameters::Depth,depth)
		&& obAsm.CachedAssembly.Properties.TryGetProperty(BIM::EScope::ToeKick,BIM::Parameters::Height,height))
	{
		outToeKickDims.X = UModumateDimensionStatics::StringToFormattedDimension(depth).Centimeters;
		outToeKickDims.Y = UModumateDimensionStatics::StringToFormattedDimension(height).Centimeters;
		return true;
	}

	return false;
}

bool UModumateFunctionLibrary::SetMeshMaterial(UMeshComponent *MeshComponent, const FArchitecturalMaterial &Material, int32 MatIndex, UMaterialInstanceDynamic** CachedMIDPtr)
{
	static const FName baseColorParamName(TEXT("ColorMultiplier"));
	static const FName uvScaleParamName(TEXT("UVScale"));

	if (Material.EngineMaterial.IsValid())
	{
		UMaterialInterface* engineMat = Material.EngineMaterial.Get();

		// Only create and set a MaterialInstanceDynamic if there's a param (color) that needs to be set.
		if (Material.DefaultBaseColor.IsValid())
		{
			UMaterialInterface* curEngineMat = MeshComponent->GetMaterial(MatIndex);
			UMaterialInstanceDynamic* curMID = Cast<UMaterialInstanceDynamic>(curEngineMat);
			bool bReapplyMID = false;

			// If the mesh's current material isn't the right MID, see if the cached one is.
			if (((curMID == nullptr) || (curMID->Parent != engineMat)) && CachedMIDPtr)
			{
				curMID = *CachedMIDPtr;
				bReapplyMID = true;
			}

			if ((curMID == nullptr) || (curMID->Parent != engineMat))
			{
				curMID = UMaterialInstanceDynamic::Create(engineMat, MeshComponent);
				MeshComponent->SetMaterial(MatIndex, curMID);
			}
			else if (bReapplyMID)
			{
				MeshComponent->SetMaterial(MatIndex, curMID);
			}

			curMID->SetVectorParameterValue(baseColorParamName, Material.DefaultBaseColor.Color);
			curMID->SetScalarParameterValue(uvScaleParamName, Material.UVScaleFactor);

			// Cache the MID if requested
			if (CachedMIDPtr)
			{
				*CachedMIDPtr = curMID;
			}
		}
		// Otherwise just set the material to the base MaterialInterface asset.
		else
		{
			MeshComponent->SetMaterial(MatIndex, engineMat);
		}

		return true;
	}

	return false;
}

bool UModumateFunctionLibrary::SetMeshMaterialsFromMapping(UMeshComponent *MeshComponent, const TMap<FName, FArchitecturalMaterial> &MaterialMapping, const TMap<FName, int32> *MatIndexMapping)
{
	bool bSuccess = false;

	for (auto &kvp : MaterialMapping)
	{
		const FArchitecturalMaterial &matData = kvp.Value;
		FName materialSlotName = kvp.Key;
		int32 matIndex = INDEX_NONE;
		if (MatIndexMapping && MatIndexMapping->Contains(materialSlotName))
		{
			matIndex = MatIndexMapping->FindChecked(materialSlotName);
		}
		else
		{
			matIndex = MeshComponent->GetMaterialIndex(materialSlotName);
		}

		if (matIndex == INDEX_NONE)
		{
			continue;
		}

		if (SetMeshMaterial(MeshComponent, matData, matIndex))
		{
			bSuccess = true;
		}
	}

	return bSuccess;
}

bool UModumateFunctionLibrary::SetMeshMaterialsFromAssemblyLayer(UMeshComponent* MeshComponent, const FModumateObjectAssemblyLayer &AssemblyLayer, const TMap<FName, int32> *MatIndexMapping)
{
	bool bSuccess = false;

	for (int32 i = -1; i < AssemblyLayer.ExtraMaterials.Num(); ++i)
	{
		auto &matData = (i < 0) ? AssemblyLayer.Material : AssemblyLayer.ExtraMaterials[i];
		if (matData.EngineMaterial.IsValid())
		{
			FName materialSlotName(*FString::Printf(TEXT("Material%d"), i + 1));
			int32 matIndex = INDEX_NONE;
			if (MatIndexMapping && MatIndexMapping->Contains(materialSlotName))
			{
				matIndex = MatIndexMapping->FindChecked(materialSlotName);
			}
			else
			{
				matIndex = MeshComponent->GetMaterialIndex(materialSlotName);
			}

			if (!ensureMsgf(matIndex != INDEX_NONE, TEXT("Couldn't find mesh material index on %s for slot %s"),
				*MeshComponent->GetName(), *materialSlotName.ToString()))
			{
				continue;
			}

			if (SetMeshMaterial(MeshComponent, matData, matIndex))
			{
				bSuccess = true;
			}
		}
	}

	return bSuccess;
}

FColor UModumateFunctionLibrary::GetColorFromHex(FString Hex)
{
	return FColor::FromHex(Hex);
}

FString UModumateFunctionLibrary::GetHexFromColor(FColor Color)
{
	return Color.ToHex();
}

FLinearColor UModumateFunctionLibrary::ModumateMakeFromHSV(uint8 H, uint8 S, uint8 V)
{
	return FLinearColor::MakeFromHSV8(H, S, V);
}

void UModumateFunctionLibrary::ModumateDebugDrawSphere(AActor* Outer, FVector Location, float Radius, float DrawTime, FLinearColor DrawColor)
{
	FColor debugColor = DrawColor.ToFColor(false);
	DrawDebugSphere(Outer->GetWorld(), Location, Radius, 8, debugColor, false, DrawTime);
}

FString UModumateFunctionLibrary::GetEnumString(UEnum* EnumClass, uint8 EnumValue)
{
	return EnumClass ? EnumClass->GetNameStringByValue(EnumValue) : FString();
}

void UModumateFunctionLibrary::DocTransverseObject(AActor* MoiActor)
{
	TArray<FVector> resultVerts;
	if (MoiActor != nullptr)
	{
		AEditModelGameState_CPP * GameState = MoiActor->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		FModumateObjectInstance *moi = GameState->Document.ObjectFromActor(MoiActor);
		GameState->Document.TransverseObjects(TArray<FModumateObjectInstance*>{moi});
	}
}

void UModumateFunctionLibrary::PopulatePatternModuleVariables(TMap<FString, float> &patternExprVars, const FVector &moduleDims, int32 moduleIdx)
{
	auto makeModuleDimensionKey = [](int32 idx, const TCHAR* dimensionPrefix) {
		return (idx == 0) ? FString(dimensionPrefix) : FString::Printf(TEXT("%s%d"), dimensionPrefix, idx + 1);
	};

	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("L")), moduleDims.X);
	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("W")), moduleDims.Y);
	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("H")), moduleDims.Z);
}

bool UModumateFunctionLibrary::ApplyTileMaterialToMeshFromLayer(UProceduralMeshComponent *MeshComponent, const FModumateObjectAssemblyLayer &Layer,
	const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial, UMaterialInstanceDynamic** CachedMIDPtr)
{
	static const FString continuousPatternKey(TEXT("Continuous"));
	static const FName masterPBRTexParamBaseColor(TEXT("BaseColor"));
	static const FName masterPBRTexParamMRSA(TEXT("MRSA"));
	static const FName masterPBRTexParamNormal(TEXT("Normal"));
	static const FName patternExtentsParamName(TEXT("TotalSize"));
	static const FName bgColorParamName(TEXT("BackgroundColorMult"));
	static const FName bgTexParamBaseColor(TEXT("BackgroundBaseColor"));
	static const FName bgTexParamMRSA(TEXT("BackgroundMRSA"));
	static const FName bgTexParamNormal(TEXT("BackgroundNormal"));
	static const FName bgParamColor(TEXT("BackgroundColorMult"));
	static const FString commaStr(TEXT(","));

	if (ensure(MeshComponent))
	{
		const auto &pattern = Layer.Pattern;
		int32 numModuleTiles = pattern.ParameterizedModuleDimensions.Num();
		auto *layerOverrideMat = Layer.Material.EngineMaterial.Get();

		// Check if incoming key from pattern is continuous, if true, modify color only
		if (MeshComponent && !pattern.Key.IsNone() && (pattern.Key.ToString() != continuousPatternKey) && (numModuleTiles > 0) &&
			ensure((pattern.ModuleCount == Layer.Modules.Num()) && numModuleTiles <= TilingMaterials.Num()) &&
			TilingMaterials[numModuleTiles - 1])
		{
			TMap<FString, float> patternExprVars;
			patternExprVars.Add(FString(TEXT("G")), Layer.Gap.GapExtents.X);

			// Define the dimension parameters for each module definition
			for (int32 moduleIdx = 0; moduleIdx < pattern.ModuleCount; ++moduleIdx)
			{
				auto &moduleData = Layer.Modules[moduleIdx];

				// Define L, W, and H since some 3D patterns can be applied to 2D modules,
				// so make sure all extents are defined for all modules.
				FVector moduleDims = moduleData.ModuleExtents;
				if (moduleDims.Z == 0.0f)
				{
					moduleDims.Z = moduleDims.Y;
				}

				PopulatePatternModuleVariables(patternExprVars, moduleDims, moduleIdx);
			}

			FString extentsExpressions = pattern.ParameterizedExtents;
			extentsExpressions.RemoveFromStart(TEXT("("));
			extentsExpressions.RemoveFromEnd(TEXT(")"));

			FString patternWidthExpr, patternHeightExpr;
			FVector patternExtentsValue(ForceInitToZero);
			if (extentsExpressions.Split(commaStr, &patternWidthExpr, &patternHeightExpr))
			{
				patternExtentsValue.X = Modumate::Expression::Evaluate(patternExprVars, patternWidthExpr);
				patternExtentsValue.Y = Modumate::Expression::Evaluate(patternExprVars, patternHeightExpr);
			}

			// Get (or create) the MaterialInstanceDynamic for the desired tiling material on the target layer mesh.
			UMaterialInterface *tilingMaterial = TilingMaterials[numModuleTiles - 1];
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(0));
			if (MID && (MID->Parent == tilingMaterial))
			{
				MID->ClearParameterValues();
			}
			else
			{
				MID = UMaterialInstanceDynamic::Create(tilingMaterial, MeshComponent);
				MeshComponent->SetMaterial(0, MID);
			}

			// Set the extents
			MID->SetVectorParameterValue(patternExtentsParamName, patternExtentsValue);

			// If the layer specifies a gap background material, then set its parameters here
			auto *layerGapMat = Layer.Gap.Material.EngineMaterial.Get();
			auto *layerGapMatInst = Cast<UMaterialInstance>(layerGapMat);
			if (layerGapMatInst && (layerGapMatInst->Parent == MasterPBRMaterial))
			{
				UTexture *bgTexBaseColor = nullptr, *bgTexMRSA = nullptr, *bgTexNormal = nullptr;
				if (layerGapMatInst->GetTextureParameterValue(FMaterialParameterInfo(masterPBRTexParamBaseColor), bgTexBaseColor) &&
					layerGapMatInst->GetTextureParameterValue(FMaterialParameterInfo(masterPBRTexParamMRSA), bgTexMRSA) &&
					layerGapMatInst->GetTextureParameterValue(FMaterialParameterInfo(masterPBRTexParamNormal), bgTexNormal))
				{
					MID->SetTextureParameterValue(bgTexParamBaseColor, bgTexBaseColor);
					MID->SetTextureParameterValue(bgTexParamMRSA, bgTexMRSA);
					MID->SetTextureParameterValue(bgTexParamNormal, bgTexNormal);
				}

				if (Layer.Gap.BaseColor.IsValid())
				{
					// NOTE: this assumes the provided color is in sRGB space,
					// otherwise we should use layerData.GapBaseColor.ReinterpretAsLinear()
					FLinearColor bgColor = Layer.Gap.BaseColor.Color;
					MID->SetVectorParameterValue(bgColorParamName, bgColor);
				}
			}

			TArray<FPatternModuleParams> allModuleInstParams;
			for (int32 moduleInstIdx = 0; moduleInstIdx < numModuleTiles; ++moduleInstIdx)
			{
				FPatternModuleParams moduleInstParams = FPatternModuleParams();

				// Parse the module dimensions into X, Y, W, H, and material index
				auto &moduleInstData = pattern.ParameterizedModuleDimensions[moduleInstIdx];

				moduleInstParams.Dimensions = FLinearColor(
					Modumate::Expression::Evaluate(patternExprVars, moduleInstData.ModuleXExpr),
					Modumate::Expression::Evaluate(patternExprVars, moduleInstData.ModuleYExpr),
					Modumate::Expression::Evaluate(patternExprVars, moduleInstData.ModuleWidthExpr),
					Modumate::Expression::Evaluate(patternExprVars, moduleInstData.ModuleHeightExpr)
				);

				// TODO: populate texture and color details from real data
				static const float defaultHueVariation = 0.0f;
				static const float defaultBrightnessVariation = 0.1f;
				static const float defaultTextureRotation = 0.0f;
				static const float defaultUVScale = 1.0f;
				moduleInstParams.TileTexDetails = FLinearColor(defaultHueVariation, defaultBrightnessVariation, defaultTextureRotation, defaultUVScale);

				// TODO: populate bevel details from real data
				static const float defaultBevelWidth = 0.5f;
				static const float defaultBevelIntensity = 1.5f;
				moduleInstParams.TileShapeDetails = FLinearColor(defaultBevelWidth, defaultBevelIntensity, 0.0f, 0.0f);

				if (moduleInstData.ModuleDefIndex < Layer.Modules.Num())
				{
					auto &moduleDef = Layer.Modules[moduleInstData.ModuleDefIndex];
					moduleInstParams.TileShapeDetails.R = moduleDef.BevelWidth.AsWorldCentimeters();

					auto &moduleMaterialData = moduleDef.Material;

					if (moduleMaterialData.DefaultBaseColor.IsValid())
					{
						moduleInstParams.Color = moduleMaterialData.DefaultBaseColor.Color;
					}

					moduleInstParams.TileTexDetails.A = moduleMaterialData.UVScaleFactor;

					auto *moduleSourceMat = Layer.Material.IsValid() ?
						Layer.Material.EngineMaterial.Get() :
						moduleMaterialData.EngineMaterial.Get();

					auto *moduleMatInst = Cast<UMaterialInstance>(moduleSourceMat);
					if (moduleMatInst && (moduleMatInst->Parent == MasterPBRMaterial))
					{
						moduleSourceMat->GetTextureParameterValue(FMaterialParameterInfo(masterPBRTexParamBaseColor), moduleInstParams.BaseColorTex);
						moduleSourceMat->GetTextureParameterValue(FMaterialParameterInfo(masterPBRTexParamMRSA), moduleInstParams.MRSATex);
						moduleSourceMat->GetTextureParameterValue(FMaterialParameterInfo(masterPBRTexParamNormal), moduleInstParams.NormalTex);
					}
				}

				// NOTE: this assumes the provided colors are in sRGB space,
				// otherwise we should use layerData.BaseColor.ReinterpretAsLinear()
				if (Layer.BaseColor.IsValid())
				{
					moduleInstParams.Color = Layer.BaseColor.Color;
				}
				else if (Layer.Material.IsValid() && Layer.Material.DefaultBaseColor.IsValid())
				{
					moduleInstParams.Color = Layer.Material.DefaultBaseColor.Color;
				}

				auto makeTileParamName = [moduleInstIdx](const TCHAR* paramSuffix) {
					return FName(*FString::Printf(TEXT("Tile%d%s"), moduleInstIdx + 1, paramSuffix));
				};

				MID->SetVectorParameterValue(makeTileParamName(TEXT("Dims")), moduleInstParams.Dimensions);
				MID->SetVectorParameterValue(makeTileParamName(TEXT("ColorMult")), moduleInstParams.Color);
				MID->SetVectorParameterValue(makeTileParamName(TEXT("TexDetails")), moduleInstParams.TileTexDetails);
				MID->SetVectorParameterValue(makeTileParamName(TEXT("ShapeDetails")), moduleInstParams.TileShapeDetails);
				MID->SetTextureParameterValue(makeTileParamName(TEXT("BaseColor")), moduleInstParams.BaseColorTex);
				MID->SetTextureParameterValue(makeTileParamName(TEXT("MRSA")), moduleInstParams.MRSATex);
				MID->SetTextureParameterValue(makeTileParamName(TEXT("Normal")), moduleInstParams.NormalTex);
			}

			return true;
		}
		else
		{
			auto* dynMat = MeshComponent->CreateDynamicMaterialInstance(0, MeshComponent->GetMaterial(0));
			if (dynMat != nullptr)
			{
				dynMat->SetVectorParameterValue(TEXT("ColorMultiplier"), Layer.BaseColor.Color);
			}
		}
	}

	return false;
}

bool UModumateFunctionLibrary::UpdateMaterialsFromAssembly(const TArray<UProceduralMeshComponent*> &ProceduralSubLayers, const FBIMAssemblySpec &Assembly,
	const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial,
	TArray<UMaterialInstanceDynamic*> *CachedMIDs, bool bLayersReversed)
{
	bool bAppliedAnyTileMaterials = false;
	int32 numLayers = Assembly.CachedAssembly.Layers.Num();

	if (ensure(ProceduralSubLayers.Num() == numLayers))
	{
		for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
		{
			UProceduralMeshComponent *layerMesh = ProceduralSubLayers[layerIdx];

			int32 fixedLayerIdx = bLayersReversed ? (numLayers - layerIdx - 1) : layerIdx;
			const FModumateObjectAssemblyLayer &layerData = Assembly.CachedAssembly.Layers[fixedLayerIdx];

			UMaterialInstanceDynamic** cachedMIDPtr = nullptr;
			if (CachedMIDs && (layerIdx < CachedMIDs->Num()))
			{
				cachedMIDPtr = &((*CachedMIDs)[layerIdx]);
			}

			if (UModumateFunctionLibrary::ApplyTileMaterialToMeshFromLayer(layerMesh, layerData,
				TilingMaterials, MasterPBRMaterial, cachedMIDPtr))
			{
				bAppliedAnyTileMaterials = true;
			}
			else
			{
				UModumateFunctionLibrary::SetMeshMaterial(layerMesh, layerData.Material, 0, cachedMIDPtr);
			}
		}
	}

	return bAppliedAnyTileMaterials;
}

void UModumateFunctionLibrary::CopyToClipboard(const FString& ClipboardContents)
{
	FPlatformApplicationMisc::ClipboardCopy(*ClipboardContents);
}

void UModumateFunctionLibrary::GetClipboardContents(FString& ClipboardContents)
{
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContents);
}

FString UModumateFunctionLibrary::GetProjectVersion()
{
	return FModumateModule::Get().GetProjectDisplayVersion();
}

bool UModumateFunctionLibrary::IsTargetCloseToControlPointEdges(const FVector& target, const TArray<FVector>& controlPoints, const FTransform& parentTransform, float tolerance)
{
	bool isClose = false;
	TArray<FVector> worldCPs;
	for (FVector curCP : controlPoints)
	{
		worldCPs.Add(parentTransform.TransformPosition(curCP));
	}
	for (int32 curP = 0; curP < worldCPs.Num(); ++curP)
	{
		int32 nextP = (curP + 1 < worldCPs.Num()) ? curP + 1 : 0;
		float dist = UKismetMathLibrary::GetPointDistanceToSegment(target, worldCPs[curP], worldCPs[nextP]);
		if (dist < tolerance)
		{
			isClose = true;
			break;
		}
	}
	return isClose;
}

bool UModumateFunctionLibrary::ClipLine2DToRectangle(const FVector2D& lineStart, const FVector2D& lineEnd, const FBox2D& rectBox, FVector2D& returnLineStart, FVector2D& returnLineEnd, float tolerance)
{
	// Establish border from screenSize
	FVector topLeft = FVector(rectBox.Min.X, rectBox.Min.Y, 0.f);
	FVector topRight = FVector(rectBox.Max.X, rectBox.Min.Y, 0.f);
	FVector bottomLeft = FVector(rectBox.Min.X, rectBox.Max.Y, 0.f);
	FVector bottomRight = FVector(rectBox.Max.X, rectBox.Max.Y, 0.f);
	FVector start = FVector(lineStart.X, lineStart.Y, 0.f);
	FVector end = FVector(lineEnd.X, lineEnd.Y, 0.f);

	// If both points in line are in border, return the originals
	bool startInBorder = rectBox.IsInside(lineStart);
	bool endInBorder = rectBox.IsInside(lineEnd);

	if (startInBorder && endInBorder)
	{
		returnLineStart = lineStart;
		returnLineEnd = lineEnd;
		return true;
	}

	// For floating-point precision, make sure that the start point is the one closer to the rectangle.
	// Otherwise, for very large numbers this function will fail to find an intersection when it should have.
	FVector2D rectCenter = rectBox.GetCenter();
	float startDistFromCenter = FVector2D::Distance(lineStart, rectCenter);
	float endDistFromCenter = FVector2D::Distance(lineEnd, rectCenter);
	if (startDistFromCenter > endDistFromCenter)
	{
		Swap(start, end);
	}

	// Loop through the corners to check for intersections
	TArray<FVector> corners = { topLeft, topRight, bottomRight, bottomLeft };
	TArray<FVector> intersections;
	FVector p1, p2;
	for (int32 curId = 0; curId < corners.Num(); ++curId)
	{
		int32 nextId = (curId + 1 < corners.Num()) ? curId + 1 : 0;
		UKismetMathLibrary::FindNearestPointsOnLineSegments(corners[curId], corners[nextId], start, end, p1, p2);
		if (p1.Equals(p2, tolerance))
		{
			if (!IsVectorInArray(intersections, p1, tolerance))
			{
				intersections.AddUnique(p1);
			}
		}
	}
	// Clip both start and end
	if (intersections.Num() == 2)
	{
		returnLineStart = FVector2D(intersections[0]);
		returnLineEnd = FVector2D(intersections[1]);
		return true;
	}
	// Clip end only
	if (startInBorder && (intersections.Num() == 1))
	{
		returnLineStart = lineStart;
		returnLineEnd = FVector2D(intersections[0]);
		return true;
	}
	// Clip start only
	if (endInBorder && (intersections.Num() == 1))
	{
		returnLineStart = FVector2D(intersections[0]);
		returnLineEnd = lineEnd;
		return true;
	}
	// No clip, both points are outside the rectangle and no intersection with its border
	return false;
}

FVector UModumateFunctionLibrary::GetComponentExtentLocalSpace(const USceneComponent* Comp)
{
	if (!Comp)
	{
		return FVector::ZeroVector;
	}
	FTransform ComponentToActor = Comp->GetComponentTransform() * (Comp->GetComponentTransform().Inverse());
	return Comp->CalcBounds(ComponentToActor).GetBox().GetExtent();
}

FString UModumateFunctionLibrary::GetNextStringInNumericalSequence(const FString &currentString, const TCHAR firstDigitChar, const TCHAR lastDigitChar)
{
	FString ret;
	if (currentString.IsEmpty())
	{
		ret.AppendChar(firstDigitChar);
		return ret;
	}

	ret = currentString;
	if (ret[ret.Len() - 1] == lastDigitChar)
	{
		ret[ret.Len() - 1] = firstDigitChar;
		ret.AppendChar(firstDigitChar);
	}
	else
	{
		ret[ret.Len() - 1] = ret[ret.Len() - 1] + 1;
	}

	return ret;
}

