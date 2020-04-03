// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "ModumateFunctionLibrary.h"
#include "EditModelAdjustmentHandleBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateGeometryStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

// Sets default values
AAdjustmentHandleActor_CPP::AAdjustmentHandleActor_CPP()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	SetMobility(EComponentMobility::Movable);
}


// Called when the game starts or when spawned
void AAdjustmentHandleActor_CPP::BeginPlay()
{
	Super::BeginPlay();

	if (GetStaticMeshComponent() != nullptr)
	{
		HandleMesh = GetStaticMeshComponent();
		OriginalMaterial = HandleMesh->GetMaterial(0);
		HandleMesh->SetCollisionObjectType(COLLISION_HANDLE);

		// Allow outline to be draw over the handle mesh
		HandleMesh->SetRenderCustomDepth(true);
		HandleMesh->SetCustomDepthStencilValue(1);
	}

	Controller = Cast<AEditModelPlayerController_CPP>(this->GetWorld()->GetFirstPlayerController());

	bEnabled = true;
}

void AAdjustmentHandleActor_CPP::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsHovered)
	{
		OnHover(false);
	}

	// If the adjust handle actor is destroyed while it's in use, then abort the handle usage.
	if (Implementation && Controller && Controller->EMPlayerState &&
		(Controller->EMPlayerState->InteractionHandle == this))
	{
		UE_LOG(LogTemp, Warning, TEXT("Adjustment handle was destroyed while it was being used!"));
		Implementation->OnAbortUse();
	}

	bEnabled = false;

	Super::EndPlay(EndPlayReason);
}

void AAdjustmentHandleActor_CPP::OnHover(bool bNewHovered)
{
	if (bIsHovered != bNewHovered)
	{
		bIsHovered = bNewHovered;

		if (DynamicMaterial)
		{
			static const FName hoverParamName(TEXT("Hover"));
			DynamicMaterial->SetScalarParameterValue(hoverParamName, bIsHovered ? 1.0f : 0.0f);
		}
	}
}

