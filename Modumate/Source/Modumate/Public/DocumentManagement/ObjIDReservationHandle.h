// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

class FModumateDocument;

// RAII wrapper for reserving object IDs from FModumateDocument
class MODUMATE_API FObjIDReservationHandle
{
public:
	FObjIDReservationHandle(FModumateDocument* InDocument, int32 InReservingID);
	~FObjIDReservationHandle();

private:
	FModumateDocument* Document;
	int32 ReservingID;

public:
	int32 NextID;
};
