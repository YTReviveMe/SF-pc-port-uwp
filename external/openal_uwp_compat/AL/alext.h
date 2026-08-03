#pragma once

#include <alext.h>

// Older OpenAL headers bundled with several UWP ports predate the callback
// buffer extension. The shipped OpenAL Soft runtime exports it through
// alGetProcAddress, so supply the ABI declaration when the header lacks it.
#ifndef AL_SOFT_callback_buffer
#define AL_SOFT_callback_buffer 1
typedef ALsizei (AL_APIENTRY* ALBUFFERCALLBACKTYPESOFT)(
    ALvoid* userptr, ALvoid* sampledata, ALsizei numbytes);
typedef void (AL_APIENTRY* LPALBUFFERCALLBACKSOFT)(
    ALuint buffer, ALenum format, ALsizei frequency,
    ALBUFFERCALLBACKTYPESOFT callback, ALvoid* userptr);
#endif
