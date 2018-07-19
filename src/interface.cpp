//	Lagarith v1.3.25, copyright 2011 by Ben Greenwood.
//	http://lags.leetcode.net/codec.html
//
//	This program is free software; you can redistribute it and/or
//	modify it under the terms of the GNU General Public License
//	as published by the Free Software Foundation; either version 2
//	of the License, or (at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "interface.h"
#include "lagarith.h"
#include <commctrl.h>
#include <shellapi.h>
#include <Windowsx.h>
#include <intrin.h>

#define return_badformat() return (DWORD)ICERR_BADFORMAT;
//#define return_badformat() { char msg[256];sprintf(msg,"Returning error on line %d", __LINE__);MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION); return (DWORD)ICERR_BADFORMAT; }

// Test for MMX, SSE, SSE2, and SSSE3 support
bool DetectFlags() {
	int CPUInfo[4];
	__cpuid(CPUInfo, 1);
	SSSE3 = (CPUInfo[2] & (1 << 9)) != 0;
	return true;
}

CodecInst::CodecInst() {
#ifndef LAGARITH_RELEASE
	if (started == 0x1337) {
		char msg[128];
		sprintf_s(msg, 128, "Attempting to instantiate a codec instance that has not been destroyed");
		MessageBox(HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
	}
#endif

	buffer         = NULL;
	prev           = NULL;
	buffer2        = NULL;
	length         = 0;
	nullframes     = false;
	use_alpha      = false;
	started        = 0;
	cObj.buffer    = NULL;
	multithreading = 0;
	memset(threads, 0, sizeof(threads));
	performance.count  = 0;
	performance.time_a = 0xffffffffffffffff;
	performance.time_b = 0xffffffffffffffff;
}

HMODULE hmoduleLagarith = 0;

void StoreRegistrySettings(bool nullframes, bool suggestrgb, bool multithread, bool noupsample,
                           int mode) {
	DWORD       dp;
	HKEY        regkey;
	const char* ModeStrings[4] = {"RGBA", "RGB", "YUY2", "YV12"};
	if (RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Lagarith", 0, NULL, REG_OPTION_NON_VOLATILE,
	                   KEY_WRITE, NULL, &regkey, &dp) == ERROR_SUCCESS) {
		RegSetValueEx(regkey, "NullFrames", 0, REG_DWORD, (unsigned char*)&nullframes, 4);
		RegSetValueEx(regkey, "SuggestRGB", 0, REG_DWORD, (unsigned char*)&suggestrgb, 4);
		RegSetValueEx(regkey, "Multithread", 0, REG_DWORD, (unsigned char*)&multithread, 4);
		RegSetValueEx(regkey, "NoUpsample", 0, REG_DWORD, (unsigned char*)&noupsample, 4);
		RegSetValueEx(regkey, "Mode", 0, REG_SZ, (unsigned char*)ModeStrings[mode], 4);
		RegCloseKey(regkey);
	}
}

void LoadRegistrySettings(bool* nullframes, bool* suggestrgb, bool* multithread, bool* noupsample,
                          int* mode) {
	HKEY          regkey;
	unsigned char data[] = {0, 0, 0, 0, 0, 0, 0, 0};
	DWORD         size   = sizeof(data);
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Lagarith", 0, KEY_READ, &regkey) ==
	    ERROR_SUCCESS) {
		if (nullframes) {
			RegQueryValueEx(regkey, "NullFrames", 0, NULL, data, &size);
			*nullframes = (data[0] > 0);
			size        = sizeof(data);
			*(int*)data = 0;
		}
		if (suggestrgb) {
			RegQueryValueEx(regkey, "SuggestRGB", 0, NULL, data, &size);
			*suggestrgb = (data[0] > 0);
			size        = sizeof(data);
			*(int*)data = 0;
		}
		if (multithread) {
			RegQueryValueEx(regkey, "Multithread", 0, NULL, data, &size);
			*multithread = (data[0] > 0);
			size         = sizeof(data);
			*(int*)data  = 0;
		}
		if (noupsample) {
			RegQueryValueEx(regkey, "NoUpsample", 0, NULL, data, &size);
			*noupsample = (data[0] > 0);
			size        = sizeof(data);
			*(int*)data = 0;
		}
		if (mode) {
			RegQueryValueEx(regkey, "Mode", 0, NULL, data, &size);

			int cmp = *(int*)data;
			if (cmp == 'ABGR') {
				*mode = 0;
			} else if (cmp == '2YUY') {
				*mode = 2;
			} else if (cmp == '21VY') {
				*mode = 3;
			} else {
				*mode = 1;
			}
		}
		RegCloseKey(regkey);
	} else {
		bool nf      = GetPrivateProfileInt("settings", "nullframes", false, "lagarith.ini") > 0;
		bool nu      = GetPrivateProfileInt("settings", "noupsample", false, "lagarith.ini") > 0;
		bool suggest = GetPrivateProfileInt("settings", "suggest", false, "lagarith.ini") > 0;
		bool mt      = GetPrivateProfileInt("settings", "multithreading", false, "lagarith.ini") > 0;
		int  m       = GetPrivateProfileInt("settings", "lossy_option", 1, "lagarith.ini");
		if (m < 0 || m > 4) {
			m = 1;
		} else if (m == 4) {
			m = 3;
		}
		StoreRegistrySettings(nf, suggest, mt, nu, m);
		if (nullframes)
			*nullframes = nf;
		if (suggestrgb)
			*suggestrgb = suggest;
		if (multithread)
			*multithread = mt;
		if (mode)
			*mode = m;
	}
}

