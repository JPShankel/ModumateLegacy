// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "IImageWrapper.h"
#include "UObject/Object.h"

#include "ThumbnailCacheManager.generated.h"

UCLASS()
class MODUMATE_API UThumbnailCacheManager : public UObject
{
	GENERATED_BODY()

public:
	static const FString ThumbnailCacheDirName;
	static const FString ThumbnailImageExt;

	void Init();
	void Shutdown();
	void ClearCachedThumbnails();

	UPROPERTY(EditAnywhere, Category = "Modumate|Thumbnails")
	bool bDiskEnabled = false;

	UFUNCTION(Category = "Modumate|Thumbnails")
	bool HasCachedThumbnail(FName ThumbnailKey);

	UFUNCTION(Category = "Modumate|Thumbnails")
	bool HasSavedThumbnail(FName ThumbnailKey);

	UFUNCTION(Category = "Modumate|Thumbnails")
	bool IsSavingThumbnail(FName ThumbnailKey);

	UFUNCTION(Category = "Modumate|Thumbnails")
	void OnThumbnailSaved(FName ThumbnailKey, bool bSaveSuccess);

	UFUNCTION(BlueprintCallable, Category = "Modumate|Thumbnails")
	UTexture2D* GetCachedThumbnail(FName ThumbnailKey);

	static FName GetThumbnailKeyForAssembly(const FBIMAssemblySpec &Assembly);

	static FName GetThumbnailKeyForPreset(const FGuid& PresetID);

	UFUNCTION(BlueprintPure, Category = "Modumate|Thumbnails")
	static FString GetThumbnailCacheDir();

	UFUNCTION(BlueprintPure, Category = "Modumate|Thumbnails")
	static FString GetThumbnailCachePathForKey(FName ThumbnailKey);

	UFUNCTION(BlueprintCallable, Category = "Modumate|Thumbnails", meta = (WorldContext = "WorldContextObject"))
	static UTexture2D* GetCachedThumbnailFromPresetKey(const FGuid& PresetKey, UObject *WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Modumate|Thumbnails", meta = (WorldContext = "WorldContextObject"))
	static bool SaveThumbnailFromPresetKey(UTexture *ThumbnailTexture, const FGuid& PresetKey, UTexture2D*& OutSavedTexture, UObject *WorldContextObject, bool AllowOverwrite = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate|Thumbnails")
	bool SaveThumbnail(UTexture *ThumbnailTexture, FName ThumbnailKey, UTexture2D*& OutSavedTexture, bool AllowOverwrite = false);

	static bool GetThumbnailFromTexture(UTexture* ThumbnailTexture, FName ThumbnailKey, UTexture2D*& OutSavedTexture, UObject* Outer);

	UFUNCTION(BlueprintCallable, Category = "Modumate|Texture")
	static UTexture2D* CreateTexture2D(int32 SizeX, int32 SizeY, int32 NumMips = 1, EPixelFormat Format = PF_B8G8R8A8, UObject* Outer = nullptr, FName Name = NAME_None);

	// Should only be used during viewport draw, else there will be a TextureRHI ensure
	UFUNCTION(BlueprintCallable, Category = "Modumate|Texture")
	static bool CopyViewportToTexture(UTexture2D* InTexture, UObject* WorldContextObject = nullptr);

protected:
	TSet<FName> SavedThumbnailsByKey;		// A set of thumbnails that have already been saved, by key.
	TSet<FName> SavingThumbnailsByKey;		// A set of thumbnails that are currently being saved, by key.

	UPROPERTY()
	TMap<FName, UTexture2D*> CachedThumbnailTextures;

	bool WriteThumbnailToDisk(UTexture2D *Texture, FName ThumbnailKey, EImageFormat ImageFormat, bool bAsync, bool AllowOverwrite = false);
};
