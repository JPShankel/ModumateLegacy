// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"

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