// Called every frame
void AAdjustmentHandleActor_CPP::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Implementation)
	{
		if (ParentMOI && HandleType == EHandleType::Justification)
		{
			HandleDirection = ParentMOI->GetNormal();
			FVector attachLocation = Implementation->GetAttachmentPoint();
			UModumateFunctionLibrary::ComponentToUIScreenPosition(HandleMesh, attachLocation, FVector::ZeroVector);

			// Draw line from attachLocation to center of ParentMOI
			Modumate::FModumateObjectInstance* parentMetaplane = ParentMOI->GetParentObject();
			if (parentMetaplane != nullptr && parentMetaplane->ObjectType == EObjectType::OTMetaPlane)
			{
				FVector metaPlaneMidpoint = parentMetaplane->GetObjectLocation();
				if (!metaPlaneMidpoint.Equals(attachLocation))
				{
					// Make new line
					FModumateLines newLine;
					newLine.Point1 = metaPlaneMidpoint;
					newLine.Point2 = attachLocation;
					newLine.Color = FLinearColor::White;
					newLine.Thickness = 2.f;
					Controller->HUDDrawWidget->LinesToDraw.Add(newLine);
				}

				// TODO: Add hover tag above the justification handles
			}
		}
		else
		{
			// BEGIN Implement screen location adjustment of Default handles
			if (ParentMOI != nullptr && ParentMOI->ControlPoints.Num() > 0)
			{
				if (OverrideHandleDirection.IsUnit())
				{
					HandleDirection = OverrideHandleDirection;
				}
				else if (SideCP.Num() > 1)
				{
					FVector sidePoint0 = ParentMOI->ControlPoints[SideCP[0]];
					FVector sidePoint1 = ParentMOI->ControlPoints[SideCP[1]];
					FVector sideDir = (sidePoint1 - sidePoint0).GetSafeNormal();
					FVector sideNormal = sideDir ^ FVector(polyPlane);

					HandleDirection = sideNormal;
				}
				else if (Side == 0) // Side = which control point is the direction facing
				{
					if (ParentMOI->ControlPoints.Num() > 3)
					{
						FVector vFrom = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[3]);
						FVector vTarget = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[0]);
						HandleDirection = UKismetMathLibrary::GetDirectionUnitVector(vFrom, vTarget);

					}
					else
						if (ParentMOI->ControlPoints.Num() > 1)
						{
							FVector vFrom = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[1]);
							FVector vTarget = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[0]);
							HandleDirection = UKismetMathLibrary::GetDirectionUnitVector(vFrom, vTarget);

						}
				}
				else if (Side == 1)
				{
					if (ParentMOI->ControlPoints.Num() > 3)
					{
						FVector vFrom = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[0]);
						FVector vTarget = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[3]);
						HandleDirection = UKismetMathLibrary::GetDirectionUnitVector(vFrom, vTarget);
					}
					else
						if (ParentMOI->ControlPoints.Num() > 1)
						{
							FVector vFrom = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[0]);
							FVector vTarget = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[1]);
							HandleDirection = UKismetMathLibrary::GetDirectionUnitVector(vFrom, vTarget);
						}

				}
				else if (Side == 2) // Upward
				{
					HandleDirection = FVector(0.0f, 0.0f, 1.0f);
				}
				else if (Side == 3) // Downward
				{
					HandleDirection = FVector(0.0f, 0.0f, -1.0f);
				}
				else if (Side == 4) // perpendicular to CP0 and CP1
				{
					FVector vFrom = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[0]);
					FVector vTarget = UKismetMathLibrary::TransformLocation(ParentMOI->GetActor()->GetActorTransform(), ParentMOI->ControlPoints[1]);
					FVector vWallDir = UKismetMathLibrary::GetDirectionUnitVector(vFrom, vTarget);
					HandleDirection = UKismetMathLibrary::RotateAngleAxis(vWallDir, 90.f, FVector::UpVector);
					ScreenOffset = FVector(0.f, -40.f, 0.f); // Need to take spacing of surrounding handles into account. Size is close to screen space
				}

				UModumateFunctionLibrary::ComponentToUIScreenPositionMoiOffset(HandleMesh, Implementation->GetAttachmentPoint(), HandleDirection, OffSetScreenDistance, ScreenOffset);
			}
			else // Ex. handles' parent that do not have Control Points
			{
				if (Side == 2) // Upward
				{
					ScreenOffset = FVector(0.f, -40.f, 0.f); // Upward adjustment handle: Need to take spacing of surrounding handles into account. Size is close to screen space
					HandleDirection = FVector::UpVector;
					UModumateFunctionLibrary::ComponentToUIScreenPositionMoiOffset(HandleMesh, Implementation->GetAttachmentPoint(), HandleDirection, OffSetScreenDistance, ScreenOffset);
				}
				else if (Side == 1) // Since there's no control points, use ParentMOI's rotation as direction
				{
					HandleDirection = ParentMOI->GetObjectRotation().Vector();
					UModumateFunctionLibrary::ComponentToUIScreenPositionMoiOffset(HandleMesh, Implementation->GetAttachmentPoint(), HandleDirection, OffSetScreenDistance, ScreenOffset);
				}
				else if (Side == 4) // Since there's no control points, use ParentMOI's rotation as direction
				{
					FVector vDir = ParentMOI->GetObjectRotation().Vector();
					HandleDirection = UKismetMathLibrary::RotateAngleAxis(vDir, 90.f, FVector::UpVector);
					UModumateFunctionLibrary::ComponentToUIScreenPositionMoiOffset(HandleMesh, Implementation->GetAttachmentPoint(), HandleDirection, OffSetScreenDistance, ScreenOffset);
				}
				else if (ScreenOffset.Equals(FVector::ZeroVector) == false)
				{
					UModumateFunctionLibrary::ComponentToUIScreenPositionMoiOffset(HandleMesh, Implementation->GetAttachmentPoint(), HandleDirection, OffSetScreenDistance, ScreenOffset);
				}
				else
				{
					UModumateFunctionLibrary::ComponentToUIScreenPosition(HandleMesh, Implementation->GetAttachmentPoint());
				}
			}
			// END Implement screen location adjustment for default handle
		}
		// Make handle size relative to the distance between camera and the handle's MOI
		if (ParentMOI)
		{
			// Objects hosted on metaplane has different method of resizing handlemesh
			if (HandleType == EHandleType::Justification)
			{
				UModumateFunctionLibrary::ComponentAsBillboard(HandleMesh, HandleScreenSize);
				UModumateFunctionLibrary::ScreenSizeComponentFreezeDirection(HandleMesh, HandleDirection);
			}
			else
			{
				float distToScreen = (Controller->PlayerCameraManager->GetCameraLocation() - ParentMOI->GetObjectLocation()).Size();
				// Settings for handle size
				float distHandleMulti = UKismetMathLibrary::MapRangeClamped(distToScreen,
					HandleNearDistance, HandleFarDistance,
					HandleNearScaleMultiplier, HandleFarScaleMultiplier);
				if (!AsUpwardBillboard)
				{
					// Handle will look like a UE4 billboard actor (always face camera)
					UModumateFunctionLibrary::ComponentAsBillboard(HandleMesh, HandleScreenSize * distHandleMulti);
					UModumateFunctionLibrary::ScreenSizeComponentFreezeDirection(HandleMesh, HandleDirection);
				}
				else
				{
					// Handle will face camera while maintaining its screen size
					FVector curDir = (Controller->PlayerCameraManager->GetCameraLocation() - Implementation->GetAttachmentPoint()).GetSafeNormal();
					HandleMesh->SetWorldRotation(FVector(curDir.X * -1.f, curDir.Y * -1.f, 0.f).Rotation());
					FVector boundOrigin, boundExtent;
					ParentMOI->GetActor()->GetActorBounds(true, boundOrigin, boundExtent);
					float estimateSize = (boundExtent.Size()) / (distToScreen * FMath::Sin(Controller->PlayerCameraManager->GetFOVAngle()));
					float curScreenSize = FMath::Clamp(FMath::Abs(estimateSize), 0.f, 0.30f);
					FVector newSize = UModumateFunctionLibrary::GetWorldComponentToScreenSizeScale(HandleMesh, FVector(curScreenSize / 40.f));
					HandleMesh->SetWorldScale3D(newSize);
				}
			}
		}
		FHitResult camResult;
		if (Controller->CameraInputLock)
		{
			return;
		}

		FQuat overrideRotation;
		if (Implementation->GetOverrideHandleRotation(overrideRotation))
		{
			HandleMesh->SetWorldRotation(overrideRotation);
		}

		// Potentially show dim string
		if (bIsHovered && ParentMOI && (HandleType == EHandleType::Default))
		{
			switch (ParentMOI->ObjectType)
			{
			case EObjectType::OTWallSegment:
				ShowHoverWallDimensionString();
				break;
			case EObjectType::OTCabinet:
				ShowHoverCabinetDimensionString();
				break;
			case EObjectType::OTFloorSegment:
			case EObjectType::OTCountertop:
			case EObjectType::OTMetaPlane:
			case EObjectType::OTCutPlane:
				ShowHoverMetaPlaneDimensionString();
				break;
			}
		}
	}
}

