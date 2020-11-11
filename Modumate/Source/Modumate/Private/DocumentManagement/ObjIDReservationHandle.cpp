// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ObjIDReservationHandle.h"

#include "DocumentManagement/ModumateDocument.h"


FObjIDReservationHandle::FObjIDReservationHandle(FModumateDocument* InDocument, int32 InReservingID)
	: Document(InDocument)
	, ReservingID(InReservingID)
	, NextID(MOD_ID_NONE)
{
	if (Document)
	{
		NextID = Document->ReserveNextIDs(ReservingID);
	}
}

FObjIDReservationHandle::~FObjIDReservationHandle()
{
	if (NextID != MOD_ID_NONE)
	{
		Document->SetNextID(NextID, ReservingID);
	}
}
