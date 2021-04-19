// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"


struct FModumatePlatform
{
	static constexpr uint32 INDEX_MODFILE = 1;
	static constexpr uint32 INDEX_PDFFILE = 2;
	static constexpr uint32 INDEX_PNGFILE = 3;
	static constexpr uint32 INDEX_DWGFILE = 4;
	static constexpr uint32 INDEX_ILOGFILE = 5;
	static constexpr uint32 INDEX_CSVFILE = 6;
	
	static const TArray<FString> FileExtensions;
	
	static bool GetOpenFilename(FString &filename, bool bUseDefaultFilters = true);
	static bool GetSaveFilename(FString &filename, unsigned int filter);

	enum EMessageBoxResponse
	{
		Cancel,
		Yes,
		No,
		Retry
	};

	enum EMessageBoxType
	{
		Okay,
		OkayCancel,
		YesNo,
		YesNoCancel,
		RetryCancel
	};

	static EMessageBoxResponse ShowMessageBox(const FString &msg, const FString &caption,EMessageBoxType type);

	FString GetStringValueFromHKCU(const FString &regSubKey, const FString &regValue);
};
