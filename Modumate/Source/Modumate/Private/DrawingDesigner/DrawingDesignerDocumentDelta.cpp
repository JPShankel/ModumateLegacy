// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"

static const TCHAR* DrawingDesignerDeltaLabel = TEXT("document");

FDrawingDesignerDocumentDelta::FDrawingDesignerDocumentDelta(const FDrawingDesignerDocument& doc,
	FDrawingDesignerJsDeltaPackage package
) : FDocumentDelta()
{
	doc.WriteJson(this->from);
	this->to = this->from;

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
		Doc->DrawingDesignerDocument.ReadJson(this->to);
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
		FDrawingDesignerDocument toDoc;
		toDoc.ReadJson(to);
		switch (delta.header.verb)
		{
		case EDeltaVerb::add:
		{
			if (delta.details.id == INDEX_NONE)
			{
				delta.details.id = toDoc.GetAndIncrDrawingId();
			}

			rtn = toDoc.Add(delta.details);
			break;
		}
		case EDeltaVerb::remove:
		{
			FString id = FString::FromInt(delta.header.id);
			rtn = toDoc.Remove(id);
			break;
		}
		case EDeltaVerb::modify:
		{
			FString id = FString::FromInt(delta.header.id);
			rtn = toDoc.Modify(delta.details);
			break;
		}
		default:
			rtn = false;
			break;
		}

		toDoc.WriteJson(to);
	}

	return rtn;
}

bool FDrawingDesignerJsDeltaPackage::ReadJson(const FString& InJson)
{
	auto JsonReader = TJsonReaderFactory<>::Create(InJson);

	TSharedPtr<FJsonObject> FileJsonRead;
	const TSharedPtr<FJsonObject>* rootObject;

	bool bSuccess = FJsonSerializer::Deserialize(JsonReader, FileJsonRead) && FileJsonRead.IsValid();

	if (bSuccess)
	{
		if (FileJsonRead->TryGetObjectField(DrawingDesignerDeltaLabel, rootObject))
		{
			const TArray<TSharedPtr<FJsonValue>>* deltasArray;

			if ((*rootObject)->TryGetArrayField(TEXT("deltas"), deltasArray))
			{
				//For each element in this array field
				for (TSharedPtr<FJsonValue> deltaValue : *deltasArray) 
				{
					if (deltaValue.IsValid()) 
					{
						const TSharedPtr<FJsonObject>* deltaAsObject;
						if (deltaValue->TryGetObject(deltaAsObject))
						{
							auto& delta = this->deltas.AddDefaulted_GetRef();
							//Read the Header....
							if (FJsonObjectConverter::JsonObjectToUStruct<FDrawingDesignerJsDelta>(deltaAsObject->ToSharedRef(), &delta))
							{
								//Read the Detail Object..
								const TSharedPtr<FJsonObject>* detailObject;
								if ((*deltaAsObject)->TryGetObjectField(TEXT("details"), detailObject))
								{
									if (!delta.details.FromJsonObject(*detailObject))
									{
										UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Details provided but unable to parse"));
										bSuccess = false;
									}
								}
							}
							else
							{
								UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Failed to add delta from drawing designer"));
								bSuccess = false;
							}
						}
					}
				}
			}
		}
	}

	return bSuccess;
}

