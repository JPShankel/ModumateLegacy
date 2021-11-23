// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/PointHosted.h"
#include "UnrealClasses/CompoundMeshActor.h"

AMOIPointHosted::AMOIPointHosted()
	: AModumateObjectInstance()
{}

AActor* AMOIPointHosted::CreateActor(const FVector& loc, const FQuat& rot)
{
	return GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
}

void AMOIPointHosted::SetupDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIPointHosted::UpdateDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIPointHosted::InternalUpdateGeometry(bool bCreateCollision)
{
	const AModumateObjectInstance* parentObj = GetParentObject();
	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (parentObj && cma)
	{
		FVector parentLocation = parentObj->GetLocation();
		FQuat parentRotation = parentObj->GetRotation();

		if (CachedAssembly.Parts.Num() > 0)
		{
			cma->MakeFromAssembly(CachedAssembly, FVector::OneVector, false, bCreateCollision);
			FTransform cmaTransform;
			cmaTransform.SetLocation(parentLocation);
			cmaTransform.SetRotation(parentRotation);

			cma->SetActorTransform(cmaTransform);
		}
	}
}
