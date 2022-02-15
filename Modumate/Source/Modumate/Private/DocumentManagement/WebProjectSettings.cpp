// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/WebProjectSettings.h"

FWebProjectSettingsPropertyOption::FWebProjectSettingsPropertyOption()
{
}

FWebProjectSettingsPropertyOption::FWebProjectSettingsPropertyOption(const FString& InName, const FString& InId)
	: name(InName), id(InId)
{}