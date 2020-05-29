// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "Drafting/ModumateDwgDraw.h"

namespace Modumate
{
	/**
	 * Encapsulate connection to cloud drafting server.
	 */
	class FModumateDwgConnect
	{
	public:
		FModumateDwgConnect(const FModumateDwgDraw& dwgDraw);
		~FModumateDwgConnect();

		bool ConvertJsonToDwg(FString filename);

	private:
		const FModumateDwgDraw& DwgDraw;
		FSocket* Socket { nullptr };
		FString Filename;

		static const FString ServerAddress;

		bool UploadJsonAndSave();

		// Commands sent on link between client and server:
		enum ServerCommand: int32
		{
			kError = -1,  // Communication error.
			kPing,
			kPingReply,
			kConvertPage
		};

		bool Recv(uint8 * buffer, int32 length);
		template<class T> bool Send(const T& v) { int32 bs; return Socket->Send((const uint8*)&v, sizeof(T), bs); }
		template<class T> bool Recv(T* v) { return Recv((uint8*)v, sizeof(T)); }
	};

}