// Copyright 2018 Modumate, Inc. All Rights Reserved.
#include "ModumateCore/PlatformFunctions.h"

#include "HAL/PlatformAtomics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ModumatePlatform"

const TArray<FString> FModumatePlatform::FileExtensions({
	FString(),
	FString(TEXT("mdmt")),
	FString(TEXT("pdf")),
	FString(TEXT("png")),
	FString(TEXT("dwg")),
	FString(TEXT("ilog")),
	FString(TEXT("csv"))
});

#if PLATFORM_WINDOWS

#include <string>
#include "Windows/WindowsSystemIncludes.h"
#include "Windows/AllowWindowsPlatformTypes.h"
	#include <shlobj.h>
	#include <shlwapi.h>
	#include <fileapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

static HRESULT CDialogEventHandler_CreateInstance(REFIID riid, void **ppv);

#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

LPCWSTR pszModProjExt = L"mdmt";
LPCWSTR pszModProjExtWildcard = L"*.mdmt";
static const PCWSTR pszModSaveFolder = L"Modumate";

const COMDLG_FILTERSPEC c_rgSaveTypes[] =
{
	{ L"Modumate Project Files (*.mdmt)", pszModProjExtWildcard },
	{ L"PDF Files (*.pdf)",       L"*.pdf"  },
	{ L"PNG Files (*.png)",       L"*.png"  },
	{ L"DWG Files (*.zip)",       L"*.zip"  },
	{ L"Input Log Files (*.ilog)",       L"*.ilog"  },
	{ L"CSV Files (*.csv)",       L"*.csv"  }
};

const COMDLG_FILTERSPEC c_rgSaveProjectFileType[] =
{
	{ L"Modumate Project Files (*.mdmt)", pszModProjExtWildcard }
};

const COMDLG_FILTERSPEC c_rgSavePdfFileType[] =
{
	{ L"PDF Files (*.pdf)",       L"*.pdf"  }
};

const COMDLG_FILTERSPEC c_rgSavePngFileType[] =
{
	{ L"PNG Files (*.png)",       L"*.png"  }
};

const COMDLG_FILTERSPEC c_rgSaveDwgFileType[] =
{
	{ L"AutoCAD DWG Bundle (*.zip)",    L"*.zip"  }
};

const COMDLG_FILTERSPEC c_rgSaveILogFileType[] =
{
	{ L"Input Log Files (*.ilog)",    L"*.ilog"  }
};

const COMDLG_FILTERSPEC c_rgSaveCsvFileType[] =
{
	{ L"CSV Files (*.csv)",       L"*.csv"  }
};


/* File Dialog Event Handler *****************************************************************************************************/

class CDialogEventHandler : public IFileDialogEvents, public IFileDialogControlEvents
{
public:

	static bool DialogVisible;

	// IUnknown methods
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		static const QITAB qit[] = {
			QITABENT(CDialogEventHandler, IFileDialogEvents),
			QITABENT(CDialogEventHandler, IFileDialogControlEvents),
			{ 0 },
#pragma warning(suppress:4838)
		};
		return QISearch(this, qit, riid, ppv);
	}


	IFACEMETHODIMP_(ULONG) AddRef()
	{
		return (ULONG)FPlatformAtomics::InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG) Release()
	{
		ULONG cRef = (ULONG)FPlatformAtomics::InterlockedDecrement(&_cRef);
		if (!cRef)
			delete this;
		return cRef;
	}

	// IFileDialogEvents methods
	IFACEMETHODIMP OnFileOk(IFileDialog *) override { return S_OK; };
	IFACEMETHODIMP OnFolderChanging(IFileDialog *, IShellItem *) override { return S_OK; };
	IFACEMETHODIMP OnFolderChange(IFileDialog *) override { return S_OK; };
	IFACEMETHODIMP OnSelectionChange(IFileDialog *) override { return S_OK; };
	IFACEMETHODIMP OnShareViolation(IFileDialog *, IShellItem *, FDE_SHAREVIOLATION_RESPONSE *) override { return S_OK; };
	IFACEMETHODIMP OnTypeChange(IFileDialog *pfd) override { return S_OK; };
	IFACEMETHODIMP OnOverwrite(IFileDialog *, IShellItem *, FDE_OVERWRITE_RESPONSE *) override { return S_OK; };

	// IFileDialogControlEvents methods
	IFACEMETHODIMP OnItemSelected(IFileDialogCustomize *pfdc, DWORD dwIDCtl, DWORD dwIDItem) override { return S_OK; };
	IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize *, DWORD) override { return S_OK; };
	IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize *, DWORD, BOOL) override { return S_OK; };
	IFACEMETHODIMP OnControlActivating(IFileDialogCustomize *, DWORD) override { return S_OK; };

	CDialogEventHandler() : _cRef(1) { };