void AAdjustmentHandleActor_CPP::SetActorMesh(UStaticMesh *mesh)
{
	if (GetStaticMeshComponent() != nullptr)
	{
		HandleMesh = GetStaticMeshComponent();
		HandleMesh->SetStaticMesh(mesh);
		DynamicMaterial = UMaterialInstanceDynamic::Create(mesh->GetMaterial(0), this);
		HandleMesh->SetMaterial(0, DynamicMaterial);
	}
}

void AAdjustmentHandleActor_CPP::SetHandleScale(const FVector &s)
{
	SetActorScale3D(s);
}

void AAdjustmentHandleActor_CPP::SetHandleScaleScreenSize(const FVector & s)
{
	HandleScreenSize = s;
}

void AAdjustmentHandleActor_CPP::SetOverrideHandleDirection(const FVector& overrideHandleDirection, Modumate::FModumateObjectInstance* MOI, float offsetDist)
{
	OverrideHandleDirection = overrideHandleDirection;
	ParentMOI = MOI;
	OffSetScreenDistance = offsetDist;
}

void AAdjustmentHandleActor_CPP::SetWallHandleSide(const int32 & side, Modumate::FModumateObjectInstance* MOI, float offsetDist)
{
	Side = side;
	ParentMOI = MOI;
	OffSetScreenDistance = offsetDist;
}

void AAdjustmentHandleActor_CPP::SetPolyHandleSide(const TArray<int32>& CP, Modumate::FModumateObjectInstance * MOI, float offsetDist)
{
	SideCP = CP;
	ParentMOI = MOI;

	if (ParentMOI->ControlPoints.Num() >= 3)
	{
		UModumateGeometryStatics::GetPlaneFromPoints(ParentMOI->ControlPoints, polyPlane);
	}

	OffSetScreenDistance = offsetDist;
}

void AAdjustmentHandleActor_CPP::SetHandleRotation(const FQuat &r)
{
	SetActorRotation(r);
}

