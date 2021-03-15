// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/VertexBase.h"

#include "Components/StaticMeshComponent.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"

AMOIVertexBase::AMOIVertexBase()
	: Super()
	, SelectedColor(0x00, 0x35, 0xFF)
	, BaseColor(0x00, 0x00, 0x00)
	, CurColor(0x00, 0x00, 0x00)
	, DefaultScreenSize(4.0f)
	, HoveredScreenSize(6.0f)
	, CurScreenSize(4.0f)
{
	VertexMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VertexMeshComp"));
	SetRootComponent(VertexMeshComp);
}

FVector AMOIVertexBase::GetCorner(int32 index) const
{
	ensure(index == 0);
	return GetLocation();
}

int32 AMOIVertexBase::GetNumCorners() const
{
	return 1;
}

void AMOIVertexBase::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	UModumateObjectStatics::GetNonPhysicalEnabledFlags(this, bOutVisible, bOutCollisionEnabled);

	SetActorHiddenInGame(!bOutVisible);
	SetActorTickEnabled(bOutVisible);
	SetActorEnableCollision(bOutCollisionEnabled);
}

void AMOIVertexBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	TArray<FVector> vertexTangents;
	GetTangents(vertexTangents);

	// TODO: support multiple axis affordances for a single point
	FVector defaultTangent = (vertexTangents.Num() > 0) ? vertexTangents[0] : FVector::ZeroVector;

	outPoints.Add(FStructurePoint(GetLocation(), defaultTangent, 0));
}

AActor *AMOIVertexBase::CreateActor(const FVector &loc, const FQuat &rot)
{
	SetActorLocation(loc);
	return this;
}

bool AMOIVertexBase::OnHovered(AEditModelPlayerController* controller, bool bNewHovered)
{
	if (!Super::OnHovered(controller, bNewHovered))
	{
		return false;
	}

	CurScreenSize = bHovered ? HoveredScreenSize : DefaultScreenSize;
	UpdateVertexMesh();

	return true;
}

bool AMOIVertexBase::OnSelected(bool bIsSelected)
{
	if (!Super::OnSelected(bIsSelected))
	{
		return false;
	}

	CurScreenSize = bIsSelected ? HoveredScreenSize : DefaultScreenSize;
	CurColor = bIsSelected ? SelectedColor : BaseColor;
	UpdateVertexMesh();

	return true;
}

void AMOIVertexBase::GetTangents(TArray<FVector>& OutTangents) const
{
}

void AMOIVertexBase::BeginPlay()
{
	Super::BeginPlay();

	auto gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	VertexMeshComp->SetStaticMesh(gameMode->VertexMesh);
	VertexMeshComp->SetMaterial(0, gameMode->VertexMaterial);

	VertexMeshComp->SetCollisionObjectType(COLLISION_HANDLE);

	// Allow outline to be draw over the handle mesh
	VertexMeshComp->SetRenderCustomDepth(true);
	VertexMeshComp->SetCustomDepthStencilValue(1);

	// Disable shadows because they are expensive, and there are a lot of these things
	VertexMeshComp->SetCastShadow(false);

	CurColor = BaseColor;
	CurScreenSize = DefaultScreenSize;
	UpdateVertexMesh();
}

void AMOIVertexBase::UpdateVertexMesh()
{
	FLinearColor nonSRGBColor = CurColor.ReinterpretAsLinear();
	VertexMeshComp->SetCustomPrimitiveDataVector4(0, FVector4(nonSRGBColor.R, nonSRGBColor.G, nonSRGBColor.B, CurScreenSize));
}
