// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/DocumentDelta.h"
#include "Objects/MOIState.h"

class FModumateObjectInstance;

class MODUMATE_API FMOIDelta : public FDocumentDelta
{
public:

	FMOIDelta() {}
	FMOIDelta(const FMOIStateData& State);
	FMOIDelta(const TArray<FMOIStateData>& States);
	FMOIDelta(FModumateObjectInstance* Object);
	FMOIDelta(const TArray<FModumateObjectInstance*>& Objects);

	void AddCreateDestroyState(const FMOIStateData& State);
	void AddCreateDestroyStates(const TArray<FMOIStateData>& States);
	void AddMutationState(FModumateObjectInstance* Object);
	void AddMutationStates(const TArray<FModumateObjectInstance*>& Objects);

	virtual bool ApplyTo(FModumateDocument* doc, UWorld* world) const override;
	virtual FDeltaPtr MakeInverse() const override;

	typedef TPair<FMOIStateData, FMOIStateData> FStatePair;
	TArray<FStatePair> StatePairs;
};
