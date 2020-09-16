// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/ThumbnailCacheManager.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "ImagePixelData.h"
#include "ImageUtils.h"
#include "ImageWriteQueue.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ModumateCore/ModumateThumbnailHelpers.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"


const FString UThumbnailCacheManager::ThumbnailCacheDirName(TEXT("ThumbnailCache"));
const FString UThumbnailCacheManager::ThumbnailImageExt(TEXT(".png"));

UThumbnailCacheManager::UThumbnailCacheManager(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UThumbnailCacheManager::Init()
{
	FString cacheDir = GetThumbnailCacheDir();
	TArray<FString> cachedThumbnailFileNames;
	IFileManager::Get().FindFiles(cachedThumbnailFileNames, *cacheDir, *ThumbnailImageExt);

	// Keep track of which thumbnails are already loaded, and won't need to be re-saved.
	for (const FString &cachedThumbnailFileName : cachedThumbnailFileNames)
	{
		FString cachedThumbnailPath = cacheDir / cachedThumbnailFileName;
		FString cachedThumbnailBaseName = FPaths::GetBaseFilename(cachedThumbnailFileName, true);
		FName cachedThumbnailKey(*cachedThumbnailBaseName);

		// Skip duplicate entries, because apparently FindFiles returns a duplicated list
		// for directories that are outside of the engine directory... (thanks, Epic)
		if (SavedThumbnailsByKey.Contains(cachedThumbnailKey))
		{
			continue;
		}

		SavedThumbnailsByKey.Add(cachedThumbnailKey);

		// TODO: make this asynchronous
		UTexture2D *cachedTexture = FImageUtils::ImportFileAsTexture2D(cachedThumbnailPath);
		if (cachedTexture)
		{
			// Make sure the loaded texture belongs to the cache manager
			uint32 renameFlags = REN_ForceNoResetLoaders | REN_NonTransactional;
			cachedTexture->Rename(*cachedThumbnailBaseName, this, renameFlags);

			// Keep the cached texture in memory
			CachedThumbnailTextures.Add(cachedThumbnailKey, cachedTexture);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load saved thumbnail texture file: %s"), *cachedThumbnailPath);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Found %d cached thumbnails."), cachedThumbnailFileNames.Num());
}

void UThumbnailCacheManager::Shutdown()
{
}

bool UThumbnailCacheManager::HasCachedThumbnail(FName ThumbnailKey)
{
	return CachedThumbnailTextures.Contains(ThumbnailKey);
}

bool UThumbnailCacheManager::HasSavedThumbnail(FName ThumbnailKey)
{
	return SavedThumbnailsByKey.Contains(ThumbnailKey);
}

bool UThumbnailCacheManager::IsSavingThumbnail(FName ThumbnailKey)
{
	return SavingThumbnailsByKey.Contains(ThumbnailKey);
}

void UThumbnailCacheManager::OnThumbnailSaved(FName ThumbnailKey, bool bSaveSuccess)
{
	if (ensureAlways(!ThumbnailKey.IsNone() && SavingThumbnailsByKey.Contains(ThumbnailKey)))
	{
		SavingThumbnailsByKey.Remove(ThumbnailKey);

		if (bSaveSuccess)
		{
			SavedThumbnailsByKey.Add(ThumbnailKey);
		}
	}
}

UTexture2D* UThumbnailCacheManager::GetCachedThumbnail(FName ThumbnailKey)
{
	return CachedThumbnailTextures.FindRef(ThumbnailKey);
}

FName UThumbnailCacheManager::GetThumbnailKeyForShoppingItemAndTool(const FBIMKey &Key, EToolMode ToolMode, UObject *WorldContextObject)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState_CPP *gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	const FBIMAssemblySpec *assembly = gameState ? gameState->Document.PresetManager.GetAssemblyByKey(ToolMode, Key) : nullptr;

	if (assembly)
	{
		return GetThumbnailKeyForAssembly(*assembly);
	}
	else
	{
		return NAME_None;
	}
	return NAME_None;
}

