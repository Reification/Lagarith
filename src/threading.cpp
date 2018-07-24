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
#include "lagarith_internal.h"

namespace Lagarith {
static DWORD WINAPI encode_worker_thread(LPVOID i) {
	ThreadData* threaddata = (ThreadData*)i;

	WaitForSingleObject(threaddata->StartEvent, INFINITE);

	const unsigned int   width  = threaddata->width;
	unsigned char* const buffer = (unsigned char*)threaddata->buffer;
	assert(buffer != NULL);

	while (true) {
		unsigned int length = threaddata->length;
		if (length == 0xFFFFFFFF) {
			break;
		} else if (length > 0) {
			unsigned char* src  = (unsigned char*)threaddata->source;
			unsigned char* dest = (unsigned char*)threaddata->dest;
			unsigned char* dst  = buffer + ((uintptr_t)src & 15);

			Block_Predict(src, dst, width, length, /*rgbmode=*/true);

			threaddata->size = threaddata->cObj.Compact(dst, dest, length);
			assert(*(__int64*)dest != 0);
			threaddata->length = 0;
		} else {
			assert(false);
		}

		SignalObjectAndWait(threaddata->DoneEvent, threaddata->StartEvent, INFINITE, false);
	}
	threaddata->cObj.FreeCompressBuffers();
	lag_aligned_free(threaddata->buffer, "Thread Buffer");
	threaddata->length = 0;
	return 0;
}

static DWORD WINAPI decode_worker_thread(LPVOID i) {
	ThreadData* threaddata = (ThreadData*)i;

	WaitForSingleObject(threaddata->StartEvent, INFINITE);
	while (true) {
		unsigned int length = threaddata->length;

		if (length == 0xFFFFFFFF) {
			break;
		} else if (length > 0) {
			unsigned char* src  = (unsigned char*)threaddata->source;
			unsigned char* dest = (unsigned char*)threaddata->dest;
			threaddata->cObj.Uncompact(src, dest, length);
			threaddata->length = 0;
		} else {
			assert(false);
		}
		SignalObjectAndWait(threaddata->DoneEvent, threaddata->StartEvent, INFINITE, false);
	}
	threaddata->cObj.FreeCompressBuffers();
	threaddata->length = 0;
	return 0;
}

bool Codec::InitThreads(int encode) {
	const unsigned int use_format = format;
	DWORD              temp       = 0;

	assert(width && height && "CompressBegin/DecompressBegin not called!");

	if (!width || !height) {
		return false;
	}

	threads[0].StartEvent = CreateEvent(NULL, false, false, NULL);
	threads[0].DoneEvent  = CreateEvent(NULL, false, false, NULL);

	threads[0].width  = width;
	threads[0].height = height;
	threads[0].format = use_format;
	threads[0].length = 0;
	threads[0].thread = NULL;
	threads[0].buffer = NULL;

	threads[1].StartEvent = CreateEvent(NULL, false, false, NULL);
	threads[1].DoneEvent  = CreateEvent(NULL, false, false, NULL);
	threads[1].width      = width;
	threads[1].height     = height;
	threads[1].format     = use_format;
	threads[1].length     = 0;
	threads[1].thread     = NULL;
	threads[1].buffer     = NULL;

	int buffer_size = align_round(width, 16) * height + 256;

	bool memerror = false;
	bool interror = false;

	if (encode) {
		if (!threads[0].cObj.InitCompressBuffers(buffer_size) ||
		    !threads[1].cObj.InitCompressBuffers(buffer_size)) {
			memerror = true;
		}
	}
	if (!memerror) {
		if (!interror && !memerror) {
			if (encode) {
				threads[0].thread =
				  CreateThread(NULL, 0, encode_worker_thread, &threads[0], CREATE_SUSPENDED, &temp);
				threads[1].thread =
				  CreateThread(NULL, 0, encode_worker_thread, &threads[1], CREATE_SUSPENDED, &temp);
			} else {
				threads[0].thread =
				  CreateThread(NULL, 0, decode_worker_thread, &threads[0], CREATE_SUSPENDED, &temp);
				threads[1].thread =
				  CreateThread(NULL, 0, decode_worker_thread, &threads[1], CREATE_SUSPENDED, &temp);
			}
			if (!threads[0].thread || !threads[1].thread) {
				interror = true;
			} else {
				SetThreadPriority(threads[0].thread, THREAD_PRIORITY_BELOW_NORMAL);
				SetThreadPriority(threads[1].thread, THREAD_PRIORITY_BELOW_NORMAL);
			}
		}
	}

	if (!memerror && !interror && encode) {
		threads[0].buffer = (unsigned char*)lag_aligned_malloc((void*)threads[0].buffer, buffer_size,
		                                                       16, "threads[0].buffer");
		threads[1].buffer = (unsigned char*)lag_aligned_malloc((void*)threads[1].buffer, buffer_size,
		                                                       16, "threads[1].buffer");
		if (threads[0].buffer == NULL || threads[1].buffer == NULL) {
			memerror = true;
		}
	}

	if (memerror || interror) {
		if (encode) {
			lag_aligned_free(threads[0].buffer, "threads[0].buffer");
			lag_aligned_free(threads[1].buffer, "threads[1].buffer");
			threads[0].cObj.FreeCompressBuffers();
			threads[1].cObj.FreeCompressBuffers();
		}

		threads[0].thread = NULL;
		threads[1].thread = NULL;

	  return false;
	}

	ResumeThread(threads[0].thread);
	ResumeThread(threads[1].thread);
	
	return true;
}

void Codec::EndThreads() {
	threads[0].length = 0xFFFFFFFF;
	threads[1].length = 0xFFFFFFFF;

	HANDLE events[2] = {threads[0].thread, threads[1].thread};

	if (threads[0].thread) {
		SetEvent(threads[0].StartEvent);
	}
	if (threads[1].thread) {
		SetEvent(threads[1].StartEvent);
	}

	WaitForMultipleObjectsEx(2, &events[0], true, 10000, true);

	if (!CloseHandle(threads[0].thread)) {
		/*LPVOID lpMsgBuf;
		FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM | 
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			0, // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL 
		);
		MessageBox( NULL, (char *)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
		LocalFree( lpMsgBuf );*/
		//MessageBox (HWND_DESKTOP, "CloseHandle failed for thread A", "Error", MB_OK | MB_ICONEXCLAMATION);
	}

	if (!CloseHandle(threads[1].thread)) {
		//	MessageBox (HWND_DESKTOP, "CloseHandle failed for thread B", "Error", MB_OK | MB_ICONEXCLAMATION);
	}

	threads[0].thread = NULL;
	threads[1].thread = NULL;

	threads[0].length = 0;
	threads[1].length = 0;
}

} // namespace Lagarith
