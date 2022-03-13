#define YYSDK_PLUGIN    // This is needed so the SDK knows not to include core-specific headers. Always define it before including the SDK!
#include "SDK/SDK.hpp"  // Include the SDK.
#include <Windows.h>    // Include Windows's mess.
#include <vector>       // Include the STL vector.

YYRValue EasyGMLCall(YYTKPlugin* pPlugin, const std::string& Name, const std::vector<YYRValue>& rvRef)
{
    // Get the callbuiltin function from the core API
    auto CallBuiltin = pPlugin->GetCoreExport<bool(*)(YYRValue& Result,
        const std::string& Name,
        CInstance* Self,
        CInstance* Other,
        const std::vector<YYRValue>& Args)>("CallBuiltin");

    // Call it like normal
    YYRValue Result;
    CallBuiltin(Result, Name, nullptr, nullptr, rvRef);

    return Result;
}

CCode* GetCodeFromScript(CScript* pScript)
{
    // If the script is invalid
    if (!pScript)
        return nullptr;

    // Old runner versions (pre DR 1.10 / DR 1.09)
    if (pScript->s_code)
        return pScript->s_code;

    // New runners have objects shifted up by 1 (due to the s_text member missing) so cast it to CCode and call it a day.
    if (pScript->s_text)
        return (CCode*)pScript->s_text;

    return nullptr;
}

// Handles all events that happen inside the game.
// Previously, callbacks served this purpose, however in 0.0.3, an object-oriented design was implemented.
// If you want to modify a code entry, you're gonna need this function.
YYTKStatus PluginEventHandler(YYTKPlugin* pPlugin, YYTKEventBase* pEvent)
{
    // Check if the event currently raised is a rendering event (EndScene for DX9, Present for DX11)
    if (pEvent->GetEventType() == EventType::EVT_DOCALLSCRIPT)
    {
        // Convert the base event to the actual event object based on it's type.
        // Tip: Use dynamic_cast to catch issues with exceptions!
        YYTKScriptEvent* pCodeEvent = dynamic_cast<YYTKScriptEvent*>(pEvent);

        // Unpack the pScript variable from the tuple, ignore the rest.
        CScript* pScript = nullptr;

        // Extract arguments from the tuple into individual objects. C++ rules apply.
        std::tie(pScript, std::ignore, std::ignore, std::ignore, std::ignore, std::ignore) = pCodeEvent->Arguments();

        // Check if values are valid
        if (pScript)
        {
            if (CCode* pCode = GetCodeFromScript(pScript))
            {
                if (strcmp(pCode->i_pName, "gml_Script_scr_debug"))
                {
                    // Run GML: variable_global_set("debug", 1.00)
                    EasyGMLCall(pPlugin, "variable_global_set", { "debug", 1.00 });
                }
            }
        }

        // Go To Room port
        if (GetAsyncKeyState(VK_F3) & 1)
        {
            YYRValue CurrentRoom = 30.0;

            YYRValue Result = EasyGMLCall(pPlugin, "get_integer", { "Go to room (ported by Archie from UMT to YYToolkit).\nEnter the room ID you wish to teleport to.", CurrentRoom });

            EasyGMLCall(pPlugin, "room_goto", { Result });
        }
    }
    return YYTK_OK;
}

DllExport YYTKStatus PluginUnload(YYTKPlugin* pPlugin)
{
    EasyGMLCall(pPlugin, "variable_global_set", { "debug", 0.0 });

    return YYTK_OK;
}

// Create an entry routine - it has to be named exactly this, and has to accept these arguments.
// It also has to be declared DllExport (notice how the other functions are not).
DllExport YYTKStatus PluginEntry(YYTKPlugin* pPlugin)
{
    // Set 'PluginEventHandler' as the function to call when we a game event happens.
    // This is not required if you don't need to modify code entries / draw with D3D / anything else that requires precise timing.
    pPlugin->PluginHandler = PluginEventHandler;
    pPlugin->PluginUnload = PluginUnload;
    
    printf("[Chapter2++] - Plugin loaded for YYTK version %s\n", YYSDK_VERSION);
    printf("[Chapter2++] - Note the newest DR version is not fully supported.\n");
    printf("[Chapter2++] - Please report any bugs you encounter on GitHub or the Underminers Discord.\n");

    // Tell the core everything went fine.
    return YYTK_OK;
}

// Boilerplate setup for a Windows DLL, can just return TRUE.
// This has to be here or else you get linker errors (unless you disable the main method)
BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    return 1;
}