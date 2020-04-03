// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelGameState_CPP.h"
#include "ModumateDraftingView.h"
#include "ModumateGameInstance.h"
#include "ModumateFunctionLibrary.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateCommands.h"
#include "ModumateObjectDatabase.h"
#include "Algo/Reverse.h"
#include "Algo/Transform.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "ModumateObjectInstance.h"


AEditModelGameState_CPP::AEditModelGameState_CPP()
{
}

AEditModelGameState_CPP::~AEditModelGameState_CPP()
{
}

/*
The GameState manages the flow of information between the (read only) core catalog of objects,
stored in GameMode::ObjectDatabase, and the assembly schedule stored in the document

Data goes back and forth to Blueprint via the FShoppingItem struct. The Document interface
supports the internal assembly and layer structures

*/
const FModumateObjectAssembly *AEditModelGameState_CPP::GetAssemblyByKey(const FName &key) const
{
	return Document.PresetManager.GetAssemblyByKey(key);
}

const FModumateObjectAssembly *AEditModelGameState_CPP::GetAssemblyByKey_DEPRECATED(EToolMode mode,const FName &key) const
{
	return Document.PresetManager.GetAssemblyByKey(mode, key);
}

/*
Tool Mode accessors
*/
TArray<FShoppingItem> AEditModelGameState_CPP::GetAssembliesForToolMode(EToolMode mode) const
{
	TArray<FShoppingItem> ret;
	Algo::Transform(Document.GetAssembliesForToolMode_DEPRECATED(GetWorld(), mode),ret,[](const FModumateObjectAssembly &moa){return moa.AsShoppingItem();});
	return ret;
}

TArray<FShoppingItem> AEditModelGameState_CPP::GetMarketplaceAssembliesForToolMode(EToolMode mode) const
{
	TArray<FShoppingItem> ret;
	Modumate::ModumateObjectDatabase *objectDB = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
	Modumate::DataCollection<FModumateObjectAssembly> *db = objectDB->PresetManager.AssemblyDBs_DEPRECATED.Find(mode);

	if (db != nullptr)
	{
		for (auto &kvp : db->DataMap)
		{
			ret.Add(kvp.Value.AsShoppingItem());
		}
	}

	return ret;
}

void AEditModelGameState_CPP::ImportAssemblyFromMarketplace(EToolMode mode, const FName &key)
{
	Modumate::ModumateObjectDatabase *objectDB = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
	const FModumateObjectAssembly *pAsm = objectDB->PresetManager.GetAssemblyByKey(mode,key);

	if (pAsm != nullptr)
	{
		TArray<FName> childPresets;
		pAsm->GatherPresets_DEPRECATED(objectDB->PresetManager, childPresets);
		Document.ImportPresetsIfMissing_DEPRECATED(GetWorld(), childPresets);

		Modumate::FModumateFunctionParameterSet params;
		pAsm->ToParameterSet_DEPRECATED(params);

		Modumate::FModumateCommand cmd(Modumate::Commands::kCreateNewAssembly_DEPRECATED, false, 3);
		cmd.SetParameterSet(params);

		UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
		gameInstance->DoModumateCommand(cmd);
	}
}

/*
Assembly Accessors
*/
TArray<FShoppingItem> AEditModelGameState_CPP::GetComponentsForAssembly(EToolMode mode, const FShoppingItem &item) const
{
	// This may be called with an empty assembly, which legitimately should return empty results.
	if (item.Key.IsNone())
	{
		return TArray<FShoppingItem>();
	}

	const FModumateObjectAssembly *pMOA = Document.PresetManager.GetAssemblyByKey(mode, item.Key);

	if (ensureAlways(pMOA != nullptr))
	{
		TArray<FShoppingItem> ret;
		Algo::Transform(pMOA->Layers,ret,[](const FModumateObjectAssemblyLayer &moa){return moa.AsShoppingItem();});
		return ret;
	}

	ensureAlways(false);
	return TArray<FShoppingItem>();
}

TArray<float> AEditModelGameState_CPP::GetComponentsThicknessWithKey(EToolMode mode, const FString &assemblyKey) const
{
	TArray<float> layersThickness;

	// This may be called with an empty assembly, which legitimately should return empty results.
	if (assemblyKey.IsEmpty())
	{
		return layersThickness;
	}

	const FModumateObjectAssembly *pMOA = Document.PresetManager.GetAssemblyByKey(mode, FName(*assemblyKey));

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

int32 AEditModelGameState_CPP::CheckRemoveAssembly(EToolMode mode, const FString &assemblyKey)
{
	TArray<int32> obIds;
	Document.GetObjectIdsByAssembly(*assemblyKey,obIds);
	return obIds.Num();
}

bool AEditModelGameState_CPP::DoRemoveAssembly(EToolMode mode, const FString &assemblyKey, const FString &replaceAssemblyKey)
{
	UModumateGameInstance *gameInstance = Cast<UModumateGameInstance>(GetWorld()->GetGameInstance());

	FName modeName = FindEnumValueFullName<EToolMode>(TEXT("EToolMode"), mode);

	FName nameKey = *assemblyKey;
	FName replaceKey = *replaceAssemblyKey;

	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(mode);

	if (!UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(objectType))
	{
		gameInstance->DoModumateCommand(
			Modumate::FModumateCommand(Modumate::Commands::kRemoveAssembly_DEPRECATED)
			.Param(Modumate::Parameters::kToolMode, modeName)
			.Param(Modumate::Parameters::kAssembly, nameKey)
			.Param(Modumate::Parameters::kReplacementKey, replaceKey));
	}
	else
	{
		gameInstance->DoModumateCommand(
			Modumate::FModumateCommand(Modumate::Commands::kRemovePresetProjectAssembly)
			.Param(Modumate::Parameters::kPresetKey, assemblyKey)
			.Param(Modumate::Parameters::kReplacementKey, replaceKey));
	}

	return true;
}

bool AEditModelGameState_CPP::GetPortalToolTip(EToolMode mode, const FName &assemblyKey, FString &type, FString &configName, TArray<FString> &parts)
{
	const FModumateObjectAssembly *moa = Document.PresetManager.GetAssemblyByKey(mode, assemblyKey);
	if (moa != nullptr)
	{
		type = FindEnumValueString<EPortalFunction>(TEXT("EPortalFunction"), moa->PortalConfiguration.PortalFunction);
		configName = moa->PortalConfiguration.DisplayName.ToString();
		
		for (auto& curPart : moa->PortalParts)
		{
			parts.Add(curPart.Value.DisplayName.ToString());
		}
		return true;
	}
	return false;
}
