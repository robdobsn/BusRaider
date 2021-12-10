// RICUtils
// Rob Dobson 2021

#pragma once

static const int HW_REVISION_NUMBER_UNKNOWN = 0;
static const int HW_REVISION_NUMBER_1 = 1;
static const int HW_REVISION_NUMBER_2_OR_3 = 2;
static const int HW_REVISION_NUMBER_4 = 4;

// Detect RIC hardware revision number
int getRICRevision();
