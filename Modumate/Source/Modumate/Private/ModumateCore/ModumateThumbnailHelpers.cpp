// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateThumbnailHelpers.h"

#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "UnrealClasses/ModumateViewportClient.h"
#include "Rendering/SlateRenderer.h"


EImageFormat FModumateThumbnailHelpers::DefaultThumbImageFormat = EImageFormat::JPEG;
int32 FModumateThumbnailHelpers::DefaultThumbCompression = 80;
FIntPoint FModumateThumbnailHelpers::DefaultThumbSize(256, 256);

TSharedPtr<FSlateDynamicImageBrush> FModumateThumbnailHelpers::LoadProjectThumbnail(const FString &thumbnailBase64, const FString &thumbnailName)
{
	TSharedPtr<FSlateDynamicImageBrush> thumbnailBrush;
	TArray<uint8> imageCompressedBytes;

	if (!thumbnailBase64.IsEmpty() && FBase64::Decode(thumbnailBase64, imageCompressedBytes))
	{
		auto &imageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName(TEXT("ImageWrapper")));

		EImageFormat inputFormat = imageWrapperModule.DetectImageFormat(imageCompressedBytes.GetData(), imageCompressedBytes.GetAllocatedSize());
		if (inputFormat == EImageFormat::Invalid)
		{
			inputFormat = DefaultThumbImageFormat;
		}

		TSharedPtr<IImageWrapper> imageWrapper = imageWrapperModule.CreateImageWrapper(DefaultThumbImageFormat);

		if (imageWrapper.IsValid() && imageWrapper->SetCompressed(imageCompressedBytes.GetData(), imageCompressedBytes.Num()))
		{
			int32 imageWidth = imageWrapper->GetWidth();
			int32 imageHeight = imageWrapper->GetHeight();
			TArray64<uint8> imageRawBytes;

			if (imageWrapper->GetRaw(ERGBFormat::RGBA, 8, imageRawBytes) &&
				ensureAlways((4 * imageWidth * imageHeight) == imageRawBytes.Num()))
			{
				// Reorder the image bytes to BGRA for the slate resource generation.
				// See FSlateRHIResourceManager::MakeDynamicTextureResource for details.
				TArray<uint8> imageRawBytesReordered;
				for (int32 i = 0; i < imageRawBytes.Num(); i += 4)
				{
					imageRawBytesReordered.Add(imageRawBytes[i + 2]);
					imageRawBytesReordered.Add(imageRawBytes[i + 1]);
					imageRawBytesReordered.Add(imageRawBytes[i]);
					imageRawBytesReordered.Add(imageRawBytes[i + 3]);
				}

				const static FString resourceSuffix(TEXT("_Thumbnail"));
				FName resourceName(*(thumbnailName + resourceSuffix));

				if (FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(
					resourceName, imageWidth, imageHeight, imageRawBytesReordered))
				{
					thumbnailBrush = MakeShared<FSlateDynamicImageBrush>(resourceName, FVector2D(imageWidth, imageHeight));
				}
			}
		}
	}

	return thumbnailBrush;
}

bool FModumateThumbnailHelpers::GetRenderTextureData(UTextureRenderTarget2D* RenderTarget, TArray64<uint8>& OutRawData)
{
	// Taken from GetRawData in ImageUtils
	FRenderTarget* renderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	EPixelFormat renderTargetFormat = RenderTarget->GetFormat();

	int32 numTextureBytes = CalculateImageBytes(RenderTarget->SizeX, RenderTarget->SizeY, 0, renderTargetFormat);
	OutRawData.AddUninitialized(numTextureBytes);
	bool bReadSuccess = false;
	switch (renderTargetFormat)
	{
	case PF_FloatRGBA:
	{
		TArray<FFloat16Color> floatColors;
		bReadSuccess = renderTargetResource->ReadFloat16Pixels(floatColors);
		FMemory::Memcpy(OutRawData.GetData(), floatColors.GetData(), numTextureBytes);
	}
	break;
	case PF_B8G8R8A8:
		bReadSuccess = renderTargetResource->ReadPixelsPtr((FColor*)OutRawData.GetData());
		break;
	}

	if (!bReadSuccess)
	{
		OutRawData.Empty();
	}

	return bReadSuccess;
}

bool FModumateThumbnailHelpers::CreateProjectThumbnail(class UTextureRenderTarget2D* textureTarget, FString &thumbnailBase64)
{
	TArray64<uint8> rawTextureTargetData;
	if (FModumateThumbnailHelpers::GetRenderTextureData(textureTarget, rawTextureTargetData))
	{
		// Mostly taken from FImageUtils::ExportRenderTarget
		EPixelFormat textureTargetFormat = textureTarget->GetFormat();
		int32 bitsPerPixel = (textureTargetFormat == PF_B8G8R8A8) ? 8 : (sizeof(FFloat16Color) / 4) * 8;
		ERGBFormat textureRGBFormat = (textureTargetFormat == PF_B8G8R8A8) ? ERGBFormat::BGRA : ERGBFormat::RGBA;

		IImageWrapperModule& imageWrapperModule = FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> imageWrapper = imageWrapperModule.CreateImageWrapper(DefaultThumbImageFormat);

		if (imageWrapper.IsValid() && imageWrapper->SetRaw(rawTextureTargetData.GetData(), rawTextureTargetData.GetAllocatedSize(),
			textureTarget->SizeX, textureTarget->SizeY, textureRGBFormat, bitsPerPixel))
		{
			// Compress the render target teture with our default compression format, and then encode it as a Base64 string
			const TArray64<uint8>& imageCompressedBytes = imageWrapper->GetCompressed(DefaultThumbCompression);
			if (imageCompressedBytes.Num() > 0)
			{
				thumbnailBase64 = FBase64::Encode(imageCompressedBytes.GetData(), imageCompressedBytes.Num());
				return true;
			}
		}
	}

	return false;
}

