// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/Terrain.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2DVertex.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/DynamicTerrainActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"


AMOITerrain::AMOITerrain()
{

}

FVector AMOITerrain::GetCorner(int32 index) const
{
	return FVector::ZeroVector;
}

int32 AMOITerrain::GetNumCorners() const
{
	return InstanceData.Heights.Num();
}

FVector AMOITerrain::GetNormal() const
{
	return FVector::UpVector;
}

FVector AMOITerrain::GetLocation() const
{
	return InstanceData.Origin;
}

FQuat AMOITerrain::GetRotation() const
{
	return FQuat::Identity;
}

bool AMOITerrain::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		SetupTerrainMaterial();
		UpdateTerrainActor();
		break;
	}

	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();

	default:
		break;
	}

	return true;
}

bool AMOITerrain::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	if (!Super::GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled))
	{
		return false;
	}

	ADynamicTerrainActor* actor = Cast<ADynamicTerrainActor>(GetActor());
	if (!ensure(actor))
	{
		return false;
	}

	return true;
}

void AMOITerrain::UpdateTerrainActor()
{
	// TODO: Each uncontained polygon in the graph should have its own actor, instead
	// of assuming just one terrain patch per MOI.
	const auto graph2d = GetDocument()->FindSurfaceGraph(ID);
	if (ensure(graph2d))
	{
		ADynamicTerrainActor* actor = Cast<ADynamicTerrainActor>(GetActor());
		if (!ensure(actor))
		{
			return;
		}

		const TMap<int32, Modumate::FGraph2DVertex>& vertices = graph2d->GetVertices();
		TArray<FVector> perimeterPoints;
		TArray<FVector2D> perimeterPoints2D;
		TArray<FVector> heightPoints;

		if (vertices.Num() != InstanceData.Heights.Num())
		{
			return;
		}

		for (const auto& vertexKvp: vertices)
		{
			float height = InstanceData.Heights[vertexKvp.Key];

			FVector2D position2d = vertexKvp.Value.Position;
			FVector planePosition = InstanceData.Origin + position2d.X * FVector::ForwardVector + position2d.Y * FVector::RightVector;
			FVector position = planePosition + height * FVector::UpVector;

			perimeterPoints.Add(planePosition);
			perimeterPoints2D.Add(FVector2D(planePosition));
			heightPoints.Add(position);
		}

		auto* gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();

		if (perimeterPoints.Num() >= 3)
		{
			actor->TestSetupTerrainGeometryGTE(perimeterPoints2D, heightPoints, TArray<FVector2D>(), false);
			actor->GrassMesh->BuildTreeIfOutdated(false, true);
		}
	}
}

bool AMOITerrain::SetupTerrainMaterial()
{
	ADynamicTerrainActor* actor = Cast<ADynamicTerrainActor>(GetActor());
	if (!ensure(actor))
	{
		return false;
	}

	auto* gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	UMaterialInstanceDynamic* dynamicMat = Cast<UMaterialInstanceDynamic>(actor->Mesh->GetMaterial(0));
	if (dynamicMat == nullptr)
	{
		dynamicMat = UMaterialInstanceDynamic::Create(gameMode->TerrainMaterial, actor->Mesh);
		actor->TerrainMaterial = dynamicMat;

		actor->GrassStaticMesh = gameMode->GrassMesh;
		UMaterialInstance* grassMat = Cast<UMaterialInstance>(actor->GrassMesh->GetMaterial(0));
		if (grassMat == nullptr)
		{
			actor->GrassMesh->SetMaterial(0, gameMode->GrassMaterial);
		}

	}

	return true;
}

AActor* AMOITerrain::CreateActor(const FVector& loc, const FQuat& rot)
{
	ADynamicTerrainActor* actor = GetWorld()->SpawnActor<ADynamicTerrainActor>(ADynamicTerrainActor::StaticClass(), FTransform(rot, loc));
	return actor;
}
