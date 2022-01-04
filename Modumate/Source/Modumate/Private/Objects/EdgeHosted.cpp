// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/EdgeHosted.h"
#include "UnrealClasses/CompoundMeshActor.h"

FMOIEdgeHostedData::FMOIEdgeHostedData()
{}

AMOIEdgeHosted::AMOIEdgeHosted()
	: AModumateObjectInstance()
{}

AActor* AMOIEdgeHosted::CreateActor(const FVector& loc, const FQuat& rot)
{
	return GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
}

void AMOIEdgeHosted::SetupDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIEdgeHosted::UpdateDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIEdgeHosted::InternalUpdateGeometry(bool bCreateCollision)
{
	const AModumateObjectInstance* parentObj = GetParentObject();
	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (parentObj && cma)
	{
		FVector lineStartPos = parentObj->GetCorner(0);
		FVector lineEndPos = parentObj->GetCorner(1);
		FVector lineDir = (lineEndPos - lineStartPos).GetSafeNormal();

		// Obj pivot at midpoint
		FVector cmaLocation = (lineStartPos + lineEndPos) / 2.f;

		// Obj dir
		FVector lineBasisX, lineBasisY;
		UModumateGeometryStatics::FindBasisVectors(lineBasisX, lineBasisY, lineDir);
		FQuat cmaRotation = FRotationMatrix::MakeFromXY(lineBasisX, lineBasisY).ToQuat();

		// Obj scale
		float lineLength = (lineStartPos - lineEndPos).Size();
		float cmaLength = CachedAssembly.GetCompoundAssemblyNativeSize().Z;
		float lengthScale = lineLength / cmaLength;
		FVector cmaScale = FVector(lengthScale);

		if (CachedAssembly.Parts.Num() > 0)
		{
			cma->MakeFromAssembly(CachedAssembly, cmaScale, false, bCreateCollision);
			FTransform cmaTransform;
			cmaTransform.SetLocation(cmaLocation);
			cmaTransform.SetRotation(cmaRotation);
			//Scale already done in MakeFromAssembly() at component level
			cmaTransform.SetScale3D(FVector::OneVector);
			cma->SetActorTransform(cmaTransform);
		}
	}
}