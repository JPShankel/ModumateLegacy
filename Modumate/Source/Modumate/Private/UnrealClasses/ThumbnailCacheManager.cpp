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
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/ModumateGameInstance.h"


const FString UThumbnailCacheManager::ThumbnailCacheDirName(TEXT("ThumbnailCache"));
const FString UThumbnailCacheManager::ThumbnailImageExt(TEXT(".png"));

void UThumbnailCacheManager::Init()
{
	if (!bDiskEnabled)
	{
		return;
	}

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

void UThumbnailCacheManager::ClearCachedThumbnails()
{
	CachedThumbnailTextures.Empty();
	SavingThumbnailsByKey.Empty();
	SavedThumbnailsByKey.Empty();
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
	// If the callback's thumbnail key is not being saved, it could have just been canceled by being overwritten by another request.
	// TODO: queue up thumbnail disk writes after previous overwritten results have finished,
	// rather than assuming sequential overlapping writes complete in the order in which they were started.
	if (ensureAlways(!ThumbnailKey.IsNone()) && SavingThumbnailsByKey.Contains(ThumbnailKey))
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

FName UThumbnailCacheManager::GetThumbnailKeyForAssembly(const FBIMAssemblySpec &Assembly)
{
	return GetThumbnailKeyForPreset(Assembly.UniqueKey());
}

FName UThumbnailCacheManager::GetThumbnailKeyForPreset(const FGuid& PresetID)
{
	const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
	const FString& projectVersion = projectSettings->ProjectVersion;

	FString hashKey = FString::Printf(TEXT("%08X%s"), GetTypeHash(PresetID), *projectVersion);
	return FName(*hashKey);

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

UTexture2D* UThumbnailCacheManager::GetCachedThumbnailFromPresetKey(const FGuid& PresetKey, UObject *WorldContextObject)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UModumateGameInstance *modGameInst = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	UThumbnailCacheManager *thumbnailCacheMan = modGameInst ? modGameInst->ThumbnailCacheManager : nullptr;

	FName thumbnailKey = GetThumbnailKeyForPreset(PresetKey);

	if (thumbnailCacheMan && !thumbnailKey.IsNone())
	{
		return thumbnailCacheMan->GetCachedThumbnail(thumbnailKey);
	}

	return nullptr;
}

bool UThumbnailCacheManager::SaveThumbnailFromPresetKey(UTexture *ThumbnailTexture, const FGuid& PresetKey, UTexture2D*& OutSavedTexture, UObject *WorldContextObject, bool AllowOverwrite)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UModumateGameInstance *modGameInst = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	UThumbnailCacheManager *thumbnailCacheMan = modGameInst ? modGameInst->ThumbnailCacheManager : nullptr;

	FName thumbnailKey = GetThumbnailKeyForPreset(PresetKey);

	if (thumbnailCacheMan && !thumbnailKey.IsNone())
	{
		return thumbnailCacheMan->SaveThumbnail(ThumbnailTexture, thumbnailKey, OutSavedTexture, AllowOverwrite);
	}

	return false;
}

bool UThumbnailCacheManager::SaveThumbnail(UTexture *ThumbnailTexture, FName ThumbnailKey, UTexture2D*& OutSavedTexture, bool AllowOverwrite)
{
#if UE_SERVER
	return false;
#endif

	OutSavedTexture = nullptr;

	// We require a valid thumbnail key
	if (!ensureAlways(!ThumbnailKey.IsNone()))
	{
		return false;
	}

	// If we already have the thumbnail loaded, then return it so it can be used.
	if (!AllowOverwrite && HasCachedThumbnail(ThumbnailKey))
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

		if (GetThumbnailFromTexture(ThumbnailTexture, ThumbnailKey, OutSavedTexture, this))
		{
			CachedThumbnailTextures.Add(ThumbnailKey, OutSavedTexture);
		}
	}

	// If we can't save the cached thumbnail to disk right now, then return.
	if (IsSavingThumbnail(ThumbnailKey) || HasSavedThumbnail(ThumbnailKey))
	{
		if (AllowOverwrite)
		{
			SavingThumbnailsByKey.Remove(ThumbnailKey);
			SavedThumbnailsByKey.Remove(ThumbnailKey);
		}
		else
		{
			return false;
		}
	}

	if (bDiskEnabled)
	{
		// Actually write the converted texture to disk
		return WriteThumbnailToDisk(OutSavedTexture, ThumbnailKey, EImageFormat::PNG, true, AllowOverwrite);
	}
	{
		// Otherwise, it's only an in-memory cache, so just return success
		return true;
	}
}

bool UThumbnailCacheManager::GetThumbnailFromTexture(UTexture* ThumbnailTexture, FName ThumbnailKey, UTexture2D*& OutSavedTexture, UObject* Outer)
{
	OutSavedTexture = Cast<UTexture2D>(ThumbnailTexture);
	UTextureRenderTarget2D* renderTargetSource = Cast<UTextureRenderTarget2D>(ThumbnailTexture);

	if (!ensureAlwaysMsgf((OutSavedTexture != nullptr) || (renderTargetSource != nullptr),
		TEXT("Unsupported texture type for object %s, key %s; neither UTexture2D nor UTextureRenderTarget2D!"),
		*ThumbnailTexture->GetName(), *ThumbnailKey.ToString()))
	{
		return false;
	}

	if (renderTargetSource)
	{
		// If the source is a render target, then it must be converted to a texture first
		OutSavedTexture = FModumateThumbnailHelpers::CreateTexture2DFromRT(renderTargetSource, Outer, ThumbnailKey, EObjectFlags::RF_NoFlags);
		if (OutSavedTexture)
		{
			OutSavedTexture->SRGB = true;
		}
	}

	return true;
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

bool UThumbnailCacheManager::WriteThumbnailToDisk(UTexture2D *Texture, FName ThumbnailKey, EImageFormat ImageFormat, bool bAsync, bool AllowOverwrite)
{
	if (!bDiskEnabled)
	{
		return false;
	}

	FString thumbnailPath = GetThumbnailCachePathForKey(ThumbnailKey);
	if (Texture && (AllowOverwrite || !IFileManager::Get().FileExists(*thumbnailPath)))
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
		writeTask->bOverwriteFile = AllowOverwrite;
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
