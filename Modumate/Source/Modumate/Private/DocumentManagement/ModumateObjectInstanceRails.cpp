#include "ModumateObjectInstanceRails.h"
#include "EditModelAdjustmentHandleBase.h"
#include "EditModelPlayerController_CPP.h"
#include "AdjustmentHandleActor_CPP.h"
#include "HUDDrawWidget_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelGameState_CPP.h"


namespace Modumate
{
	FMOIRailImpl::FMOIRailImpl(FModumateObjectInstance *moi) : FDynamicModumateObjectInstanceImpl(moi) {};

	FMOIRailImpl::~FMOIRailImpl() {}

	// Dynamic geometry
	void FMOIRailImpl::SetupDynamicGeometry()
	{
		AEditModelGameState_CPP *gameState = DynamicMeshActor->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		Modumate::FModumateDocument *doc = &gameState->Document;
		MOI->Extents.Y = doc->GetDefaultRailHeight();
		DynamicMeshActor->SetupRailGeometry(MOI->ControlPoints, doc->GetDefaultRailHeight());
	}

	void FMOIRailImpl::UpdateDynamicGeometry()
	{
		DynamicMeshActor->UpdateRailGeometry(MOI->ControlPoints, 100.f);
	}

	void FMOIRailImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		int32 numRailPoints = MOI->ControlPoints.Num();

		for (int32 i = 0; i < numRailPoints; i += 2)
		{
			FVector dh = FVector(0, 0, MOI->Extents.Y);
			FVector dir = (MOI->ControlPoints[i + 1] - MOI->ControlPoints[i]).GetSafeNormal();

			outPoints.Add(FStructurePoint(MOI->ControlPoints[i], dir, i));
			outPoints.Add(FStructurePoint(MOI->ControlPoints[i + 1], dir, i + 1));
			outPoints.Add(FStructurePoint(MOI->ControlPoints[i] + dh, dir, i));
			outPoints.Add(FStructurePoint(MOI->ControlPoints[i + 1] + dh, dir, i + 1));

			outLines.Add(FStructureLine(MOI->ControlPoints[i], MOI->ControlPoints[i + 1], i, i + 1));
			outLines.Add(FStructureLine(MOI->ControlPoints[i] + dh, MOI->ControlPoints[i + 1] + dh, i, i + 1));
			outLines.Add(FStructureLine(MOI->ControlPoints[i], MOI->ControlPoints[i] + dh, i, i));
			outLines.Add(FStructureLine(MOI->ControlPoints[i + 1], MOI->ControlPoints[i + 1] + dh, i + 1, i + 1));
		}
	}

	void FMOIRailImpl::InvertObject() {}

	TArray<FModelDimensionString> FMOIRailImpl::GetDimensionStrings() const
	{
		TArray<FModelDimensionString> ret;
		for (size_t i = 0, iend = MOI->ControlPoints.Num(); i < iend; i += 2)
		{
			FModelDimensionString ds;
			ds.AngleDegrees = 0;
			ds.Point1 = MOI->ControlPoints[i];
			ds.Point2 = MOI->ControlPoints[i + 1];
			ds.Functionality = EEnterableField::None;
			ds.Offset = 50;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ds.Owner = MOI->GetActor();
			ret.Add(ds);
		}
		return ret;
	}


	void FMOIRailImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		if (AdjustmentHandles.Num() > 0)
		{
			return;
		}
		auto makeActor = [this, controller](IAdjustmentHandleImpl *impl, UStaticMesh *mesh, const FVector &s, const TArray<int32>& CP)
		{
			AAdjustmentHandleActor_CPP *actor = DynamicMeshActor->GetWorld()->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			actor->SetActorMesh(mesh);

#if 0
			actor->SetHandleScale(FVector(1, 1, 1));
			actor->SetHandleScaleScreenSize(FVector(1, 1, 1));
			actor->SetHandleScale(s);
			actor->SetHandleScaleScreenSize(s);
#endif

			impl->Handle = actor;
			actor->Implementation = impl;
			actor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepRelativeTransform);
			actor->SetActorScale3D(s);
			AdjustmentHandles.Add(TWeakObjectPtr<AAdjustmentHandleActor_CPP>(actor));
		};

		AEditModelGameMode_CPP *gameMode = controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
	}
}
