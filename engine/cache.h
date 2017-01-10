#pragma once

#ifndef _CLEAR_CHCHE_H
#define _CLEAR_CHCHE_H

#include "common.h"




VOID
Cc_ClearFileCache(
	__in PFILE_OBJECT FileObject,
	__in BOOLEAN bIsFlushCache,
	__in PLARGE_INTEGER FileOffset,
	__in ULONG Length
);



#endif
