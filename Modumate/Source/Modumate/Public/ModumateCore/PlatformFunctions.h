// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 *
 */
const unsigned int INDEX_MODFILE = 1;
const unsigned int INDEX_PDFFILE = 2;
const unsigned int INDEX_PNGFILE = 3;

namespace Modumate
{
	namespace PlatformFunctions
	{

		bool GetOpenFilename(FString &filename, bool bUseDefaultFilters = true);
		bool GetSaveFilename(FString &filename, unsigned int filter);

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

		EMessageBoxResponse ShowMessageBox(const FString &msg, const FString &caption,EMessageBoxType type);

		FString GetStringValueFromHKCU(const FString &regSubKey, const FString &regValue);
	}
}