UTexture2D* FModumateThumbnailHelpers::CreateTexture(int32 SizeX, int32 SizeY, int32 NumMips, EPixelFormat Format, UObject* Outer, FName Name, EObjectFlags ObjectFlags)
{
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	// Mostly taken from UTexture2D::CreateTransient
	UTexture2D* newTexture = nullptr;

	if (SizeX > 0 && SizeY > 0 &&
		(SizeX % GPixelFormats[Format].BlockSizeX) == 0 &&
		(SizeY % GPixelFormats[Format].BlockSizeY) == 0)
	{
		newTexture = NewObject<UTexture2D>(Outer, Name, ObjectFlags);

		newTexture->PlatformData = new FTexturePlatformData();
		newTexture->PlatformData->SizeX = SizeX;
		newTexture->PlatformData->SizeY = SizeY;
		newTexture->PlatformData->PixelFormat = Format;

		// Allocate mipmap(s)
		for (int32 mipIdx = 0; mipIdx < NumMips; ++mipIdx)
		{
			int32 mipSizeX = SizeX << mipIdx;
			int32 mipSizeY = SizeY << mipIdx;
			if ((mipSizeX < 4) || (mipSizeY < 4))
			{
				break;
			}

			FTexture2DMipMap* mipData = new FTexture2DMipMap();
			newTexture->PlatformData->Mips.Add(mipData);
			mipData->SizeX = mipSizeX;
			mipData->SizeY = mipSizeY;

			int32 numBlocksX = mipSizeX / GPixelFormats[Format].BlockSizeX;
			int32 numBlocksY = mipSizeY / GPixelFormats[Format].BlockSizeY;

			mipData->BulkData.Lock(LOCK_READ_WRITE);
			mipData->BulkData.Realloc(numBlocksX * numBlocksY * GPixelFormats[Format].BlockBytes);
			mipData->BulkData.Unlock();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid parameters specified for FModumateThumbnailHelpers::CreateTexture"));
	}

	return newTexture;
}

UTexture2D* FModumateThumbnailHelpers::CreateTexture2DFromRT(UTextureRenderTarget2D *RenderTarget, UObject* Outer, FName NewTexName, EObjectFlags ObjectFlags, uint32 Flags, TArray<uint8>* AlphaOverride)
{
	UTexture2D* newTexture = nullptr;

	// Mostly taken from UTextureRenderTarget2D::ConstructTexture2D
	if (RenderTarget == nullptr)
	{
		return newTexture;
	}

	// Check render target size is valid and power of two
	const bool bIsValidSize = (
		(RenderTarget->SizeX != 0) && !(RenderTarget->SizeX & (RenderTarget->SizeX - 1)) &&
		(RenderTarget->SizeY != 0) && !(RenderTarget->SizeY & (RenderTarget->SizeY - 1)));

	// The r2t resource will be needed to read its surface contents
	FRenderTarget* rtResource = RenderTarget->GameThread_GetRenderTargetResource();
	const EPixelFormat pixelFormat = RenderTarget->GetFormat();

	// Exit if source is not compatible
	if (!bIsValidSize || (rtResource == nullptr) || (pixelFormat != EPixelFormat::PF_B8G8R8A8))
	{
		return newTexture;
	}

	// Read the render target pixels, and exit if they are unavailable
	TArray<FColor> rtPixels;
	if (!rtResource->ReadPixels(rtPixels))
	{
		return newTexture;
	}

	// Create the 2d texture, with the same size as the render target
	newTexture = FModumateThumbnailHelpers::CreateTexture(RenderTarget->SizeX, RenderTarget->SizeY, 1, pixelFormat,
		Outer, NewTexName, ObjectFlags);

	// Populate the texture's first mip with the render target contents
	FTexture2DMipMap& mipData = newTexture->PlatformData->Mips[0];
	uint32* mipBytes = (uint32*)mipData.BulkData.Lock(LOCK_READ_WRITE);
	const int32 mipSize = mipData.BulkData.GetBulkDataSize();

	// TODO: Find how ue4 handles render target format RTF_RGBA8_SRGB
	// If render target gamma used was 1.0 then disable SRGB for the static texture
	if (FMath::Abs(rtResource->GetDisplayGamma() - 1.0f) < KINDA_SMALL_NUMBER)
	{
		Flags &= ~CTF_SRGB;
	}
	newTexture->SRGB = (Flags & CTF_SRGB) != 0;

	// Override the alpha if desired
	if (AlphaOverride)
	{
		check(rtPixels.Num() == AlphaOverride->Num());
		for (int32 Pixel = 0; Pixel < rtPixels.Num(); Pixel++)
		{
			rtPixels[Pixel].A = (*AlphaOverride)[Pixel];
		}
	}
	else if (Flags & CTF_RemapAlphaAsMasked)
	{
		// if the target was rendered with a masked texture, then the depth will probably have been written instead of 0/255 for the
		// alpha, and the depth when unwritten will be 255, so remap 255 to 0 (masked out area) and anything else as 255 (written to area)
		for (int32 Pixel = 0; Pixel < rtPixels.Num(); Pixel++)
		{
			rtPixels[Pixel].A = (rtPixels[Pixel].A == 255) ? 0 : 255;
		}
	}
	else if (Flags & CTF_ForceOpaque)
	{
		for (int32 Pixel = 0; Pixel < rtPixels.Num(); Pixel++)
		{
			rtPixels[Pixel].A = 255;
		}
	}

	// copy the 2d surface data to the first mip of the static 2d texture
	check(mipSize == rtPixels.Num() * sizeof(FColor));
	FMemory::Memcpy(mipBytes, rtPixels.GetData(), mipSize);

	mipData.BulkData.Unlock();

	// Disable compression and editor-only mipmap generation
	// TODO: naively generate simple average mipmaps here, since it's not much more time than everything else we've done so far
#if WITH_EDITORONLY_DATA
	newTexture->CompressionNone = true;
	newTexture->DeferCompression = false;
	newTexture->MipGenSettings = TMGS_LeaveExistingMips;
#endif // WITH_EDITORONLY_DATA
	newTexture->CompressionSettings = TC_EditorIcon;

	newTexture->UpdateResource();

	return newTexture;
}

bool FModumateThumbnailHelpers::ReadTexturePixelData(UTexture2D* Texture, TUniquePtr<TImagePixelData<FColor>>& OutPixelData)
{
	// Make sure the input texture and output pixel data are valid
	if ((Texture == nullptr) || (Texture->GetPixelFormat() != PF_B8G8R8A8) ||
		(Texture->PlatformData == nullptr) || (Texture->PlatformData->Mips.Num() == 0))
	{
		return false;
	}

	FIntPoint textureDims(Texture->GetSizeX(), Texture->GetSizeY());

	// We expect the pixel data to already have been initialized with the input texture dims
	if (OutPixelData->GetSize() != textureDims)
	{
		return false;
	}

	FTexture2DMipMap& mipData = Texture->PlatformData->Mips[0];
	const int32 mipSize = mipData.BulkData.GetBulkDataSize();
	const int32 numPixels = mipSize / sizeof(FColor);

	// Ensure the dimensions are consistent with our expectations (2D texture, FColor elements)
	// We expect this to always be the case if we've passed the texture format checks earlier.
	if (!ensureAlways(
		(textureDims.X == mipData.SizeX) && (textureDims.Y == mipData.SizeY) && (mipData.SizeZ == 0) &&
		(numPixels == (textureDims.X * textureDims.Y))))
	{
		return false;
	}

	FColor* mipPixels = (FColor*)mipData.BulkData.Lock(LOCK_READ_ONLY);
	OutPixelData->Pixels.Reset(numPixels);
	OutPixelData->Pixels.Append(mipPixels, numPixels);
	mipData.BulkData.Unlock();

	return true;
}

bool FModumateThumbnailHelpers::CopyViewportToTexture(UTexture2D* InTexture, UObject* Outer /*= nullptr*/)
{
	if (Outer == nullptr || InTexture == nullptr)
	{
		return false;
	}

	int32 sizeX = Outer->GetWorld()->GetGameViewport()->Viewport->GetSizeXY().X;
	int32 sizeY = Outer->GetWorld()->GetGameViewport()->Viewport->GetSizeXY().Y;

	// Make sure the texture format and size are correct
	if (InTexture->GetSizeX() != sizeX || InTexture->GetSizeY() != sizeY || InTexture->GetPixelFormat() != EPixelFormat::PF_B8G8R8A8)
	{
		return false;
	}

	TArray<FColor> rtPixels;
	GetViewportScreenShot(Outer->GetWorld()->GetGameViewport()->Viewport, rtPixels);

	// Get the texture's first mip
	FTexture2DMipMap& mipData = InTexture->PlatformData->Mips[0];
	uint32* mipBytes = (uint32*)mipData.BulkData.Lock(LOCK_READ_WRITE);
	const int32 mipSize = mipData.BulkData.GetBulkDataSize();

	// copy the FColor data from viewport to the first mip
	check(mipSize == rtPixels.Num() * sizeof(FColor));
	FMemory::Memcpy(mipBytes, rtPixels.GetData(), mipSize);
	mipData.BulkData.Unlock();
	InTexture->UpdateResource();

	return true;
}
