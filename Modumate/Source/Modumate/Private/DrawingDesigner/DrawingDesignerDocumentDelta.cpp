// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"

FDrawingDesignerDocumentDelta::FDrawingDesignerDocumentDelta(const FDrawingDesignerDocument& doc,
	FDrawingDesignerJsDeltaPackage package
) : FDocumentDelta()
{
	this->from = doc;
	this->to = doc;

	for (FDrawingDesignerJsDelta& delta : package.deltas)
	{
		delta.details.id = delta.header.id;
		delta.details.parent = delta.header.parent;

		bool appliedCorrectly = ParseDeltaVerb(delta);

		if (!appliedCorrectly)
		{
			UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Failed to apply drawing designer delta package, noop'ing delta"));
			this->to = this->from;
		}
	}
}

bool FDrawingDesignerDocumentDelta::ApplyTo(UModumateDocument* Doc, UWorld* World) const
{
	if (Doc)
	{
		Doc->DrawingDesignerDocument = this->to;
		Doc->DrawingDesignerDocument.bDirty = true;
		return true;
	}

	return false;
}

TSharedPtr<FDocumentDelta> FDrawingDesignerDocumentDelta::MakeInverse() const
{
	TSharedPtr<FDrawingDesignerDocumentDelta> inverted = MakeShareable(new FDrawingDesignerDocumentDelta());
	if (inverted)
	{
		inverted->from = this->to;
		inverted->to = this->from;
		return inverted;
	}
	return NULL;
}

FStructDataWrapper FDrawingDesignerDocumentDelta::SerializeStruct()
{ 
	return FStructDataWrapper(StaticStruct(), this, true); 
}

bool FDrawingDesignerDocumentDelta::ParseDeltaVerb(FDrawingDesignerJsDelta& delta)
{
	bool rtn = true;
	if (rtn)
	{
		switch (delta.header.verb)
		{
		case EDeltaVerb::add:
		{
			if (delta.details.id == INDEX_NONE)
			{
				delta.details.id = to.GetAndIncrDrawingId();
			}

			rtn = to.Add(delta.details);
			break;
		}
		case EDeltaVerb::remove:
		{
			FString id = FString::FromInt(delta.header.id);
			rtn = to.Remove(id);
			break;
		}
		case EDeltaVerb::modify:
		{
			FString id = FString::FromInt(delta.header.id);
			rtn = to.Modify(delta.details);
			break;
		}
		default:
			rtn = false;
			break;
		}
	}

	return rtn;
}

bool FDrawingDesignerJsDeltaPackage::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerJsDeltaPackage>(InJson, this);
}

