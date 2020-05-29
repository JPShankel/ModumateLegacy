// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDwgConnect.h"

#include "IPAddress.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "Misc/Timespan.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"

#include "Drafting/MiniZip.h"

namespace Modumate
{
	// Hard-coded address for initial tests:
	const FString FModumateDwgConnect::ServerAddress { "127.0.0.1:8080" };

	FModumateDwgConnect::FModumateDwgConnect(const FModumateDwgDraw& dwgDraw)
		: DwgDraw(dwgDraw)
	{ }

	Modumate::FModumateDwgConnect::~FModumateDwgConnect()
	{
		Socket->Close();
	}

	bool FModumateDwgConnect::ConvertJsonToDwg(FString filename)
	{
		Filename = filename;

		FTcpSocketBuilder socketBuilder(TEXT("DwgServer"));
		socketBuilder.Build();
		Socket = socketBuilder;
		Socket->SetNonBlocking(false);

		FIPv4Endpoint address;
		FIPv4Endpoint::Parse(ServerAddress, address);
		auto internetAddr = address.ToInternetAddr();
		Socket->Connect(*internetAddr);

		if (Socket->GetConnectionState() != ESocketConnectionState::SCS_Connected)
		{
			UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't connect to DWG server at %s"), *ServerAddress);
			return false;
		}

		return UploadJsonAndSave();
	}

	bool FModumateDwgConnect::UploadJsonAndSave()
	{
		FString basename = FPaths::GetBaseFilename(Filename, true);
		FString zipDirectory = FPaths::GetBaseFilename(Filename, false);
		TArray<FString> dwgFilenames;

		IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!platformFile.CreateDirectory(*zipDirectory))
		{
			UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't create archive directory %s"), *zipDirectory);
			return false;
		}

		int numPages = DwgDraw.GetNumPages();
		for (int page = 0; page < numPages; ++page)
		{
			ServerCommand cmnd = ServerCommand::kConvertPage;
			int32 bytesSent;
			Send(cmnd);
			FString pageJson = DwgDraw.GetJsonAsString(page);
			int32 jsonLength = pageJson.Len();
			Send(jsonLength);
			Socket->Send((uint8*) TCHAR_TO_ANSI(*pageJson), jsonLength, bytesSent);
			int32 dwgLength = 0;
			if (!Recv(&dwgLength) || dwgLength <= 0)
			{
				UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't receive DWG length"));
				return false;
			}
			
			TArray<uint8> dwgContents;
			dwgContents.Init(0, dwgLength);

			if (!Recv(dwgContents.GetData(), dwgLength))
			{
				UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't receive DWG contents"));
				return false;
			}

			{
				FString filenameDwg = FString::Printf(TEXT("%s/%s%d.dwg"), *zipDirectory, *basename, page + 1);

				if (!FFileHelper::SaveArrayToFile(dwgContents, *filenameDwg))
				{
					UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't save DWG file to %s"), *filenameDwg);
					return false;
				}

				dwgFilenames.Add(filenameDwg);
			}
		}

		for (auto image: DwgDraw.GetImages())
		{
			FString imageDestination = zipDirectory / FPaths::GetCleanFilename(image);
			if (!platformFile.CopyFile(*imageDestination, *image))
			{
				UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't copy DWG image file to %s"), *imageDestination);
			}
		}
		
		FMiniZip miniZip;
		struct ListFilesVisitor : IPlatformFile::FDirectoryVisitor
		{
			FMiniZip * zipper;

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (!bIsDirectory)
				{
					zipper->AddFile(FilenameOrDirectory);
				}
				return true;
			}
		} listFilesVisitor;
		listFilesVisitor.zipper = &miniZip;

		platformFile.IterateDirectory(*zipDirectory, listFilesVisitor);

		bool zipResult = miniZip.CreateArchive(zipDirectory + TEXT(".zip"));

		if (zipResult)
		{
			UE_LOG(LogAutoDrafting, Display, TEXT("Saved DWG bundle to %s.zip"), *zipDirectory);
		}
		else
		{
			UE_LOG(LogAutoDrafting, Error, TEXT("Failed to create DWG bundle from %s"), *zipDirectory);
		}
		return zipResult;
	}

	bool Modumate::FModumateDwgConnect::Recv(uint8 * buffer, int length)
	{
		int bytesRecvd = 0;
		while (bytesRecvd < length)
		{
			int bytes;
			bool result = Socket->Recv(buffer + bytesRecvd, length - bytesRecvd, bytes);
			if (!result || bytes <= 0)
			{
				return false;
			}
			bytesRecvd += bytes;
		}

		return true;
	}
}