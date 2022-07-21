// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"

FDrawingDesignerDocumentDelta::FDrawingDesignerDocumentDelta(const FDrawingDesignerDocument& Doc,
	FDrawingDesignerJsDeltaPackage Package
) : FDocumentDelta(), package(Package)
{
	
}

FDrawingDesignerJsDelta FDrawingDesignerDocumentDelta::GetInverted(const FDrawingDesignerJsDelta& Delta) const
{
	FDrawingDesignerJsDelta inverted = Delta; //Copy
	inverted.details = inverted.reverse;
	inverted.header.id = inverted.reverse.id;
	inverted.header.parent = inverted.reverse.parent;
	switch (Delta.header.verb)
	{
	case EDeltaVerb::add:
		inverted.header.verb = EDeltaVerb::remove;
		break;
	case EDeltaVerb::remove:
		inverted.header.verb = EDeltaVerb::add;
		break;
	case EDeltaVerb::modify:
		inverted.header.verb = EDeltaVerb::modify;
		break;
	case EDeltaVerb::unknown:
		break;
	}

	return inverted;
}

bool FDrawingDesignerDocumentDelta::ApplyTo(UModumateDocument* Doc, UWorld* World) const
{
	bool appliedCorrectly = false;
	if (Doc)
	{
		FDrawingDesignerDocument copy = Doc->DrawingDesignerDocument;

		//KLUDGE: Breaking const here because unlike typical deltas,
		// we do NOT know our ID until we apply. And we need to
		// store the ids we apply so that we can invert it later
		// if needed. The application of a DD id is done inside
		// ParseDeltaVerb (which is not const) -JN
		auto* me = const_cast<FDrawingDesignerDocumentDelta*>(this);
		for (FDrawingDesignerJsDelta& delta : me->package.deltas)
		{
			delta.details.id = delta.header.id;
			delta.details.parent = delta.header.parent;

			appliedCorrectly = ParseDeltaVerb(&copy, delta);

			if (!appliedCorrectly)
			{
				UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Failed to apply drawing designer delta package"));
				break;
			}
		}
		if(appliedCorrectly)
		{
			Doc->DrawingDesignerDocument = copy;
			Doc->DrawingDesignerDocument.bDirty = true;
		}
	}

	return appliedCorrectly;
}

TSharedPtr<FDocumentDelta> FDrawingDesignerDocumentDelta::MakeInverse() const
{
	TSharedPtr<FDrawingDesignerDocumentDelta> inverted = MakeShareable(new FDrawingDesignerDocumentDelta());
	if (inverted)
	{
		//Flip deltas array
		TArray<FDrawingDesignerJsDelta> flipped = this->package.deltas;
		Algo::Reverse(flipped);
		
		for(const auto& delta : flipped)
		{
			//Get the inverse of the delta
			auto invertedDelta = GetInverted(delta);
			//push on to new package deltas array
			inverted->package.deltas.Push(invertedDelta);
		}

		return inverted;
	}
	return NULL;
}

FStructDataWrapper FDrawingDesignerDocumentDelta::SerializeStruct()
{ 
	return FStructDataWrapper(StaticStruct(), this, true); 
}

bool FDrawingDesignerDocumentDelta::ParseDeltaVerb(FDrawingDesignerDocument* DDoc, FDrawingDesignerJsDelta& Delta)
{
	bool rtn = true;
	if (rtn)
	{
		switch (Delta.header.verb)
		{
		case EDeltaVerb::add:
		{
			if (Delta.details.id == INDEX_NONE)
			{
				Delta.details.id = DDoc->GetAndIncrDrawingId();
			}
				
			rtn = DDoc->Add(Delta.details);
			FString id = FString::FromInt(Delta.details.id);
			if(DDoc->nodes.Contains(id))
			{
				Delta.reverse = DDoc->nodes[id];
			}
			break;
		}
		case EDeltaVerb::remove:
		{
			FString id = FString::FromInt(Delta.header.id);
			if(DDoc->nodes.Contains(id))
			{
				Delta.reverse = DDoc->nodes[id];
			}
			rtn = DDoc->Remove(id);

			break;
		}
		case EDeltaVerb::modify:
		{
			FString id = FString::FromInt(Delta.header.id);
			if(DDoc->nodes.Contains(id))
			{
				Delta.reverse = DDoc->nodes[id];
			}
			rtn = DDoc->Modify(Delta.details);
			
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

