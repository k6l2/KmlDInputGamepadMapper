#include "DInputGamepadMapper.h"
#pragma warning( push )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	#include "imgui/imgui.h"
#pragma warning( pop )
#include "gen_kassets.h"
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	g_kpl = &memory.kpl;
	kassert(sizeof(GameState) <= memory.permanentMemoryBytes);
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
	g_krb = &memory.krb;
	ImGui::SetCurrentContext(
		reinterpret_cast<ImGuiContext*>(memory.imguiContext));
	ImGui::SetAllocatorFunctions(memory.platformImguiAlloc, 
	                             memory.platformImguiFree, 
	                             memory.imguiAllocUserData);
}
GAME_INITIALIZE(gameInitialize)
{
	// GameState memory initialization //
	*g_gs = {};
	// initialize dynamic allocators //
	g_gs->hKgaPermanent = 
		kgaInit(reinterpret_cast<u8*>(memory.permanentMemory) + 
		            sizeof(GameState), 
		        memory.permanentMemoryBytes - sizeof(GameState));
	g_gs->hKgaTransient = kgaInit(memory.transientMemory, 
	                              memory.transientMemoryBytes);
	// construct a linear frame allocator //
	{
		const size_t kalFrameSize = kmath::megabytes(5);
		void*const kalFrameStartAddress = 
			kgaAlloc(g_gs->hKgaPermanent, kalFrameSize);
		g_gs->hKalFrame = kalInit(kalFrameStartAddress, kalFrameSize);
	}
	// Contruct/Initialize the game's AssetManager //
	g_gs->assetManager = kamConstruct(g_gs->hKgaPermanent, KASSET_COUNT,
	                                  g_gs->hKgaTransient, g_kpl, g_krb);
	// Initialize the game's audio mixer //
	g_gs->kAudioMixer = kauConstruct(g_gs->hKgaPermanent, 16, 
	                                 g_gs->assetManager);
	// Tell the asset manager to load assets asynchronously! //
	kamPushAllKAssets(g_gs->assetManager);
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kauMix(g_gs->kAudioMixer, audioBuffer, sampleBlocksConsumed);
}
/** 
 * @return numGamePads if there is either no active game pad OR if there are 
 *         multiple active game pads
*/
internal u8 padMapperFirstActiveControllerIndex(GamePad* gamePadArray, 
                                                u8 numGamePads)
{
	u8 result = numGamePads;
	for(u8 c = 0; c < numGamePads; c++)
	{
		GamePad& gpad = gamePadArray[c];
		if(gpad.type == GamePadType::UNPLUGGED 
			|| gpad.type == GamePadType::XINPUT)
		{
			continue;
		}
		const u16 gpActiveButton = g_kpl->getGamePadActiveButton(c);
		const u16 gpActiveAxis   = g_kpl->getGamePadActiveAxis(c);
		if(gpActiveButton != INVALID_PLATFORM_BUTTON_INDEX
			|| gpActiveAxis != INVALID_PLATFORM_AXIS_INDEX)
		{
			if(result == numGamePads)
			{
				result = c;
			}
			else
			/* if more than one gamepad is active, then there isn't a unique 
				active game pad! */
			{
				return numGamePads;
			}
		}
	}
	return result;
}
local_persist const char* BUTTON_NAMES[] =
	{ "face-button Up"
	, "face-button Down"
	, "face-button Left"
	, "face-button Right"
	, "start/pause"
	, "back/select"
	, "d-pad Up"
	, "d-pad Down"
	, "d-pad Left"
	, "d-pad Right"
	, "shoulder left"
	, "shoulder right"
	, "stick click left"
	, "stick click right"
};
internal void padMapperImGuiDisplayPadInfo()
{
	ImGui::Text("%s{%s}", g_gs->activeGamePadProductName, 
	            g_gs->activeGamePadProductGuid);
	for(u8 b = 0; b < g_gs->nextToMapButton; b++)
	{
		ImGui::Text("button[%i]->'%s'", g_gs->buttonMap[b], BUTTON_NAMES[b]);
	}
}
#include <cstdio>
internal void padMapperSaveMapToClipboard()
{
	size_t currChar = 0;
	currChar += sprintf_s(g_gs->padMapOutputBuffer + currChar, 
	                      CARRAY_COUNT(g_gs->padMapOutputBuffer) - currChar,
	                      "%s{%s}", g_gs->activeGamePadProductName, 
	                      g_gs->activeGamePadProductGuid);
	for(u8 b = 0; b < g_gs->nextToMapButton; b++)
	{
		if(b > 0)
			currChar += 
				sprintf_s(g_gs->padMapOutputBuffer + currChar, 
				          CARRAY_COUNT(g_gs->padMapOutputBuffer) - currChar,
				          ",");
		currChar += sprintf_s(g_gs->padMapOutputBuffer + currChar, 
		                      CARRAY_COUNT(g_gs->padMapOutputBuffer) - currChar,
		                      "b%i:%i", g_gs->buttonMap[b], b);
		bool duplicateButtonFound = false;
		for(u8 bb = static_cast<u8>(b + 1); bb < g_gs->nextToMapButton; bb++)
		{
			if(g_gs->buttonMap[b] == g_gs->buttonMap[bb])
			{
				duplicateButtonFound = true;
				break;
			}
		}
		if(duplicateButtonFound)
			currChar += 
				sprintf_s(g_gs->padMapOutputBuffer + currChar, 
				          CARRAY_COUNT(g_gs->padMapOutputBuffer) - currChar,
				          "!");
	}
	ImGui::SetClipboardText(g_gs->padMapOutputBuffer);
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	kalReset(g_gs->hKalFrame);
	if(windowIsFocused)
	{
		if(gameKeyboard.escape == ButtonState::PRESSED 
			|| (gameKeyboard.f4 == ButtonState::PRESSED 
				&& gameKeyboard.modifiers.alt))
		{
			return false;
		}
		if(gameKeyboard.enter == ButtonState::PRESSED 
			&& gameKeyboard.modifiers.alt)
		{
			g_kpl->setFullscreen(!g_kpl->isFullscreen());
		}
	}
	/* hot-reload all assets which have been reported to be changed by the 
		platform layer (newer file write timestamp) */
	if(kamUnloadChangedAssets(g_gs->assetManager))
		kamPushAllKAssets(g_gs->assetManager);
	if(ImGui::BeginMainMenuBar())
	{
		if(ImGui::MenuItem("RESET"))
		{
			g_gs->gamePadMapperState = GamePadMapperState::WAITING_TO_ACQUIRE;
		}
		ImGui::EndMainMenuBar();
	}
	local_persist const f32 HOLD_SECONDS_REQUIRED = 1;// previously: 2
	if(ImGui::Begin("Controller Mapper", nullptr, 
	                ImGuiWindowFlags_AlwaysAutoResize))
	{
		switch(g_gs->gamePadMapperState)
		{
			case GamePadMapperState::WAITING_TO_ACQUIRE:
			{
				ImGui::Text("Press & hold any button or axis on any "
				            "controller.");
				g_gs->activeGamePad = 
					padMapperFirstActiveControllerIndex(gamePadArray, 
					                                    numGamePads);
				if(g_gs->activeGamePad != numGamePads)
				{
					g_gs->gamePadButtonOrAxisHoldSeconds = 0;
					g_gs->gamePadMapperState = GamePadMapperState::ACQUIRING;
				}
			} break;
			case GamePadMapperState::ACQUIRING:
			{
				g_gs->gamePadButtonOrAxisHoldSeconds += deltaSeconds;
				if(g_gs->gamePadButtonOrAxisHoldSeconds < HOLD_SECONDS_REQUIRED)
				{
					const f32 ratioHoldSeconds = 
						g_gs->gamePadButtonOrAxisHoldSeconds / 
						HOLD_SECONDS_REQUIRED;
					ImGui::ProgressBar(ratioHoldSeconds, ImVec2(300, 0), 
					                   "ACQUIRING...");
					const u8 activeGamePad = 
						padMapperFirstActiveControllerIndex(gamePadArray, 
						                                    numGamePads);
					if(activeGamePad != g_gs->activeGamePad)
					{
						g_gs->gamePadMapperState = 
							GamePadMapperState::WAITING_TO_ACQUIRE;
						break;
					}
				}
				else /* g_gs->gamePadButtonOrAxisHoldSeconds >= 
				        HOLD_SECONDS_REQUIRED */
				{
					const u8 activeGamePad = 
						padMapperFirstActiveControllerIndex(gamePadArray, 
						                                    numGamePads);
					if(activeGamePad != numGamePads)
					{
						ImGui::PushStyleColor(ImGuiCol_PlotHistogram, 
						                      ImVec4(0,1,0,1));
						ImGui::ProgressBar(1.f, ImVec2(300, 0), "ACQUIRED!");
						ImGui::PopStyleColor();
						break;
					}
					g_gs->gamePadButtonOrAxisHoldSeconds = 
						HOLD_SECONDS_REQUIRED;
					g_kpl->getGamePadProductGuid(g_gs->activeGamePad, 
					              g_gs->activeGamePadProductGuid, 
					              CARRAY_COUNT(g_gs->activeGamePadProductGuid));
					g_kpl->getGamePadProductName(g_gs->activeGamePad, 
					              g_gs->activeGamePadProductName, 
					              CARRAY_COUNT(g_gs->activeGamePadProductName));
					g_gs->nextToMapButton = 0;
					g_gs->nextToMapAxis   = 0;
					g_gs->gamePadMapperState = 
						GamePadMapperState::WAITING_FOR_NEXT_ACTIVE_BUTTON;
				}
			} break;
			case GamePadMapperState::WAITING_FOR_NEXT_ACTIVE_BUTTON:
			{
				padMapperImGuiDisplayPadInfo();
				ImGui::Text("Press & hold the '%s' button.", 
				            BUTTON_NAMES[g_gs->nextToMapButton]);
				g_gs->activeButtonIndex = 
					g_kpl->getGamePadActiveButton(g_gs->activeGamePad);
				if(g_gs->activeButtonIndex != INVALID_PLATFORM_BUTTON_INDEX)
				{
					g_gs->gamePadButtonOrAxisHoldSeconds = 0;
					g_gs->gamePadMapperState = 
						GamePadMapperState::ACQUIRING_NEXT_ACTIVE_BUTTON;
				}
			} break;
			case GamePadMapperState::ACQUIRING_NEXT_ACTIVE_BUTTON:
			{
				g_gs->gamePadButtonOrAxisHoldSeconds += deltaSeconds;
				if(g_gs->gamePadButtonOrAxisHoldSeconds < HOLD_SECONDS_REQUIRED)
				{
					const f32 ratioHoldSeconds = 
						g_gs->gamePadButtonOrAxisHoldSeconds / 
						HOLD_SECONDS_REQUIRED;
					ImGui::ProgressBar(ratioHoldSeconds, ImVec2(300, 0), 
					                   "ACQUIRING...");
					const u16 activeButtonIndex = 
						g_kpl->getGamePadActiveButton(g_gs->activeGamePad);
					if(activeButtonIndex != g_gs->activeButtonIndex)
					{
						g_gs->gamePadMapperState = 
							GamePadMapperState::WAITING_FOR_NEXT_ACTIVE_BUTTON;
						break;
					}
				}
				else /* g_gs->gamePadButtonOrAxisHoldSeconds >= 
				            HOLD_SECONDS_REQUIRED */
				{
					const u16 activeButtonIndex = 
						g_kpl->getGamePadActiveButton(g_gs->activeGamePad);
					if(activeButtonIndex != INVALID_PLATFORM_BUTTON_INDEX)
					{
						ImGui::PushStyleColor(ImGuiCol_PlotHistogram, 
						                      ImVec4(0,1,0,1));
						ImGui::ProgressBar(1.f, ImVec2(300, 0), "ACQUIRED!");
						ImGui::PopStyleColor();
						break;
					}
					g_gs->gamePadButtonOrAxisHoldSeconds = 
						HOLD_SECONDS_REQUIRED;
					g_gs->buttonMap[g_gs->nextToMapButton] = 
						g_gs->activeButtonIndex;
					g_gs->nextToMapButton++;
					if(g_gs->nextToMapButton >= CARRAY_COUNT(g_gs->buttonMap))
					{
						g_gs->gamePadMapperState = 
							GamePadMapperState::WAITING_FOR_NEXT_ACTIVE_AXIS;
					}
					else
					{
						g_gs->gamePadMapperState = 
							GamePadMapperState::WAITING_FOR_NEXT_ACTIVE_BUTTON;
					}
				}
			} break;
			case GamePadMapperState::WAITING_FOR_NEXT_ACTIVE_AXIS:
			{
				padMapperImGuiDisplayPadInfo();
				if(ImGui::Button("Copy To Clipboard"))
				{
					padMapperSaveMapToClipboard();
				}
			} break;
			case GamePadMapperState::ACQUIRING_NEXT_ACTIVE_AXIS:
			{
			} break;
		}
	}
	ImGui::End();// "Controller Mapper"
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	return true;
}
#include "kFlipBook.cpp"
#include "kAudioMixer.cpp"
#pragma warning(push)
	// warning C4296: '<': expression is always false
	#pragma warning( disable : 4296 )
	#include "kAssetManager.cpp"
