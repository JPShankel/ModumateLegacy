// Copyright 2018 Modumate, Inc. All Rights Reserved.
#include "UnrealClasses/LineActor.h"

#include "Graph/Graph3D.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ProceduralMeshComponent.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

// Sets default values
ALineActor::ALineActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Color(FColor::Black)
	, Thickness(1.0f)
{
	SetMobility(EComponentMobility::Movable);
}

// Called when the game starts or when spawned
void ALineActor::BeginPlay()
{
	Super::BeginPlay();

	if (auto *playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>())
	{
		EMPlayerState = playerController->EMPlayerState;
		EMGameMode = EMPlayerState->GetEditModelGameMode();
		EMPlayerHUD = playerController->GetEditModelHUD();

		SetIsHUD(bIsHUD);
	}
}

void ALineActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EMPlayerHUD)
	{
		EMPlayerHUD->All3DLineActors.RemoveSingleSwap(this, false);
	}

	Super::EndPlay(EndPlayReason);
}

void ALineActor::UpdateColor()
{
	if (auto meshComp = GetStaticMeshComponent())
	{
		meshComp->SetCustomPrimitiveDataVector4(0, FVector4(Color.R, Color.G, Color.B, Color.A) / 255.0f);
	}
}

bool ALineActor::MakeGeometry()
{
	if (auto meshComp = GetStaticMeshComponent())
	{
		if (!EMGameMode)
		{
			return false;
		}

		meshComp->SetStaticMesh(EMGameMode->LineMesh);
		FArchitecturalMaterial materialData;
		materialData.EngineMaterial = EMGameMode->LineMaterial;
		meshComp->SetMaterial(0, materialData.EngineMaterial.Get());
		meshComp->SetCastShadow(false);

		// TODO: without this line, there is a flickering issue where the cpu occlusion 
		// is conflicting with material rendering.  With a large bounds scale, the cpu occlusion
		// will not hide any of the line actors.
		meshComp->SetBoundsScale(10000.0f);
	}

	UpdateTransform();
	UpdateColor();

	return true;
}

bool ALineActor::UpdateTransform()
{
	SetActorLocation((Point1 + Point2) / 2.0f);

	FVector direction = (Point2 - Point1).GetSafeNormal();
	SetActorRotation(FRotationMatrix::MakeFromX(direction).ToQuat());

	float size = (Point2 - Point1).Size();
	// this constant needs to be applied to the thickness here because it was
	// not effective in the material
	SetActorScale3D(FVector(size, Thickness * 0.0005f, 1.0f));

	return true;
}

void ALineActor::SetIsHUD(bool bRenderScreenSpace)
{
	if (!bRenderScreenSpace)
	{
		if (EMPlayerHUD)
		{
			EMPlayerHUD->All3DLineActors.RemoveSingleSwap(this, false);
		}
		bIsHUD = bRenderScreenSpace;
		MakeGeometry();
	}
	else
	{
		EMPlayerHUD->All3DLineActors.Add(this);
	}
}

void ALineActor::SetVisibilityInApp(bool bVisible)
{
	bVisibleInApp = bVisible;
	SetActorHiddenInGame(!bVisibleInApp);
}

void ALineActor::UpdateVisuals(bool bConnected, float ThicknessMultiplier, FColor NewColor)
{
	float newThickness = 2.0f * ThicknessMultiplier;
	bool bUpdateThickness = newThickness != Thickness;
	bool bUpdateColor = Color != NewColor;

	if (bUpdateColor)
	{
		Color = NewColor;
		UpdateColor();
	}
	if (bUpdateThickness)
	{
		Thickness = newThickness;
		if (!bIsHUD)
		{
			UpdateTransform();
		}
	}
}