private:
	virtual ~CDialogEventHandler() { };
	int32 _cRef;
};

bool CDialogEventHandler::DialogVisible = false;

namespace
{
	TFunction<bool()> SavedUserCallback;
	constexpr uint32 callbackTimerPeriod = 100; // ms

	void DialogTimerCallback(HWND hwnd, UINT message, UINT_PTR timerId, DWORD systemTime)
	{
		if (SavedUserCallback)
		{
			SavedUserCallback();
		}
		return;
	}
}

HRESULT FindOrAddDefaultSaveFolder(IShellItem*& saveDir)
{
	saveDir = nullptr;

	IShellLibrary* plib;
	HRESULT hr = CoCreateInstance(CLSID_ShellLibrary, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&plib));
	if (!SUCCEEDED(hr))
	{
		return hr;
	}

	hr = plib->LoadLibraryFromKnownFolder(FOLDERID_DocumentsLibrary, STGM_READWRITE);
	if (SUCCEEDED(hr))
	{
		IShellLibrary* docsLibrary;
		hr = plib->QueryInterface(IID_PPV_ARGS(&docsLibrary));

		if (SUCCEEDED(hr) && docsLibrary)
		{
			IShellItem *docsSaveFolder;
			hr = docsLibrary->GetDefaultSaveFolder(DSFT_DETECT, IID_PPV_ARGS(&docsSaveFolder));
			if (SUCCEEDED(hr) && docsSaveFolder)
			{
				PWSTR docsSaveFolderPath = nullptr;
				hr = docsSaveFolder->GetDisplayName(SIGDN_FILESYSPATH, &docsSaveFolderPath);
				if (SUCCEEDED(hr) && docsSaveFolderPath)
				{
					FString defaultSavePathStr = FString(docsSaveFolderPath) + TEXT("\\") + FString(pszModSaveFolder);
					LPCWSTR defaultSavePath = *defaultSavePathStr;
					bool bCreatedDirectory = CreateDirectoryW(defaultSavePath, NULL);

					if (bCreatedDirectory || (GetLastError() == ERROR_ALREADY_EXISTS))
					{
						hr = SHCreateItemFromParsingName(defaultSavePath, NULL, IID_PPV_ARGS(&saveDir));
					}
				}
			}

			docsSaveFolder->Release();
		}

		docsLibrary->Release();
	}

	plib->Release();

	return hr;
}

