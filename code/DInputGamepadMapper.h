#pragma once
#include "global-defines.h"
#if defined(KLOG)
	#undef KLOG
	#define KLOG(platformLogCategory, formattedString, ...) g_kpl->log(\
	    __FILENAME__, __LINE__, PlatformLogCategory::K_##platformLogCategory, \
	    formattedString, ##__VA_ARGS__)
#endif
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "kGeneralAllocator.h"
#include "kAssetManager.h"
#include "kAudioMixer.h"
#include "kFlipBook.h"
#include "kAllocatorLinear.h"
#define STBDS_REALLOC(context,ptr,size) kStbDsRealloc(ptr,size)
#define STBDS_FREE(context,ptr)         kStbDsFree(ptr)
#include "stb/stb_ds.h"
enum class GamePadMapperState : u8
	{ WAITING_TO_ACQUIRE
	, ACQUIRING
	, WAITING_FOR_NEXT_ACTIVE_BUTTON
	, ACQUIRING_NEXT_ACTIVE_BUTTON
	, WAITING_FOR_NEXT_ACTIVE_AXIS
	, ACQUIRING_NEXT_ACTIVE_AXIS
};
struct GameState
{
	KAssetManager* assetManager;
	KAudioMixer* kAudioMixer;
	KgaHandle hKgaPermanent;
	KgaHandle hKgaTransient;
	KalHandle hKalFrame;
	/* Gamepad Mapper state */
	GamePadMapperState gamePadMapperState;
	f32 gamePadButtonOrAxisHoldSeconds;
	u8 activeGamePad;
	char activeGamePadProductGuid[256];
	char activeGamePadProductName[256];
	u8 nextToMapButton;
	u8 nextToMapAxis;
	u16 activeButtonIndex;
	u16 activeAxisIndex;
	u16 buttonMap[CARRAY_COUNT(GamePad::buttons)];
	char padMapOutputBuffer[16 * 1024];
};
global_variable GameState* g_gs;
global_variable KmlPlatformApi* g_kpl;
global_variable KrbApi* g_krb;
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize);
internal void  kStbDsFree(void* allocatedAddress);