BOOL CodecInst::QueryConfigure() {
	return TRUE;
}

CodecInst* Open(ICOPEN* icinfo) {
	if (icinfo && icinfo->fccType != ICTYPE_VIDEO)
		return NULL;

	CodecInst* pinst = new CodecInst();

	if (icinfo)
		icinfo->dwError = pinst ? ICERR_OK : ICERR_MEMORY;

	return pinst;
}

CodecInst::~CodecInst() {
	try {
		if (started == 0x1337) {
			if (buffer2) {
				CompressEnd();
			} else {
				DecompressEnd();
			}
		}
		started = 0;
	} catch (...) {
	};
}

DWORD Close(CodecInst* pinst) {
	try {
		if (pinst && !IsBadWritePtr(pinst, sizeof(CodecInst))) {
			delete pinst;
		}
	} catch (...) {
	};
	return 1;
}

// Ignore attempts to set/get the codec state but return successful.
// We do not want Lagarith settings to be application specific, but
// some programs assume that the codec is not configurable if GetState
// and SetState are not supported.
DWORD CodecInst::GetState(LPVOID pv, DWORD dwSize) {
	if (pv == NULL) {
		return 1;
	} else if (dwSize < 1) {
		return ICERR_BADSIZE;
	}
	memset(pv, 0, 1);
	return 1;
}

// See GetState comment
DWORD CodecInst::SetState(LPVOID pv, DWORD dwSize) {
	if (pv) {
		return ICERR_OK;
	} else {
		return 1;
	}
}

// check if the codec can compress the given format to the desired format
DWORD CodecInst::CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	// check for valid format and bitdepth
	if (lpbiIn->biCompression == 0) {
		if (lpbiIn->biBitCount != 24 && lpbiIn->biBitCount != 32) {
			return_badformat()
		}
	} else {
		/*char msg[128];
		int x = lpbiIn->biCompression;
		char * y = (char*)&x;
		sprintf(msg,"Unknown format, %c%c%c%c",y[0],y[1],y[2],y[3]);
		MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);*/
		return_badformat()
	}

	int mode = 0;
	LoadRegistrySettings(&nullframes, NULL, &multithreading, NULL, &mode);

	use_alpha = (mode == 0);
	if (mode) {
		mode--;
		if (mode == 1 && lpbiIn->biBitCount <= 16) {
			mode = 0;
		} else if (lpbiIn->biBitCount == 12) {
			mode = 0;
		}
	}

	// Make sure width is mod 4 for YUV formats
	if ((lpbiIn->biBitCount < 24 || mode > 0) && lpbiIn->biWidth % 4) {
		return_badformat()
	}

	// Make sure the height is acceptable for YV12 formats
	if (mode > 1 || lpbiIn->biBitCount < 16) {
		if (lpbiIn->biHeight % 2) {
			return_badformat();
		}
	}

	// See if the output format is acceptable if need be
	if (lpbiOut) {
		if (lpbiOut->biSize < sizeof(BITMAPINFOHEADER))
			return_badformat()

			  if (lpbiOut->biBitCount != 24 && lpbiOut->biBitCount != 32 && lpbiOut->biBitCount != 16 &&
			      lpbiOut->biBitCount != 12) return_badformat() if (lpbiIn->biHeight != lpbiOut->biHeight)
			    return_badformat() if (lpbiIn->biWidth != lpbiOut->biWidth)
			      return_badformat() if (lpbiOut->biCompression != FOURCC_LAGS)
			        return_badformat() if (use_alpha && lpbiIn->biBitCount == 32 &&
			                               lpbiIn->biBitCount != lpbiOut->biBitCount)
			          return_badformat() if (!mode && lpbiIn->biBitCount < lpbiOut->biBitCount &&
			                                 !(lpbiIn->biBitCount == 32 && lpbiOut->biBitCount == 24))
			            return_badformat() if (mode == 1 && lpbiOut->biBitCount < 16) return_badformat()
	}

	if (!DetectFlags()) {
		return_badformat()
	}

	return (DWORD)ICERR_OK;
}

