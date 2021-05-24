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
private:
	TArray<FString> Files;

	static const int CompressionLevel = 9;
};

