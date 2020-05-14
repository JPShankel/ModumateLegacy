// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IImageWrapper.h"
#include "ImagePixelData.h"
#include "Object.h"
#include "PixelFormat.h"
#include "Styling/SlateBrush.h"
#include "Templates/SharedPointer.h"

class MODUMATE_API FModumateThumbnailHelpers
{
public:
	static TSharedPtr<struct FSlateDynamicImageBrush> LoadProjectThumbnail(const FString &thumbnailBase64, const FString &thumbnailName);

	static bool GetRenderTextureData(class UTextureRenderTarget2D* RenderTarget, TArray64<uint8>& OutRawData);
	static bool CreateProjectThumbnail(class UTextureRenderTarget2D* textureTarget, FString &thumbnailBase64);

	static class UTexture2D* CreateTexture(int32 SizeX, int32 SizeY, int32 NumMips = 1, EPixelFormat Format = PF_B8G8R8A8, UObject* Outer = nullptr, FName Name = NAME_None, EObjectFlags ObjectFlags = EObjectFlags::RF_Transient);
	static class UTexture2D* CreateTexture2DFromRT(class UTextureRenderTarget2D *RenderTarget, UObject* Outer, FName NewTexName, EObjectFlags InObjectFlags, uint32 Flags = 0, TArray<uint8>* AlphaOverride = nullptr);
	static bool ReadTexturePixelData(class UTexture2D* Texture, TUniquePtr<TImagePixelData<FColor>>& OutPixelData);
	static bool CopyViewportToTexture(UTexture2D* InTexture, UObject* Outer = nullptr);

	static EImageFormat DefaultThumbImageFormat;
	static int32 DefaultThumbCompression;
	static FIntPoint DefaultThumbSize;
};