// return the intended compress format for the given input format
DWORD CodecInst::CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	if (!lpbiOut) {
		return sizeof(BITMAPINFOHEADER) + sizeof(UINT32);
	}

	// make sure the input is an acceptable format
	if (CompressQuery(lpbiIn, NULL) == ICERR_BADFORMAT) {
		return_badformat()
	}

	int mode = 0;
	LoadRegistrySettings(&nullframes, NULL, &multithreading, NULL, &mode);

	if (mode) {
		mode--;

		if (mode == 1 && lpbiIn->biBitCount <= 16) {
			mode = 0;
		} else if (lpbiIn->biBitCount == 12) {
			mode = 0;
		}
	}

	*lpbiOut               = *lpbiIn;
	lpbiOut->biSize        = sizeof(BITMAPINFOHEADER) + sizeof(UINT32);
	lpbiOut->biPlanes      = 1;
	lpbiOut->biCompression = FOURCC_LAGS;

	if (lpbiIn->biBitCount != 24) {
		lpbiOut->biSizeImage = lpbiIn->biWidth * lpbiIn->biHeight * lpbiIn->biBitCount / 8;
	} else {
		lpbiOut->biSizeImage =
		  align_round(lpbiIn->biWidth * lpbiIn->biBitCount / 8, 4) * lpbiIn->biHeight;
	}
	lpbiOut->biBitCount = lpbiIn->biBitCount;

	*(UINT32*)(&lpbiOut[1]) = mode;
	return (DWORD)ICERR_OK;
}

// return information about the codec
DWORD CodecInst::GetInfo(ICINFO* icinfo, DWORD dwSize) {
	if (icinfo == NULL)
		return sizeof(ICINFO);

	if (dwSize < sizeof(ICINFO))
		return 0;

	icinfo->dwSize       = sizeof(ICINFO);
	icinfo->fccType      = ICTYPE_VIDEO;
	icinfo->fccHandler   = FOURCC_LAGS;
	icinfo->dwFlags      = VIDCF_FASTTEMPORALC | VIDCF_FASTTEMPORALD;
	icinfo->dwVersion    = 0x00010000;
	icinfo->dwVersionICM = ICVERSION;
	memcpy(icinfo->szName, L"Lagarith", sizeof(L"Lagarith"));
	memcpy(icinfo->szDescription, L"Lagarith Lossless Codec", sizeof(L"Lagarith Lossless Codec"));

	return sizeof(ICINFO);
}

