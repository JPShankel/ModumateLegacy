// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/DesignOption.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Objects/MOIDelta.h"
#include "Objects/MetaGraph.h"

AMOIDesignOption::AMOIDesignOption()
{
	FWebMOIProperty prop;

	prop.name = TEXT("hexColor");
	prop.type = EWebMOIPropertyType::color;
	prop.displayName = TEXT("Color");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name,prop);

	prop.name = TEXT("groups");
	prop.type = EWebMOIPropertyType::moiId;
	prop.displayName = TEXT("Groups");
	prop.isEditable = false;
	prop.isVisible = false;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("subOptions");
	prop.type = EWebMOIPropertyType::moiId;
	prop.displayName = TEXT("Sub-Options");
	prop.isEditable = false;
	prop.isVisible = false;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("isShowing");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("Showing");
	prop.isEditable = false;
	prop.isVisible = false;
	WebProperties.Add(prop.name, prop);
}

TSharedPtr<FMOIDelta> AMOIDesignOption::MakeCreateDelta(UModumateDocument* Doc, const FString& DisplayName, int32 ParentID)
{
	FMOIDesignOptionData optionData;

	FMOIStateData stateData(Doc->GetNextAvailableID(), EObjectType::OTDesignOption);
	stateData.ParentID = ParentID;
	stateData.DisplayName = DisplayName;
	optionData.hexColor = TEXT("#000000");
	auto delta = MakeShared<FMOIDelta>();
	stateData.CustomData.SaveStructData<FMOIDesignOptionData>(optionData);
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	return delta;
}

TSharedPtr<FMOIDelta> AMOIDesignOption::MakeAddRemoveGroupDelta(UModumateDocument* Doc, int32 OptionID, int32 GroupID, bool bAdd)
{
	const AMOIDesignOption* option = Cast<AMOIDesignOption>(Doc->GetObjectById(OptionID));
	if (!ensureAlways(option))
	{
		return nullptr;
	}

	if (!ensureAlways(Cast<AMOIMetaGraph>(Doc->GetObjectById(GroupID)) != nullptr))
	{
		return nullptr;
	}

	FMOIStateData oldData = option->GetStateData();
	FMOIStateData newData = oldData;
	FMOIDesignOptionData optionData;
	oldData.CustomData.LoadStructData(optionData);

	if (bAdd)
	{
		optionData.groups.AddUnique(GroupID);
	}
	else
	{
		optionData.groups.Remove(GroupID);
	}

	newData.CustomData.SaveStructData<FMOIDesignOptionData>(optionData);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddMutationState(option, oldData, newData);

	return delta;
}

bool AMOIDesignOption::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	TArray<int32> subOptions;
	TArray<int32> children = GetChildIDs();

	for (auto subOp : InstanceData.subOptions)
	{
		if (children.Contains(subOp))
		{
			subOptions.Add(subOp);
		}
	}

	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());

	if (controller && controller->GetDocument())
	{
		UModumateDocument* document = controller->GetDocument();
		for (auto child : children)
		{
			AModumateObjectInstance* childPtr = document->GetObjectById(child);
			if (childPtr && childPtr->GetObjectType() == EObjectType::OTDesignOption && !subOptions.Contains(child))
			{
				subOptions.Add(child);
			}
		}
		InstanceData.subOptions = subOptions;
	}
	return true;
}

void AMOIDesignOption::PostCreateObject(bool bNewObject)
{
	if (Document)
	{
		Super::PostCreateObject(bNewObject);
	}
}