bool FModumatePlatform::GetOpenFilename(FString& filename, TFunction<bool()> userCallback, bool bUseDefaultFilters /*= true */)
{
	if (CDialogEventHandler::DialogVisible)
	{
		return false;
	}
	CDialogEventHandler::DialogVisible = true;

	// CoCreate the File Open Dialog object.
	IFileDialog *pfd = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	if (SUCCEEDED(hr))
	{
		// Create an event handling object, and hook it up to the dialog.
		IFileDialogEvents *pfde = nullptr;
		hr = CDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
		if (SUCCEEDED(hr))
		{
			// Hook up the event handler.
			DWORD dwCookie;
			hr = pfd->Advise(pfde, &dwCookie);
			if (SUCCEEDED(hr))
			{
				// Set the options on the dialog.
				DWORD dwFlags;

				// Before setting, always get the options first in order not to override existing options.
				hr = pfd->GetOptions(&dwFlags);
				if (SUCCEEDED(hr))
				{
					// In this case, get shell items only for file system items.
					hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
					if (SUCCEEDED(hr))
					{
						IShellItem* saveDir;
						if (SUCCEEDED(FindOrAddDefaultSaveFolder(saveDir)))
						{
							pfd->SetDefaultFolder(saveDir);
						}

						// Set the file types to display only. Notice that, this is a 1-based array.
						hr = bUseDefaultFilters ? pfd->SetFileTypes(ARRAYSIZE(c_rgSaveTypes), c_rgSaveTypes) : 0;
						if (SUCCEEDED(hr))
						{
							// Set the selected file type index to Modumate JSON Files for this example.
							hr = bUseDefaultFilters ? pfd->SetFileTypeIndex(INDEX_MODFILE) : 0;
							if (SUCCEEDED(hr))
							{
								// Set the default extension to be our project file type.
								hr = bUseDefaultFilters ? pfd->SetDefaultExtension(pszModProjExt) : 0;
								if (SUCCEEDED(hr))
								{
									// Create main-thread periodic callback for any maintenance.
									ensureMsgf(!SavedUserCallback, TEXT("Callback already exists"));
									UINT_PTR timerId = 0;
									if (userCallback)
									{
										SavedUserCallback = userCallback;
										timerId = SetTimer(nullptr, 0, callbackTimerPeriod, DialogTimerCallback);
										ensureMsgf(timerId != 0, TEXT("Couldn't create periodic timer"));
									}

									// Show the dialog
									hr = pfd->Show(nullptr);
									if (SUCCEEDED(hr))
									{
										// Obtain the result, once the user clicks the 'Open' button.
										// The result is an IShellItem object.
										IShellItem *psiResult;
										hr = pfd->GetResult(&psiResult);
										if (SUCCEEDED(hr))
										{
											PWSTR pszFilePath = nullptr;
											hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
											if (SUCCEEDED(hr))
											{
												filename = pszFilePath;
												CoTaskMemFree(pszFilePath);
											}

											psiResult->Release();
										}
									}

									if (timerId != 0)
									{
										KillTimer(nullptr, timerId);
										SavedUserCallback = TFunction<bool()>();
									}
								}
							}
						}
					}
				}

				// Unhook the event handler.
				pfd->Unadvise(dwCookie);
			}

			pfde->Release();
		}

		pfd->Release();
	}

	CDialogEventHandler::DialogVisible = false;

	if (SUCCEEDED(hr))
	{
		return true;
	}
	else
	{
		return false;
	}
}

// Instance creation helper
HRESULT CDialogEventHandler_CreateInstance(REFIID riid, void **ppv)
{
	*ppv = nullptr;
	CDialogEventHandler *pDialogEventHandler = new (std::nothrow) CDialogEventHandler();
	HRESULT hr = pDialogEventHandler ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr))
	{
		hr = pDialogEventHandler->QueryInterface(riid, ppv);
		pDialogEventHandler->Release();
	}
	return hr;
}