FName UThumbnailCacheManager::GetThumbnailKeyForAssembly(const FBIMAssemblySpec &Assembly)
{
	static const FString thumbnailKeySuffix(TEXT("_Ver0"));

	FString wholeKey = Assembly.RootPreset.ToString() + thumbnailKeySuffix;
	FString shortKey = FMD5::HashAnsiString(*wholeKey);

	return FName(*shortKey);
}

FString UThumbnailCacheManager::GetThumbnailCacheDir()
{
	FString tempDir = FModumateUserSettings::GetLocalTempDir();
	return FPaths::Combine(tempDir, *ThumbnailCacheDirName);
}

FString UThumbnailCacheManager::GetThumbnailCachePathForKey(FName ThumbnailKey)
{
	if (ThumbnailKey.IsNone())
	{
		return FString();
	}
	else
	{
		FString thumbnailFilename = ThumbnailKey.ToString() + ThumbnailImageExt;
		return FPaths::Combine(*UThumbnailCacheManager::GetThumbnailCacheDir(), *thumbnailFilename);
	}
}

UTexture2D* UThumbnailCacheManager::GetCachedThumbnailFromShoppingItemAndTool(const FBIMKey& Key, EToolMode ToolMode, UObject *WorldContextObject)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UModumateGameInstance *modGameInst = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	UThumbnailCacheManager *thumbnailCacheMan = modGameInst ? modGameInst->ThumbnailCacheManager : nullptr;

	FName thumbnailKey = GetThumbnailKeyForShoppingItemAndTool(Key, ToolMode, WorldContextObject);

	if (thumbnailCacheMan && !thumbnailKey.IsNone())
	{
		return thumbnailCacheMan->GetCachedThumbnail(thumbnailKey);
	}

	return nullptr;
}

bool UThumbnailCacheManager::SaveThumbnailFromShoppingItemAndTool(UTexture *ThumbnailTexture, const FBIMKey& Key, EToolMode ToolMode, UTexture2D*& OutSavedTexture, UObject *WorldContextObject)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UModumateGameInstance *modGameInst = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	UThumbnailCacheManager *thumbnailCacheMan = modGameInst ? modGameInst->ThumbnailCacheManager : nullptr;

	FName thumbnailKey = GetThumbnailKeyForShoppingItemAndTool(Key, ToolMode, WorldContextObject);

	if (thumbnailCacheMan && !thumbnailKey.IsNone())
	{
		return thumbnailCacheMan->SaveThumbnail(ThumbnailTexture, thumbnailKey, OutSavedTexture);
	}

	return false;
}

bool UThumbnailCacheManager::SaveThumbnail(UTexture *ThumbnailTexture, FName ThumbnailKey, UTexture2D*& OutSavedTexture)
{
	OutSavedTexture = nullptr;

	// We require a valid thumbnail key
	if (!ensureAlways(!ThumbnailKey.IsNone()))
	{
		return false;
	}

	// If we already have the thumbnail loaded, then return it so it can be used.
	if (HasCachedThumbnail(ThumbnailKey))
	{
		OutSavedTexture = CachedThumbnailTextures[ThumbnailKey];
	}
	// Otherwise, we need to save the thumbnail to the cache (potentially after converting it)
	else
	{
		// We require a valid input texture
		if (!ensureAlways(ThumbnailTexture))
		{
			return false;
		}

		OutSavedTexture = Cast<UTexture2D>(ThumbnailTexture);
		UTextureRenderTarget2D *renderTargetSource = Cast<UTextureRenderTarget2D>(ThumbnailTexture);

		if (!ensureAlwaysMsgf((OutSavedTexture != nullptr) || (renderTargetSource != nullptr),
			TEXT("Unsupported texture type for object %s, key %s; neither UTexture2D nor UTextureRenderTarget2D!"),
			*ThumbnailTexture->GetName(), *ThumbnailKey.ToString()))
		{
			return false;
		}

		if (renderTargetSource)
		{
			// If the source is a render target, then it must be converted to a texture first
			OutSavedTexture = FModumateThumbnailHelpers::CreateTexture2DFromRT(renderTargetSource, this, ThumbnailKey, EObjectFlags::RF_NoFlags);
		}

		CachedThumbnailTextures.Add(ThumbnailKey, OutSavedTexture);
	}

	// If we can't save the cached thumbnail to disk right now, then return.
	if (IsSavingThumbnail(ThumbnailKey) || HasSavedThumbnail(ThumbnailKey))
	{
		return false;
	}

	// Actually write the converted texture to disk
	return WriteThumbnailToDisk(OutSavedTexture, ThumbnailKey, EImageFormat::PNG, true);
}