bool AAdjustmentHandleActor_CPP::BeginUse()
{
	if (Implementation)
	{
		// If handle is in use, reset selected widget
		if (Controller)
		{
			Controller->DimStringWidgetSelectedObject = nullptr; // Reset reference to object that was selected from widget
			UWidgetBlueprintLibrary::SetFocusToGameViewport(); // Takes keyboard focus away from any potential widget text field
		}
		return Implementation->OnBeginUse();
	}
	return true;
}

bool AAdjustmentHandleActor_CPP::UpdateUse()
{
	if (Implementation)
	{
		return Implementation->OnUpdateUse();
	}
	return true;
}

bool AAdjustmentHandleActor_CPP::EndUse()
{
	if (Implementation)
	{
		return Implementation->OnEndUse();
	}
	return true;
}

bool AAdjustmentHandleActor_CPP::AbortUse()
{
	if (Implementation)
	{
		return Implementation->OnAbortUse();
	}
	return true;
}

bool AAdjustmentHandleActor_CPP::IsInUse() const
{
	if (Implementation)
	{
		return Implementation->IsInUse();
	}
	return false;
}

bool AAdjustmentHandleActor_CPP::HandleInputNumber(float number)
{
	if (Implementation)
	{
		return Implementation->HandleInputNumber(number);
	}
	return false;
}

bool AAdjustmentHandleActor_CPP::HandleSpacebar()
{
	if (Implementation)
	{
		return Implementation->HandleSpacebar();
	}
	return false;
}

void AAdjustmentHandleActor_CPP::SetEnabled(bool bNewEnabled)
{
	if (bEnabled != bNewEnabled)
	{
		bEnabled = bNewEnabled;

		if (HandleType == EHandleType::Justification)
		{
			if (DynamicMaterial != nullptr && HandleChildren.Num() > 0)
			{
				float showExpandAlpha = bShowHandleChildren ? 1.f : 0.f;
				static const FName expandParamName(TEXT("ShowExpand"));
				DynamicMaterial->SetScalarParameterValue(expandParamName, showExpandAlpha);
			}
		}

		SetActorHiddenInGame(!bEnabled);
		SetActorEnableCollision(bEnabled);
		SetActorTickEnabled(bEnabled);

		if (bEnabled)
		{
			if (Side != INDEX_NONE)
			{
				SetWallHandleSide(Side, ParentMOI, OffSetScreenDistance);
			}

			if (SideCP.Num() > 0)
			{
				SetPolyHandleSide(SideCP, ParentMOI, OffSetScreenDistance);
			}
		}
	}
}

void AAdjustmentHandleActor_CPP::ShowHoverWallDimensionString()
{
	if (bIsPointAdjuster)
	{
		FVector drawFrom = ParentMOI->ControlPoints[0];
		FVector drawTo = ParentMOI->ControlPoints[1];
		if (Side == 2) // 0 = bottom point adjuster. 2 = top point adjuster
		{
			drawFrom += FVector(0.f, 0.f, ParentMOI->Extents.Y);
			drawTo += FVector(0.f, 0.f, ParentMOI->Extents.Y);
		}
		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			drawFrom,
			drawTo,
			ParentMOI->GetObjectRotation().Vector(),
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			ParentMOI->GetActor(),
			EDimStringStyle::HCamera,
			EEnterableField::NonEditableText,
			40.f,
			EAutoEditableBox::Never);
	}
	else
	{
		switch (Side)
		{
		case 0:
		case 1:
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				ParentMOI->ControlPoints[0] + FVector(0.f, 0.f, ParentMOI->Extents.Y / 2.f),
				ParentMOI->ControlPoints[1] + FVector(0.f, 0.f, ParentMOI->Extents.Y / 2.f),
				ParentMOI->GetObjectRotation().Vector(),
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Delta,
				0,
				ParentMOI->GetActor(),
				EDimStringStyle::HCamera,
				EEnterableField::NonEditableText,
				40.f,
				EAutoEditableBox::Never);
			break;
		case 2:
		case 3:
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				(ParentMOI->ControlPoints[0] + ParentMOI->ControlPoints[1]) / 2.f,
				FVector(0.f, 0.f, ParentMOI->Extents.Y) + (ParentMOI->ControlPoints[0] + ParentMOI->ControlPoints[1]) / 2.f,
				Controller->PlayerCameraManager->GetActorRightVector() * FVector(-1.f),
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Delta,
				0,
				ParentMOI->GetActor(),
				EDimStringStyle::Fixed,
				EEnterableField::NonEditableText,
				40.f,
				EAutoEditableBox::Never);
			break;
		}
	}
}