bool FModumatePlatform::GetSaveFilename(FString& filename, TFunction<bool()> userCallback, unsigned int fileType)
{
	if (CDialogEventHandler::DialogVisible)
	{
		return false;
	}
	CDialogEventHandler::DialogVisible = true;

	// CoCreate the File Save Dialog object.
	IFileDialog *pfd = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	if (SUCCEEDED(hr))
	{
		// Create an event handling object, and hook it up to the dialog.
		IFileDialogEvents *pfde = nullptr;
		hr = CDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
		if (SUCCEEDED(hr))
		{
			// Hook up the event handler.
			DWORD dwCookie;
			hr = pfd->Advise(pfde, &dwCookie);
			if (SUCCEEDED(hr))
			{
				// Set the options on the dialog.
				DWORD dwFlags;

				// Before setting, always get the options first in order not to override existing options.
				hr = pfd->GetOptions(&dwFlags);
				if (SUCCEEDED(hr))
				{
					IShellItem* saveDir;
					if (SUCCEEDED(FindOrAddDefaultSaveFolder(saveDir)))
					{
						pfd->SetDefaultFolder(saveDir);
					}

					// In this case, get shell items only for file system items.
					hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
					if (SUCCEEDED(hr))
					{
						// Set the file types to display only. Notice that, this is a 1-based array.
						if (fileType == INDEX_MODFILE)
						{
							hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSaveProjectFileType), c_rgSaveProjectFileType);
						}
						else if (fileType == INDEX_PDFFILE)
						{
							hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSavePdfFileType), c_rgSavePdfFileType);
						}
						else if (fileType == INDEX_PNGFILE)
						{
							hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSavePngFileType), c_rgSavePngFileType);
						}
						else if (fileType == INDEX_DWGFILE)
						{
							hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSaveDwgFileType), c_rgSaveDwgFileType);
						}
						else if (fileType == INDEX_ILOGFILE)
						{
							hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSaveILogFileType), c_rgSaveILogFileType);
						}
						else if (fileType == INDEX_CSVFILE)
						{
							hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSaveCsvFileType), c_rgSaveCsvFileType);
						}

						if (SUCCEEDED(hr))
						{
							// Set the selected file type to fileType.
							hr = pfd->SetFileTypeIndex(fileType);
							if (SUCCEEDED(hr))
							{
								// Create main-thread periodic callback for any maintenance.
								ensureMsgf(!SavedUserCallback, TEXT("Callback already exists"));
								UINT_PTR timerId = 0;
								if (userCallback)
								{
									SavedUserCallback = userCallback;
									timerId = SetTimer(nullptr, 0, callbackTimerPeriod, DialogTimerCallback);
									ensureMsgf(timerId != 0, TEXT("Couldn't create periodic timer"));
								}

								// Show the dialog
								hr = pfd->Show(nullptr);
								if (SUCCEEDED(hr))
								{
									// Obtain the result, once the user clicks the 'Save' button.
									// The result is an IShellItem object.
									IShellItem *psiResult;
									hr = pfd->GetResult(&psiResult);
									if (SUCCEEDED(hr))
									{
										PWSTR pszFilePath = nullptr;
										hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
										if (SUCCEEDED(hr))
										{
											// Save the file here
											filename = c_rgSaveTypes[fileType - 1].pszSpec;

											FString extension = filename.Right(filename.Len() - 1);
											FString fp = pszFilePath;

											// if we don't already have the file extension, add it
											if (fp.Len() < extension.Len() || fp.Right(extension.Len()) != extension)
											{
												filename = filename.Replace(L"*", pszFilePath);
											}
											else
											{
												filename = pszFilePath;
											}

											CoTaskMemFree(pszFilePath);
										}

										psiResult->Release();
									}
								}

								if (timerId != 0)
								{
									KillTimer(nullptr, timerId);
									SavedUserCallback = TFunction<bool()>();
								}
							}
						}
					}
				}

				// Unhook the event handler.
				pfd->Unadvise(dwCookie);
			}

			pfde->Release();
		}

		pfd->Release();
	}

	CDialogEventHandler::DialogVisible = false;


	if (SUCCEEDED(hr))
	{
		return true;
	}
	else
	{
		return false;
	}
}

// Note: this method only return the value of the registry keys of type REG_SZ. It will fail for registry keys of type REG_MULTI_SZ.
FString FModumatePlatform::GetStringValueFromHKCU(const FString &regSubKey, const FString &regValue)
{
	size_t bufferSize = 0xFFF;	// If too small, will be resized down below
	std::wstring valueBuf;	    // Contiguous buffer since C++11
	valueBuf.resize(bufferSize);
	auto cbData = static_cast<DWORD>(bufferSize);

	LSTATUS rc = RegGetValueW(
		HKEY_CURRENT_USER,
		TCHAR_TO_WCHAR(*regSubKey),
		TCHAR_TO_WCHAR(*regValue),
		RRF_RT_REG_SZ,
		nullptr,
		static_cast<void*>(&valueBuf.at(0)),
		&cbData);

	if (rc == ERROR_MORE_DATA) // valueBuf was not large enough to hold the data
	{
		// Increase the buffer size and call the API again
		valueBuf.resize(cbData);

		rc = RegGetValueW(
			HKEY_CURRENT_USER,
			TCHAR_TO_WCHAR(*regSubKey),
			TCHAR_TO_WCHAR(*regValue),
			RRF_RT_REG_SZ,
			nullptr,
			static_cast<void*>(&valueBuf.at(0)),
			&cbData);
	}

	if (rc == ERROR_SUCCESS)
	{
		return valueBuf.c_str();
	}
	else
	{
		return "";
	}
}


