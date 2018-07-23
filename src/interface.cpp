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

#include "lagarith.h"
//#include <commctrl.h>
//#include <shellapi.h>
//#include <Windowsx.h>
//#include <intrin.h>

#define return_badformat() return (DWORD)ICERR_BADFORMAT;
//#define return_badformat() { char msg[256];sprintf(msg,"Returning error on line %d", __LINE__);MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION); return (DWORD)ICERR_BADFORMAT; }

CodecInst::CodecInst() {
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
