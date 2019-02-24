// Bus Raider
// Rob Dobson 2019

#pragma once

#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "="  VALUE(var)

#ifndef MEGABYTE
	#define MEGABYTE		0x100000
#endif

#define KERNEL_MAX_SIZE		(10 * MEGABYTE)
#define MEM_SIZE			(256 * MEGABYTE)		// default size
#define GPU_MEM_SIZE		(64 * MEGABYTE)			// set in config.txt
#define ARM_MEM_SIZE		(MEM_SIZE - GPU_MEM_SIZE)	// normally overwritten
#define MEM_HEAP_SIZE   	64 * MEGABYTE

#define PAGE_SIZE			4096				// page size used by us

#define PAGE_TABLE1_SIZE	0x4000

#define MEM_KERNEL_START	0x8000
#define MEM_KERNEL_END		(MEM_KERNEL_START + KERNEL_MAX_SIZE)
#define MEM_PAGE_TABLE1		MEM_KERNEL_END				// must be 16K aligned
#define MEM_PAGE_TABLE1_END	(MEM_PAGE_TABLE1 + PAGE_TABLE1_SIZE)

// coherent memory region (1 section)
#define MEM_COHERENT_REGION	((MEM_PAGE_TABLE1_END + 2*MEGABYTE) & ~(MEGABYTE-1))

// Heap
#define MEM_HEAP_START		(MEM_COHERENT_REGION + MEGABYTE)

// OTA Update area
#define OTA_UPDATE_START    (MEM_HEAP_START + MEM_HEAP_SIZE)