FModumatePlatform::EMessageBoxResponse FModumatePlatform::ShowMessageBox(const FString &msg, const FString &caption, EMessageBoxType type)
{
	int mbType = MB_OK;

	switch (type)
	{
		case OkayCancel: mbType = MB_OKCANCEL; break;
		case YesNo: mbType = MB_YESNO; break;
		case YesNoCancel: mbType = MB_YESNOCANCEL; break;
		case RetryCancel: mbType = MB_RETRYCANCEL; break;
		default: break;
	};

	switch (MessageBox(
		GetActiveWindow(),
		*msg,
		*caption,
		mbType | MB_APPLMODAL
	))
	{
		case IDCANCEL: return Cancel; break;
		case IDNO: return No; break;
		case IDYES: case IDOK: return Yes; break;
		case IDRETRY: return Retry; break;
	};

	return Cancel;
}

bool FModumatePlatform::ConsumeTempMessage(FString& OutMessage)
{
	WCHAR TempDir[MAX_PATH];
	DWORD TempPathResult = GetTempPath(MAX_PATH, TempDir);
	if ((TempPathResult > MAX_PATH) || (TempPathResult == MAX_PATH))
	{
		return false;
	}

	HMODULE CurModuleHandle = GetModuleHandle(NULL);
	if (!ensure(CurModuleHandle != INVALID_HANDLE_VALUE))
	{
		return false;
	}

	WCHAR CurModuleFilePathW[MAX_PATH];
	if (!ensure(GetModuleFileNameW(CurModuleHandle, CurModuleFilePathW, sizeof(CurModuleFilePathW)) != 0))
	{
		return false;
	}

	FString CurModuleFileName = FPaths::GetCleanFilename(FString(WCHAR_TO_TCHAR(CurModuleFilePathW)));
	const WCHAR* CurModuleFileNameW = TCHAR_TO_WCHAR(*CurModuleFileName);

	DWORD CurPID = GetCurrentProcessId();
	WCHAR TempFileNameW[MAX_PATH];
	UINT TempFileResult = GetTempFileNameW(TempDir, CurModuleFileNameW, CurPID, TempFileNameW);
	if (TempFileResult == 0)
	{
		return false;
	}

	const TCHAR* TempFileName = WCHAR_TO_TCHAR(TempFileNameW);
	if (!FFileHelper::LoadFileToString(OutMessage, TempFileName))
	{
		return false;
	}

	return IFileManager::Get().Delete(TempFileName, false, true, true);
}

#elif PLATFORM_MAC

#include "Mac/MacApplication.h"
#include "Misc/FeedbackContextMarkup.h"
#include "Mac/CocoaThread.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"


enum EFileDialogType { Open, Save };

