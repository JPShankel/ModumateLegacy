// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelDatasmithRuntimeActor.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"
#include "DatasmithRuntimeBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
AEditModelDatasmithRuntimeActor::AEditModelDatasmithRuntimeActor()
{
}

void AEditModelDatasmithRuntimeActor::OnImportEnd()
{
	Super::OnImportEnd();

	TArray<AActor*> attachedActors;
	GetAttachedActorsRecursive(this, attachedActors);

	// Search for all StaticMeshComponents created and attached from import
	for (auto curActor : attachedActors)
	{
		// The attached actors can be StaticMeshActor or UActor
		// Search for StaticMeshComponents using different methods
		AStaticMeshActor* curStaticMeshActor = Cast<AStaticMeshActor>(curActor);
		TArray<UStaticMeshComponent*> staticMeshComps;
		if (curStaticMeshActor)
		{
			staticMeshComps.Add(curStaticMeshActor->GetStaticMeshComponent());
		}
		else
		{
			TArray<UActorComponent*> actorComps;
			curActor->GetComponents<UActorComponent>(actorComps);
			for (auto curActorComp : actorComps)
			{
				UStaticMeshComponent* asMeshComp = Cast<UStaticMeshComponent>(curActorComp);
				if (asMeshComp)
				{
					staticMeshComps.Add(asMeshComp);
				}
			}
		}

		// Cache the StaticMeshes and their transforms
		for (const auto curStaticMeshComp : staticMeshComps)
		{
			UStaticMesh* mesh = curStaticMeshComp->GetStaticMesh();
			if (mesh)
			{
				StaticMeshRefs.Add(mesh);
				StaticMeshTransforms.Add(curStaticMeshComp->GetComponentTransform());
			}
		}
		curActor->SetActorHiddenInGame(true);
		curActor->SetActorEnableCollision(false);
	}

	if (DatasmithImporter.IsValid())
	{
		DatasmithImporter->OnRuntimeActorImportDone(this);
	}
}

bool AEditModelDatasmithRuntimeActor::MakeFromImportFilePath()
{
	return UDatasmithRuntimeLibrary::LoadFile(this, ImportFilePath);
}

void AEditModelDatasmithRuntimeActor::GetAttachedActorsRecursive(AActor* ParentActor, TArray<AActor*>& OutActors)
{
	TArray<AActor*> attachedActors;
	ParentActor->GetAttachedActors(attachedActors);
	for (AActor* attached : attachedActors)
	{
		if (!OutActors.Contains(attached))
		{
			GetAttachedActorsRecursive(attached, OutActors);
			OutActors.Add(attached);
		}
	}
}
