// Copyright 2020 Modumate, Inc. All Rights Reserved.

class FString;

#pragma once

class FMiniZip
{
public:
	void AddFile(const FString& filename)
	{
		Files.Add(filename);
	}
	bool CreateArchive(const FString& archiveFilename);
	bool ExtractFromArchive(const FString& ArchiveFilename);
private:
	TArray<FString> Files;

	static const int CompressionLevel = 9;
};