#pragma warning(pop)
#include "kGeneralAllocator.cpp"
#include "kAllocatorLinear.cpp"
#pragma warning( push )
	// warning C4127: conditional expression is constant
	#pragma warning( disable : 4127 )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	// warning C4365: 'argument': conversion
	#pragma warning( disable : 4365 )
	// warning C4577: 'noexcept' used with no exception handling mode 
	//                specified...
	#pragma warning( disable : 4577 )
	// warning C4774: 'sscanf' : format string expected in argument 2 is not a 
	//                string literal
	#pragma warning( disable : 4774 )
	#include "imgui/imgui_demo.cpp"
	#include "imgui/imgui_draw.cpp"
	#include "imgui/imgui_widgets.cpp"
	#include "imgui/imgui.cpp"
#pragma warning( pop )
#define STB_DS_IMPLEMENTATION
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize)
{
	return kgaRealloc(g_gs->hKgaPermanent, allocatedAddress, newAllocationSize);
}
internal void kStbDsFree(void* allocatedAddress)
{
	kgaFree(g_gs->hKgaPermanent, allocatedAddress);
}
#pragma warning( push )
	// warning C4365: 'argument': conversion
	#pragma warning( disable : 4365 )
	// warning C4456: declaration of 'i' hides previous local declaration
	#pragma warning( disable : 4456 )
	#include "stb/stb_ds.h"
#pragma warning( pop )