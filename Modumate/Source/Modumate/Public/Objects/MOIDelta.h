// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/DocumentDelta.h"
#include "Objects/MOIState.h"

#include "MOIDelta.generated.h"

class FModumateObjectInstance;

/**
 * A pair of MOI states, and the desired operation, so that a single MOI delta can perform multiple operations
 */
USTRUCT()
struct MODUMATE_API FMOIDeltaState
{
	GENERATED_BODY()

	UPROPERTY()
	FMOIStateData OldState;

	UPROPERTY()
	FMOIStateData NewState;

	UPROPERTY()
	EMOIDeltaType DeltaType = EMOIDeltaType::None;
};

class MODUMATE_API FMOIDelta : public FDocumentDelta
{
public:
	FMOIDelta() {}

	void AddCreateDestroyState(const FMOIStateData& State, EMOIDeltaType DeltaType);
	void AddCreateDestroyStates(const TArray<FMOIStateData>& InStates, EMOIDeltaType DeltaType);
	FMOIStateData& AddMutationState(const FModumateObjectInstance* Object);
	void AddMutationState(const FModumateObjectInstance* Object, const FMOIStateData& OldState, const FMOIStateData& NewState);
	void AddMutationStates(const TArray<FModumateObjectInstance*>& Objects);

	virtual bool ApplyTo(FModumateDocument* doc, UWorld* world) const override;
	virtual FDeltaPtr MakeInverse() const override;

	TArray<FMOIDeltaState> States;
};
