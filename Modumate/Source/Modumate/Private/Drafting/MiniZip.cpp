// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/MiniZip.h"

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Drafting/MiniZip/zip.h"
#include "Drafting/MiniZip/unzip.h"
#include "HAL/PlatformFilemanager.h"

#include <stdio.h>
#include <memory>

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

bool FMiniZip::ExtractFromArchive(const FString& ArchiveFilename)
{
	FString archiveNameClean = ArchiveFilename.Replace(TEXT("\\"), TEXT("/"));
	unzFile zipfile = unzOpen64(TCHAR_TO_ANSI(*archiveNameClean));
	if (zipfile == nullptr)
	{
		return false;
	}
	// First, get the global information of the compressed file
	unz_global_info64 global_info;
	if (UNZ_OK != unzGetGlobalInfo64(zipfile, &global_info))
	{
		ensureMsgf(false, TEXT("Fail to GetGlobalInfo64 from zip: %s"), *ArchiveFilename);
		unzClose(zipfile);
		return false;
	}

	//Now loop through all the files
	unz_file_info64 zFileInfo;
	int32 num = 1024; // max filename size
	std::unique_ptr<char[]> fileName(new char[num]);

	//  Traverse all files 
	for (int32 i = 0; i < global_info.number_entry; i++)
	{
		if (UNZ_OK != unzGetCurrentFileInfo64(zipfile, &zFileInfo, fileName.get(), num, NULL, 0, NULL, 0))
		{
			ensureMsgf(false, TEXT("Fail to GetCurrentFileInfo64 from file in zip: %s"), *ArchiveFilename);
			unzClose(zipfile);
			return false;
		}

		// Current entry is a file, so extract it.
		if (unzOpenCurrentFile(zipfile) != UNZ_OK)
		{
			ensureMsgf(false, TEXT("Fail to OpenCurrentFile in zip: %s"), *ArchiveFilename);
			unzClose(zipfile);
			return false;
		}

		// Read the current file
		std::unique_ptr<char[]> fileData(new char[zFileInfo.uncompressed_size]);

		int fileLength = UNZ_OK;
		fileLength = unzReadCurrentFile(zipfile, (voidp)fileData.get(), zFileInfo.uncompressed_size);
		if (fileLength < 0)
		{
			ensureMsgf(false, TEXT("Fail to ReadCurrentFile in zip: %s"), *ArchiveFilename);
			unzCloseCurrentFile(zipfile);
			unzClose(zipfile);
			return false;
		}

		// Write file data to disk.
		if (fileLength > 0)
		{
			int32 lastDelimiter = INDEX_NONE;
			archiveNameClean.FindLastChar('/', lastDelimiter);
			FString relativePath = archiveNameClean.Left(lastDelimiter + 1);
			FString writePath = relativePath + FString(fileName.get());

			// Check if subfolder is necessary.
			int32 subFolderDelimiter = INDEX_NONE;
			FString fileNameString = FString(fileName.get());
			fileNameString.FindLastChar('/', subFolderDelimiter);
			if (subFolderDelimiter != INDEX_NONE)
			{
				FString subFolderName = fileNameString.Left(subFolderDelimiter + 1);
				FString subFolderPath = relativePath + subFolderName;
				if (!FPaths::DirectoryExists(subFolderPath))
				{
					IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
					platformFile.CreateDirectory(*subFolderPath);
				}
			}

			// Open a file to write out the data.
			auto writePathChar = StringCast<ANSICHAR>(*writePath);
			const char* writeFilename = writePathChar.Get();

			// TODO: Find alternative to fopen64
			FILE* out = fopen(writeFilename, "wb");
			if (out != NULL)
			{
				fwrite(fileData.get(), fileLength, 1, out);
				fclose(out);
			}
			else
			{
				ensureMsgf(false, TEXT("Fail to write from zip: %s"), *writePath);
			}
		}
		unzGoToNextFile(zipfile);
	}

	unzCloseCurrentFile(zipfile);
	unzClose(zipfile);

	return true;
}