UTexture2D* UThumbnailCacheManager::CreateTexture2D(int32 SizeX, int32 SizeY, int32 NumMips /*= 1*/, EPixelFormat Format /*= PF_B8G8R8A8*/, UObject* Outer /*= nullptr*/, FName Name /*= NAME_None*/)
{
	return FModumateThumbnailHelpers::CreateTexture(SizeX, SizeY, NumMips, Format, Outer, Name);
}

bool UThumbnailCacheManager::CopyViewportToTexture(UTexture2D* InTexture, UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr || InTexture == nullptr)
	{
		return false;
	}
	return FModumateThumbnailHelpers::CopyViewportToTexture(InTexture, WorldContextObject);
}

bool UThumbnailCacheManager::WriteThumbnailToDisk(UTexture2D *Texture, FName ThumbnailKey, EImageFormat ImageFormat, bool bAsync)
{
	FString thumbnailPath = GetThumbnailCachePathForKey(ThumbnailKey);
	if (Texture && !IFileManager::Get().FileExists(*thumbnailPath))
	{
		// Read the texture's pixel data
		FIntPoint textureSize(Texture->GetSizeX(), Texture->GetSizeY());
		TUniquePtr<TImagePixelData<FColor>> pixelData = MakeUnique<TImagePixelData<FColor>>(textureSize);
		if (!FModumateThumbnailHelpers::ReadTexturePixelData(Texture, pixelData))
		{
			return false;
		}

		// Create an image write task
		IImageWriteQueue &imageWriteQueue = FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>(TEXT("ImageWriteQueue")).GetWriteQueue();

		TUniquePtr<FImageWriteTask> writeTask = MakeUnique<FImageWriteTask>();

		writeTask->PixelData = MoveTemp(pixelData);
		writeTask->Format = ImageFormat;
		writeTask->Filename = thumbnailPath;
		writeTask->bOverwriteFile = false;
		writeTask->CompressionQuality = 100;

		TWeakObjectPtr<UThumbnailCacheManager> thumbnailCacheManPtr(this);
		writeTask->OnCompleted = [thumbnailCacheManPtr, ThumbnailKey](bool bWriteSuccess) -> void
		{
			if (thumbnailCacheManPtr.IsValid())
			{
				thumbnailCacheManPtr->OnThumbnailSaved(ThumbnailKey, bWriteSuccess);
			}
		};

		// Enqueue the image write task
		SavingThumbnailsByKey.Add(ThumbnailKey);

		TFuture<bool> dispatchedTask = imageWriteQueue.Enqueue(MoveTemp(writeTask));

		// Wait for the task to finish if we're sync
		if (!bAsync)
		{
			// If not async, wait for the dispatched task to complete
			if (dispatchedTask.IsValid())
			{
				dispatchedTask.Wait();
			}
		}
	}

	// Return whether we're now asynchronously saving the thumbnail, or if it's been synchronously saved.
	// This will return false if it synchronously failed to save.
	return IsSavingThumbnail(ThumbnailKey) || HasSavedThumbnail(ThumbnailKey);
}
