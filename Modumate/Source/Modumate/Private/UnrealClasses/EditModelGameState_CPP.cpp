// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Drafting/ModumateDraftingView.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Database/ModumateObjectDatabase.h"
#include "Algo/Reverse.h"
#include "Algo/Transform.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "DocumentManagement/ModumateObjectInstance.h"


AEditModelGameState_CPP::AEditModelGameState_CPP()
{
}

AEditModelGameState_CPP::~AEditModelGameState_CPP()
{
}

TArray<float> AEditModelGameState_CPP::GetComponentsThicknessWithKey(EToolMode Mode, const FBIMKey& AssemblyKey) const
{
	TArray<float> layersThickness;

	// This may be called with an empty assembly, which legitimately should return empty results.
	if (AssemblyKey.IsNone())
	{
		return layersThickness;
	}

	const FBIMAssemblySpec *pMOA = Document.PresetManager.GetAssemblyByKey(Mode, AssemblyKey);

	if (ensureAlways(pMOA != nullptr))
	{
		for (int32 i = 0; i < pMOA->Layers.Num(); ++i)
		{
			layersThickness.Add(pMOA->Layers[i].Thickness.AsWorldCentimeters());
		}
		return layersThickness;
	}

	ensureAlways(false);
	return layersThickness;
}

bool AEditModelGameState_CPP::GetPortalToolTip(EToolMode Mode, const FBIMKey& AssemblyKey, FString &Type, FString &ConfigName, TArray<FString>& Parts)
{
	Type = TEXT("UNKNOWN PORTAL TYPE");
	ConfigName = TEXT("UNKNOWN PORTAL CONFIG");
	return false;
}
