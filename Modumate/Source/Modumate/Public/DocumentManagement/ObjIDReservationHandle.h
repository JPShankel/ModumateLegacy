// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

class UModumateDocument;

// RAII wrapper for reserving object IDs from UModumateDocument
class MODUMATE_API FObjIDReservationHandle
{
public:
	FObjIDReservationHandle(UModumateDocument* InDocument, int32 InReservingID);
	~FObjIDReservationHandle();

private:
	UModumateDocument* Document;
	int32 ReservingID;

public:
	int32 NextID;
};