// check if the codec can decompress the given format to the desired format
DWORD CodecInst::DecompressQuery(const LPBITMAPINFOHEADER lpbiIn,
                                 const LPBITMAPINFOHEADER lpbiOut) {
	if (lpbiIn->biCompression != FOURCC_LAGS) {
		return_badformat();
	}

	if (lpbiIn->biBitCount == 16 || lpbiIn->biBitCount == 12) {
		if (lpbiIn->biWidth % 4) {
			return_badformat();
		}
		if (lpbiIn->biBitCount == 12 && lpbiIn->biHeight % 2) {
			return_badformat();
		}
	}

	// make sure the input bitdepth is valid
	if (lpbiIn->biBitCount != 24 && lpbiIn->biBitCount != 32 && lpbiIn->biBitCount != 16 &&
	    lpbiIn->biBitCount != 12) {
		return_badformat();
	}

	if (!DetectFlags()) {
		return_badformat()
	}

	if (!lpbiOut) {
		return (DWORD)ICERR_OK;
	}

	bool noupsample = false;
	LoadRegistrySettings(&nullframes, NULL, &multithreading, &noupsample, NULL);

	unsigned int lossy = 0;
	if (lpbiIn->biSize == sizeof(BITMAPINFOHEADER) + 4) {
		lossy = *(UINT32*)(&lpbiIn[1]);
		if (lossy == 3) {
			lossy = 2;
		} else if (lossy > 3) {
			lossy = 0;
		}
	}
	unsigned int mode = lossy;
	(void)mode;

	//char msg[128];
	//char fcc[4];
	//*(unsigned int*)(&fcc[0])=lpbiOut->biCompression;
	//sprintf(msg,"Format = %d, BiComp= %c%c%c%c",lpbiOut->biBitCount,fcc[3],fcc[2],fcc[1],fcc[0] );
	//MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);

	// make sure the output bitdepth is valid
	if (lpbiOut->biBitCount != 32 && lpbiOut->biBitCount != 24 && lpbiOut->biBitCount != 16 &&
	    lpbiOut->biBitCount != 12) {
		return_badformat();
	}
	// make sure that no down sampling is being performed
	if (!lossy) {
		if (lpbiOut->biBitCount < 24 && lpbiOut->biBitCount < lpbiIn->biBitCount) {
			return_badformat();
		}
	} else {
		if (lossy == 1 && lpbiOut->biBitCount < 16) {
			return_badformat();
		} else if (lpbiOut->biBitCount < 12) {
			return_badformat();
		}
	}

	if (noupsample) {
		// make sure no colorspace conversion is being done
		if (!lossy) {
			if (lpbiIn->biBitCount < RGB24 && lpbiOut->biBitCount != lpbiIn->biBitCount) {
				return_badformat();
			}
			if (lpbiIn->biBitCount >= RGB24 && lpbiOut->biBitCount != RGB32) {
				return_badformat();
			}
		} else {
			if (lossy == 1 && lpbiOut->biBitCount != 16) {
				return_badformat();
			} else if (lossy == 2 && lpbiOut->biBitCount != 12) {
				return_badformat();
			}
		}
	}

	// check for invalid widths/heights
	if (lossy > 0 || lpbiOut->biBitCount < 24) {
		if (lpbiIn->biWidth % 2)
			return_badformat();
		if (lpbiIn->biWidth % 4) {
			if (lossy == 1 && lpbiOut->biBitCount != 16)
				return_badformat();
			if (lossy == 2 && lpbiOut->biBitCount != 12)
				return_badformat();
			if (lossy > 2)
				return_badformat();
		}
		if (lossy > 1 || lpbiOut->biBitCount < 16) {
			if (lpbiIn->biHeight % 2) {
				return_badformat();
			}
		}
	}

	if (lpbiOut->biCompression == 0 && lpbiOut->biBitCount != 24 && lpbiOut->biBitCount != 32) {
		return_badformat();
	}
	if (lpbiIn->biHeight != lpbiOut->biHeight)
		return_badformat();
	if (lpbiIn->biWidth != lpbiOut->biWidth)
		return_badformat();

	return (DWORD)ICERR_OK;
}

// return the default decompress format for the given input format
DWORD CodecInst::DecompressGetFormat(const LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	if (DecompressQuery(lpbiIn, NULL) != ICERR_OK) {
		return_badformat();
	}

	if (!lpbiOut)
		return sizeof(BITMAPINFOHEADER);

	bool suggest    = false;
	bool noupsample = false;
	LoadRegistrySettings(&nullframes, &suggest, &multithreading, &noupsample, NULL);

	unsigned int lossy = 0;
	if (lpbiIn->biSize == sizeof(BITMAPINFOHEADER) + 4) {
		lossy = *(UINT32*)(&lpbiIn[1]);
		if (lossy >= 4)
			lossy = 0;
		if (lossy == 3)
			lossy = 2;
	}
	unsigned int mode = lossy;
	(void)mode;

	*lpbiOut          = *lpbiIn;
	lpbiOut->biSize   = sizeof(BITMAPINFOHEADER);
	lpbiOut->biPlanes = 1;

	// suggest RGB32 if source is RGB, or if "always suggest RGB" is selected
	if ((!lossy && (lpbiIn->biBitCount == 24 || lpbiIn->biBitCount == 32)) || suggest) {
		lpbiOut->biBitCount    = 32;
		lpbiOut->biSizeImage   = lpbiIn->biWidth * lpbiIn->biHeight * 4;
		lpbiOut->biCompression = 0;
	} else {
		return_badformat();
	}

	return (DWORD)ICERR_OK;
}

