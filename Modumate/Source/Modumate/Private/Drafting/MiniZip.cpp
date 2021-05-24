// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/MiniZip.h"

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Drafting/MiniZip/zip.h"

// UE4's prebuilt zlib includes the contributed zip & unzip functionality for handling zip archives.
// UE4 does not appear to cleanly expose the API, hence the fragile #include above.


bool FMiniZip::CreateArchive(const FString& archiveFilename)
{
	FString archiveNameClean = archiveFilename.Replace(TEXT("\\"), TEXT("/"));
	zipFile zf = zipOpen64(TCHAR_TO_ANSI(*archiveNameClean), 0);
	if (zf == nullptr)
	{
		return false;
	}

	for (auto& filename: Files)
	{
		FString zipFilename = FPaths::GetCleanFilename(filename);
		auto err = zipOpenNewFileInZip3_64(zf, TCHAR_TO_ANSI(*zipFilename),
			nullptr, nullptr, 0, nullptr, 0, nullptr,
			Z_DEFLATED, CompressionLevel, 0,
			-MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY,
			nullptr, 0ul, 0);

		if (err == ZIP_OK)
		{
			TArray<uint8> fileContents;
			if (!FFileHelper::LoadFileToArray(fileContents, *filename))
			{
				return false;
			}
			err = zipWriteInFileInZip(zf, fileContents.GetData(), fileContents.Num());
			if (err != ZIP_OK)
			{
				return false;
			}
			err = zipCloseFileInZip(zf);
		}

	}

	return zipClose(zf, nullptr) == ZIP_OK;
}

#else

// TODO: Modify UE4's zlib so that we can take advantage of the features we need, while also using its cross-platform compilation rules and libraries

bool FMiniZip::CreateArchive(const FString& archiveFilename)
{
    return false;
}

#endif
