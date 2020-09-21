// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateFunctionLibrary.h"

#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Objects/Portal.h"
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
#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Runtime/Core/Public/Math/RandomStream.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Algo/Accumulate.h"
#include "BIMKernel/BIMKey.h"

#include <algorithm>
#include <queue>
#include <iostream>
#include <regex>

using namespace std;
using namespace Modumate;


bool UModumateFunctionLibrary::IsWithEditor()
{
	return WITH_EDITOR;
}

bool UModumateFunctionLibrary::NormalizeIDs(TArray<int32>& IDs)
{
	int32 numIDs = IDs.Num();
	if (numIDs <= 1)
	{
		return true;
	}

	int32 minIdx = INDEX_NONE;
	for (int32 i = 0; i < numIDs; ++i)
	{
		int32 curID = IDs[i];
		if ((minIdx == INDEX_NONE) || (curID < IDs[minIdx]))
		{
			minIdx = i;
		}
		else if (curID == IDs[minIdx])
		{
			// A candidate minimum is duplicated, the list can't be normalized.
			return false;
		}
	}

	if (minIdx == 0)
	{
		return true;
	}

	// Append the IDs that are before the minimum
	for (int32 i = 0; i < minIdx; ++i)
	{
		int32 ID = IDs[i];
		IDs.Add(ID);
	}

	// Remove the IDs that are after the minimum
	IDs.RemoveAt(0, minIdx);
	return true;
}

bool UModumateFunctionLibrary::AreNormalizedIDListsEqual(const TArray<int32>& IDList1, const TArray<int32>& IDList2)
{
	int32 numIDs = IDList1.Num();
	if (IDList1.Num() != IDList2.Num())
	{
		return false;
	}

	// If the arrays are identical in the same order, then we're done
	if (IDList1 == IDList2)
	{
		return true;
	}

	// Otherwise, see if the the lists are reverse orders of IDs, starting with the same ID
	bool bBackwards = true;
	int32 reverseIdx = 0;
	for (int32 idx = 0; idx < numIDs; idx++)
	{
		if (IDList1[idx] != IDList2[reverseIdx])
		{
			return false;
		}
		reverseIdx = (reverseIdx + numIDs - 1) % numIDs;
	}

	return true;
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

float UModumateFunctionLibrary::GetViewportDPIScale()
{
	FVector2D viewportSize;
	GEngine->GameViewport->GetViewportSize(viewportSize);
	int32 X = FGenericPlatformMath::FloorToInt(viewportSize.X);
	int32 Y = FGenericPlatformMath::FloorToInt(viewportSize.Y);
	return GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(X, Y));
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

FBIMKey UModumateFunctionLibrary::GetShopItemFromActor(AActor* TargetActor, bool& bSuccess)
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
	return FBIMKey();
}

using namespace Modumate;
bool UModumateFunctionLibrary::GetCabinetToeKickDimensions(const FBIMAssemblySpec &obAsm, FVector2D &outToeKickDims)
{
	outToeKickDims.Set(0.0f, 0.0f);

	FString depth, height;
	if (obAsm.RootProperties.TryGetProperty(EBIMValueScope::ToeKick, BIMPropertyNames::Depth,depth)
		&& obAsm.RootProperties.TryGetProperty(EBIMValueScope::ToeKick, BIMPropertyNames::Height,height))
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

bool UModumateFunctionLibrary::SetMeshMaterialsFromAssemblyLayer(UMeshComponent* MeshComponent, const FBIMLayerSpec &AssemblyLayer, const TMap<FName, int32> *MatIndexMapping)
{
	bool bSuccess = false;

#if 0 // TODO: refactor for new material assignments
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
#endif

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

void UModumateFunctionLibrary::PopulatePatternModuleVariables(TMap<FString, float> &patternExprVars, const FVector &moduleDims, int32 moduleIdx)
{
	auto makeModuleDimensionKey = [](int32 idx, const TCHAR* dimensionPrefix) {
		return (idx == 0) ? FString(dimensionPrefix) : FString::Printf(TEXT("%s%d"), dimensionPrefix, idx + 1);
	};

	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("L")), moduleDims.X);
	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("W")), moduleDims.Y);
	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("H")), moduleDims.Z);
}

bool UModumateFunctionLibrary::ApplyTileMaterialToMeshFromLayer(UProceduralMeshComponent *MeshComponent, const FBIMLayerSpec &Layer,
	const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial, UMaterialInstanceDynamic** CachedMIDPtr)
{

#if 0 // TODO: refactor for new patterns
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
#endif
	return false;
}

bool UModumateFunctionLibrary::UpdateMaterialsFromAssembly(const TArray<UProceduralMeshComponent*> &ProceduralSubLayers, const FBIMAssemblySpec &Assembly,
	const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial,
	TArray<UMaterialInstanceDynamic*> *CachedMIDs, bool bLayersReversed)
{
	bool bAppliedAnyTileMaterials = false;
	int32 numLayers = Assembly.Layers.Num();

	if (ensure(ProceduralSubLayers.Num() == numLayers))
	{
		for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
		{
			UProceduralMeshComponent *layerMesh = ProceduralSubLayers[layerIdx];

			int32 fixedLayerIdx = bLayersReversed ? (numLayers - layerIdx - 1) : layerIdx;
			const FBIMLayerSpec &layerData = Assembly.Layers[fixedLayerIdx];

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