DWORD CodecInst::DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	return_badformat()
}

#if 0
// DLL MAIN NOT USED - this is just to verify completeness of code!
__declspec(dllexport) 
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
  hmoduleLagarith = (HMODULE)hinstDLL;

  DWORD_PTR dwDriverID = 0;
  //HDRVR hDriver = nullptr;
  UINT uiMessage = (UINT)fdwReason;
  LPARAM lParam1 = (LPARAM)lpvReserved;
  LPARAM lParam2 = 0;

  CodecInst* pi = (CodecInst*)dwDriverID;
  switch ( uiMessage )
  {
  case DRV_LOAD:
    return (LRESULT)1L;

  case DRV_FREE:
    return (LRESULT)1L;

  case DRV_OPEN:
    return (BOOL)(LRESULT)Open( (ICOPEN*)lParam2 );

  case DRV_CLOSE:
    if ( pi ) Close( pi );
    return (LRESULT)1L;

    /*********************************************************************

    state messages

    *********************************************************************/

    // cwk
  case DRV_QUERYCONFIGURE:    // configuration from drivers applet
    return (LRESULT)1L;

  case ICM_ABOUT:
    return ICERR_UNSUPPORTED;

  case ICM_GETSTATE:
    return pi->GetState( (LPVOID)lParam1, (DWORD)lParam2 );

  case ICM_SETSTATE:
    return pi->SetState( (LPVOID)lParam1, (DWORD)lParam2 );

  case ICM_GETINFO:
    return pi->GetInfo( (ICINFO*)lParam1, (DWORD)lParam2 );

  case ICM_GETDEFAULTQUALITY:
    if ( lParam1 )
    {
      *((LPDWORD)lParam1) = 10000;
      return ICERR_OK;
    }
    break;

    /*********************************************************************

    compression messages

    *********************************************************************/

  case ICM_COMPRESS_QUERY:
    return pi->CompressQuery( (LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2 );

  case ICM_COMPRESS_BEGIN:
    return pi->CompressBegin( (LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2 );

  case ICM_COMPRESS_GET_FORMAT:
    return pi->CompressGetFormat( (LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2 );

  case ICM_COMPRESS_GET_SIZE:
    return pi->CompressGetSize( (LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2 );

  case ICM_COMPRESS:
    return pi->Compress( (ICCOMPRESS*)lParam1, (DWORD)lParam2 );

  case ICM_COMPRESS_END:
    return pi->CompressEnd();

    /*********************************************************************

    decompress messages

    *********************************************************************/

  case ICM_DECOMPRESS_QUERY:

    return pi->DecompressQuery( (LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2 );

  case ICM_DECOMPRESS_BEGIN:
    return  pi->DecompressBegin( (LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2 );

  case ICM_DECOMPRESS_GET_FORMAT:
    return pi->DecompressGetFormat( (LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2 );

  case ICM_DECOMPRESS_GET_PALETTE:
    return pi->DecompressGetPalette( (LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2 );

  case ICM_DECOMPRESS:
    return pi->Decompress( (ICDECOMPRESS*)lParam1, (DWORD)lParam2 );

  case ICM_DECOMPRESS_END:
    return pi->DecompressEnd();

    /*********************************************************************

    standard driver messages

    *********************************************************************/

  case DRV_DISABLE:
  case DRV_ENABLE:
    return (LRESULT)1L;

  case DRV_INSTALL:
  case DRV_REMOVE:
    return (LRESULT)DRV_OK;
  }

  //if ( uiMessage < DRV_USER )
  //  return (BOOL)DefDriverProc( dwDriverID, hDriver, uiMessage, lParam1, lParam2 );
  return ICERR_UNSUPPORTED;
}
#endif // 0

//MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
