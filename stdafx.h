#pragma once

#include "targetver.h"

// Disable deprecation of CRT by Microsoft VC Compiler
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
// Windows Header Files
#include <windows.h>

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <time.h>

// Shlwapi.dll provides a handful of the programming elements discussed in the Shell documentation
#include <shlobj.h>
#include <shlwapi.h>

// DirectShow is standalone after DirectX 9 and is packed into Windows SDK 7.0
#include <dshow.h>

// VIDEOINFOHEADER2 structure
#include <Dvdmedia.h>

// ISampleGrabber interface
#define __IDxtCompositor_INTERFACE_DEFINED__
#define __IDxtAlphaSetter_INTERFACE_DEFINED__
#define __IDxtJpeg_INTERFACE_DEFINED__
#define __IDxtKey_INTERFACE_DEFINED__
#include "qedit.h"
