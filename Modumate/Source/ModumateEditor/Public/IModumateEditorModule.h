// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * The public interface to this module
 */
class IModumateEditorModule : public IModuleInterface
{

public:
	static inline IModumateEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IModumateEditorModule>("ModumateEditor");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ModumateEditor");
	}
};
