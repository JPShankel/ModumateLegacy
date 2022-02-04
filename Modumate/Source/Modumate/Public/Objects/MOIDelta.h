// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/DocumentDelta.h"
#include "Objects/MOIState.h"

#include "MOIDelta.generated.h"

class AModumateObjectInstance;

UENUM()
enum class EMOIDeltaType : uint8
{
	None = 0,
	Mutate,
	Create,
	Destroy
};

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

USTRUCT()
struct MODUMATE_API FMOIDelta : public FDocumentDelta
{
	GENERATED_BODY()

	virtual ~FMOIDelta() {}

	bool IsValid() const;

	void AddCreateDestroyState(const FMOIStateData& State, EMOIDeltaType DeltaType);
	void AddCreateDestroyStates(const TArray<FMOIStateData>& InStates, EMOIDeltaType DeltaType);
	FMOIStateData& AddMutationState(const AModumateObjectInstance* Object);
	void AddMutationState(const AModumateObjectInstance* Object, const FMOIStateData& OldState, const FMOIStateData& NewState);
	void AddMutationStates(const TArray<AModumateObjectInstance*>& Objects);

	virtual bool ApplyTo(UModumateDocument* doc, UWorld* world) const override;
	virtual FDeltaPtr MakeInverse() const override;
	virtual FStructDataWrapper SerializeStruct() override;
	virtual void GetAffectedObjects(TArray<TPair<int32, EMOIDeltaType>>& OutAffectedObjects) const override;

	UPROPERTY()
	TArray<FMOIDeltaState> States;
};
