// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/DesignOption.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/MOIDelta.h"
#include "Objects/MetaGraph.h"

AMOIDesignOption::AMOIDesignOption()
{
	FWebMOIProperty prop;

	prop.Name = TEXT("hexColor");
	prop.Type = EWebMOIPropertyType::color;
	prop.DisplayName = TEXT("Color");
	WebProperties.Add(prop.Name,prop);

	prop.Name = TEXT("groups");
	prop.Type = EWebMOIPropertyType::moiId;
	prop.DisplayName = TEXT("Groups");
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("subOptions");
	prop.Type = EWebMOIPropertyType::moiId;
	prop.DisplayName = TEXT("Sub-Options");
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("isShowing");
	prop.Type = EWebMOIPropertyType::boolean;
	prop.DisplayName = TEXT("Showing");
	WebProperties.Add(prop.Name, prop);
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