void AAdjustmentHandleActor_CPP::ShowHoverFloorDimensionString(bool ShowCurrentSide)
{
	int32 numTotalCPs = ParentMOI->ControlPoints.Num();

	if (SideCP.Num() == 1) // Create dim string on both side of the point adjust handle
	{
		FVector cornerCP = ParentMOI->ControlPoints[SideCP[0]];
		FVector preCP = ParentMOI->ControlPoints[(SideCP[0] + numTotalCPs - 1) % numTotalCPs];
		FVector postCP = ParentMOI->ControlPoints[(SideCP[0] + 1) % numTotalCPs];

		FVector preEdgeDir = (cornerCP - preCP).GetSafeNormal();
		FVector preEdgeNormal = (preEdgeDir ^ FVector(polyPlane));

		FVector postEdgeDir = (postCP - cornerCP).GetSafeNormal();
		FVector postEdgeNormal = (postEdgeDir ^ FVector(polyPlane));

		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			cornerCP,
			preCP,
			preEdgeNormal,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			ParentMOI->GetActor(),
			EDimStringStyle::Fixed,
			EEnterableField::NonEditableText,
			40.f,
			EAutoEditableBox::Never);

		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			cornerCP,
			postCP,
			postEdgeNormal,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			ParentMOI->GetActor(),
			EDimStringStyle::Fixed,
			EEnterableField::NonEditableText,
			40.f,
			EAutoEditableBox::Never);
	}
	else if (SideCP.Num() == 2) // Create dim string on both side of the stretch handle
	{
		FVector sideCP0 = ParentMOI->ControlPoints[SideCP[0]];
		FVector sideCP1 = ParentMOI->ControlPoints[SideCP[1]];
		FVector preCP0 = ParentMOI->ControlPoints[(SideCP[0] + numTotalCPs - 1) % numTotalCPs];
		FVector postCP1 = ParentMOI->ControlPoints[(SideCP[1] + 1) % numTotalCPs];

		FVector preEdgeDir = (sideCP0 - preCP0).GetSafeNormal();
		FVector preEdgeNormal = (preEdgeDir ^ FVector(polyPlane));

		FVector postEdgeDir = (postCP1 - sideCP1).GetSafeNormal();
		FVector postEdgeNormal = (postEdgeDir ^ FVector(polyPlane));

		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			sideCP0,
			preCP0,
			preEdgeNormal,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			ParentMOI->GetActor(),
			EDimStringStyle::Fixed,
			EEnterableField::NonEditableText,
			40.f,
			EAutoEditableBox::Never);

		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			sideCP1,
			postCP1,
			postEdgeNormal,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			ParentMOI->GetActor(),
			EDimStringStyle::Fixed,
			EEnterableField::NonEditableText,
			40.f,
			EAutoEditableBox::Never);
	}
	// If there's no CP, can we draw Dim. String as vertical extrusion handle?
	else if (FMath::IsNearlyEqual(FMath::Abs(HandleDirection.Z), 1.f), 0.01f)
	{
		FVector camDir = Controller->PlayerCameraManager->GetCameraRotation().Vector();
		camDir.Z = 0.f;
		camDir = camDir.RotateAngleAxis(90.f, FVector::UpVector);
		float height = ParentMOI->Extents.Y;

		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			ParentMOI->GetObjectLocation(),
			ParentMOI->GetObjectLocation() + FVector(0.f, 0.f, height),
			camDir,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Total,
			0,
			ParentMOI->GetActor(),
			EDimStringStyle::Fixed);
	}
}

void AAdjustmentHandleActor_CPP::ShowHoverMetaPlaneDimensionString()
{
	// MetaPlane point adjustment handle uses the same logic as the floor point adjustment handle
	if (SideCP.Num() == 1)
	{
		ShowHoverFloorDimensionString(false);
	}
	else if (SideCP.Num() == 2)
	{
		// Only one dim. string is needed if both dim. strings are parallel and same length
		bool bIsParallel = ShowSideCPParallelDimensionString(false, 0);
		// Draw two dim strings if not parallel or different length
		if (!bIsParallel)
		{
			ShowSideCPDimensionString(0, false, 0);
			ShowSideCPDimensionString(1, false, 0);
		}
	}
}