bool GetOpenOrSavePath(FString& OutFilename, EFileDialogType DialogType, int32 FileTypeIndex = INDEX_NONE)
{
	FString fileTypeExt = (FileTypeIndex != INDEX_NONE) ? FModumatePlatform::FileExtensions[FileTypeIndex] : FString();
	
	FText titleFormat = (DialogType == EFileDialogType::Open) ?
		LOCTEXT("OpenFileTitleFormat", "Open {0}File") : LOCTEXT("SaveFileTitleFormat", "Save {0}File");
	FString fileTypeExtArg = (FileTypeIndex != INDEX_NONE) ? FString::Printf(TEXT(".%s "), *fileTypeExt) : FString();
	FString dialogTitle = FText::Format(titleFormat, FText::FromString(fileTypeExtArg)).ToString();
	
	bool bSave = false;
	FString defaultPath(TEXT("~"));
	
	MacApplication->SetCapture( NULL );
	MacApplication->SystemModalMode(true);

	bool bSuccess = false;
	bSuccess = MainThreadReturn(^{
		SCOPED_AUTORELEASE_POOL;

		NSSavePanel* panel = nullptr;
		
		switch (DialogType)
		{
			case EFileDialogType::Open:
			{
				panel = [NSOpenPanel openPanel];
				[panel setCanCreateDirectories: false];
				
				NSOpenPanel* openPanel = (NSOpenPanel*)panel;
				[openPanel setCanChooseFiles: true];
				[openPanel setCanChooseDirectories: false];
				[openPanel setAllowsMultipleSelection: false];
				break;
			}
			case EFileDialogType::Save:
			{
				panel = [NSSavePanel savePanel];
				[panel setCanCreateDirectories: true];
				break;
			}
		}

		// Set the title of the file dialog
		CFStringRef titleCFString = FPlatformString::TCHARToCFString(*dialogTitle);
		[panel setTitle: (NSString*)titleCFString];
		CFRelease(titleCFString);

		// Set up the default path where we'll look for a file
		CFStringRef defaultPathCFString = FPlatformString::TCHARToCFString(*defaultPath);
		NSURL* defaultPathURL = [NSURL fileURLWithPath: (NSString*)defaultPathCFString];
		[panel setDirectoryURL: defaultPathURL];
		CFRelease(defaultPathCFString);

		// Set up which file extensions are allowed to be opened/saved with this dialog
		if (FileTypeIndex != INDEX_NONE)
		{
			NSMutableArray* AllowedFileTypes = [NSMutableArray arrayWithCapacity: 2];
			CFStringRef fileTypeCFStr = FPlatformString::TCHARToCFString(*fileTypeExt);
			[AllowedFileTypes addObject: (NSString*)fileTypeCFStr];
			CFRelease(fileTypeCFStr);
			
			[AllowedFileTypes addObject: @""];
			[panel setAllowedFileTypes: AllowedFileTypes];
		}
		else
		{
			[panel setAllowedFileTypes: nil];
		}

		bool bOkPressed = false;
		NSWindow* FocusWindow = [[NSApplication sharedApplication] keyWindow];

		NSInteger Result = [panel runModal];

		if (Result == NSModalResponseOK)
		{
			TCHAR FilePath[MAC_MAX_PATH];
			FPlatformString::CFStringToTCHAR((CFStringRef)[[panel URL] path], FilePath);
			OutFilename = IFileManager::Get().ConvertToRelativePath(FilePath);
			FPaths::NormalizeFilename(OutFilename);

			bOkPressed = true;
		}

		[panel close];
		
		if(FocusWindow)
		{
			[FocusWindow makeKeyWindow];
		}

		return bOkPressed;
	});
	
	MacApplication->SystemModalMode(false);
	MacApplication->ResetModifierKeys();
	
	return bSuccess;
}

bool FModumatePlatform::GetOpenFilename(FString& filename, TFunction<bool()> userCallback, bool bUseDefaultFilters /*= true */)
{
	return GetOpenOrSavePath(OutFilename, EFileDialogType::Open, bUseDefaultFilters ? INDEX_MODFILE : INDEX_NONE);
}

bool FModumatePlatform::GetSaveFilename(FString& filename, TFunction<bool()> userCallback, unsigned int fileType)
{
	return GetOpenOrSavePath(OutFilename, EFileDialogType::Save, fileType);
}

FString FModumatePlatform::GetStringValueFromHKCU(const FString &regSubKey, const FString &regValue)
{
	return FString();
}

FModumatePlatform::EMessageBoxResponse FModumatePlatform::ShowMessageBox(const FString &msg, const FString &caption, EMessageBoxType type)
{
	return Cancel;
}

bool FModumatePlatform::ConsumeTempMessage(FString& OutMessage)
{
	return false;
}


#else

// TODO: implement these functions for all platforms, or use/integration UE4's cross-platform alternatives


bool FModumatePlatform::GetOpenFilename(FString& filename, TFunction<bool()> userCallback, bool bUseDefaultFilters /*= true */)
{
	return false;
}

bool FModumatePlatform::GetSaveFilename(FString& filename, TFunction<bool()> userCallback, unsigned int fileType)
{
	return false;
}

FString FModumatePlatform::GetStringValueFromHKCU(const FString &regSubKey, const FString &regValue)
{
	return FString();
}

FModumatePlatform::EMessageBoxResponse FModumatePlatform::ShowMessageBox(const FString &msg, const FString &caption, EMessageBoxType type)
{
	return Cancel;
}

bool FModumatePlatform::ConsumeTempMessage(FString& OutMessage)
{
	return false;
}


#endif

#undef LOCTEXT_NAMESPACE
