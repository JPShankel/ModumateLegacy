// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MOIDelta.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateSymbolDeltaStatics.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"


bool FMOIDelta::IsValid() const
{
	return States.Num() > 0;
}

void FMOIDelta::AddCreateDestroyState(const FMOIStateData& State, EMOIDeltaType DeltaType)
{
	if ((DeltaType == EMOIDeltaType::Create) || (DeltaType == EMOIDeltaType::Destroy))
	{
		ensureMsgf(State.CustomData.IsValid(), TEXT("Invalid CustomData in MOIStateData"));
		States.Add(FMOIDeltaState{ State, State, DeltaType });
	}
}

void FMOIDelta::AddCreateDestroyStates(const TArray<FMOIStateData>& InStates, EMOIDeltaType DeltaType)
{
	for (auto& state : InStates)
	{
		AddCreateDestroyState(state, DeltaType);
	}
}

FMOIStateData& FMOIDelta::AddMutationState(const AModumateObjectInstance* Object)
{
	auto& statePair = States.Add_GetRef(FMOIDeltaState{ Object->StateData, Object->StateData, EMOIDeltaType::Mutate });
	return statePair.NewState;
}

void FMOIDelta::AddMutationState(const AModumateObjectInstance* Object, const FMOIStateData& OldState, const FMOIStateData& NewState)
{
	if (OldState != NewState)
	{
		States.Add(FMOIDeltaState{ OldState, NewState, EMOIDeltaType::Mutate });
	}
}

void FMOIDelta::AddMutationStates(const TArray<AModumateObjectInstance*>& Objects)
{
	for (auto* ob : Objects)
	{
		if (ensure(ob))
		{
			AddMutationState(ob);
		}
	}
}

bool FMOIDelta::ApplyTo(UModumateDocument* doc, UWorld* world) const
{
	return doc->ApplyMOIDelta(*this, world);
}

FDeltaPtr FMOIDelta::MakeInverse() const
{
	auto inverse = MakeShared<FMOIDelta>();

	for (auto& deltaState : States)
	{
		EMOIDeltaType inverseDeltaType = EMOIDeltaType::None;
		switch (deltaState.DeltaType)
		{
		case EMOIDeltaType::Mutate:
			inverseDeltaType = EMOIDeltaType::Mutate;
			break;
		case EMOIDeltaType::Create:
			inverseDeltaType = EMOIDeltaType::Destroy;
			break;
		case EMOIDeltaType::Destroy:
			inverseDeltaType = EMOIDeltaType::Create;
			break;
		}
		inverse->States.Add(FMOIDeltaState{ deltaState.NewState, deltaState.OldState, inverseDeltaType });
	}

	return inverse;
}

FStructDataWrapper FMOIDelta::SerializeStruct()
{
	for (FMOIDeltaState& statePair : States)
	{
		statePair.OldState.CustomData.SaveJsonFromCbor();
		statePair.NewState.CustomData.SaveJsonFromCbor();
	}

	return FStructDataWrapper(StaticStruct(), this, true);
}

void FMOIDelta::GetAffectedObjects(TArray<TPair<int32, EMOIDeltaType>>& OutAffectedObjects) const
{
	Super::GetAffectedObjects(OutAffectedObjects);

	for (const FMOIDeltaState& statePair : States)
	{
		OutAffectedObjects.Add(TPair<int32, EMOIDeltaType>(statePair.OldState.ID, statePair.DeltaType));
	}
}

void FMOIDelta::GetDerivedDeltas(UModumateDocument* Doc, EMOIDeltaType DeltaOperation, TArray<TSharedPtr<FDocumentDelta>>& OutDeltas) const
{
	for (const auto& deltaState : States)
	{
		FModumateSymbolDeltaStatics::CreateSymbolDerivedDeltasForMoi(Doc, deltaState, DeltaOperation, OutDeltas);
	}
}