void AAdjustmentHandleActor_CPP::ShowHoverCabinetDimensionString()
{
	if (SideCP.Num() == 2)
	{
		ShowHoverMetaPlaneDimensionString();
	}
	else
	{
		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			(ParentMOI->ControlPoints[0] + ParentMOI->ControlPoints[1]) / 2.f,
			FVector(0.f, 0.f, ParentMOI->Extents.Y) + (ParentMOI->ControlPoints[0] + ParentMOI->ControlPoints[1]) / 2.f,
			Controller->PlayerCameraManager->GetActorRightVector() * FVector(-1.f),
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			ParentMOI->GetActor(),
			EDimStringStyle::Fixed,
			EEnterableField::NonEditableText,
			40.f,
			EAutoEditableBox::Never);
	}
}

void AAdjustmentHandleActor_CPP::ShowSideCPDimensionString(const int32 sideCPNum, const bool editable, const int32 groupIndex, const float offset)
{
	// There should only be two control points on each side of this handle
	if (SideCP.Num() != 2)
	{
		return;
	}

	// Get location and normal info from control point
	int32 numTotalCPs = ParentMOI->ControlPoints.Num();
	FVector vStart;
	FVector vEnd;
	FVector edgeNormal;

	if (sideCPNum == 0)
	{
		vStart = ParentMOI->ControlPoints[SideCP[0]];
		vEnd = ParentMOI->ControlPoints[(SideCP[0] + numTotalCPs - 1) % numTotalCPs];
		FVector edgeDir = (vStart - vEnd).GetSafeNormal();
		edgeNormal = (edgeDir ^ FVector(polyPlane));
	}
	else if (sideCPNum == 1)
	{
		vStart = ParentMOI->ControlPoints[(SideCP[1] + 1) % numTotalCPs];
		vEnd = ParentMOI->ControlPoints[SideCP[1]];
		FVector edgeDir = (vStart - vEnd).GetSafeNormal();
		edgeNormal = (edgeDir ^ FVector(polyPlane));
	}
	else
	{
		// The handle should only affecting two control points
		return;
	}

	UModumateFunctionLibrary::AddNewDimensionString(
		Controller,
		vStart,
		vEnd,
		edgeNormal,
		Controller->DimensionStringGroupID_Default,
		sideCPNum == 0 ? Controller->DimensionStringUniqueID_SideEdge0 : Controller->DimensionStringUniqueID_SideEdge1,
		groupIndex,
		ParentMOI->GetActor(),
		EDimStringStyle::Fixed,
		EEnterableField::NonEditableText,
		offset,
		editable ? EAutoEditableBox::UponUserInput_SameGroupIndex : EAutoEditableBox::Never,
		true);
}

bool AAdjustmentHandleActor_CPP::ShowSideCPParallelDimensionString(const bool editable, const int32 groupIndex, const float offset, const float parallelThreshold)
{
	int32 numTotalCPs = ParentMOI->ControlPoints.Num();

	FVector sideCP0 = ParentMOI->ControlPoints[SideCP[0]];
	FVector sideCP1 = ParentMOI->ControlPoints[SideCP[1]];
	FVector preCP0 = ParentMOI->ControlPoints[(SideCP[0] + numTotalCPs - 1) % numTotalCPs];
	FVector postCP1 = ParentMOI->ControlPoints[(SideCP[1] + 1) % numTotalCPs];

	FVector preEdgeDir = (sideCP0 - preCP0).GetSafeNormal();
	FVector preEdgeNormal = (preEdgeDir ^ FVector(polyPlane));

	FVector postEdgeDir = (postCP1 - sideCP1).GetSafeNormal();
	FVector postEdgeNormal = (postEdgeDir ^ FVector(polyPlane));

	// Draw only one dim string if the sides are parallel and same length
	if (FVector::Parallel(preEdgeNormal, postEdgeNormal, parallelThreshold)
		&& FMath::IsNearlyEqual((sideCP0 - preCP0).Size(), (sideCP1 - postCP1).Size(), 1.f))
	{
		FVector averageStart = (sideCP0 + sideCP1) * 0.5f;
		FVector averageEnd = (preCP0 + postCP1) * 0.5f;

		FVector dimDir = FVector::CrossProduct(ParentMOI->GetNormal(), (averageEnd - averageStart));

		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			averageStart,
			averageEnd,
			dimDir.GetSafeNormal(),
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Total,
			groupIndex,
			ParentMOI->GetActor(),
			EDimStringStyle::HCamera,
			EEnterableField::NonEditableText,
			offset,
			editable ? EAutoEditableBox::UponUserInput_SameGroupIndex : EAutoEditableBox::Never,
			true);

		return true;
	}

	return false;
}

