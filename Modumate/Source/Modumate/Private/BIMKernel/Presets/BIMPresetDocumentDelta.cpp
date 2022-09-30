// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateSymbolDeltaStatics.h"


bool FBIMPresetDelta::ApplyTo(UModumateDocument* Doc, UWorld* World) const
{
	return Doc->ApplyPresetDelta(*this, World);
}

TSharedPtr<FDocumentDelta> FBIMPresetDelta::MakeInverse() const
{
	TSharedPtr<FBIMPresetDelta> inverse = MakeShared<FBIMPresetDelta>();
	inverse->OldState = NewState;
	inverse->NewState = OldState;
	return inverse;
}

FStructDataWrapper FBIMPresetDelta::SerializeStruct() 
{ 
	return FStructDataWrapper(StaticStruct(), this, true); 
}

void FBIMPresetDelta::GetDerivedDeltas(UModumateDocument* Doc, EMOIDeltaType DeltaOperation, TArray<TSharedPtr<FDocumentDelta>>& OutDeltas) const
{
	// Only check for Preset deletion:
	if (DeltaOperation == EMOIDeltaType::Mutate && OldState.NodeScope == EBIMValueScope::Symbol
		&& !NewState.GUID.IsValid())
	{
		FModumateSymbolDeltaStatics::GetDerivedDeltasForPresetDelta(Doc, this, DeltaOperation, OutDeltas);
	}
}
