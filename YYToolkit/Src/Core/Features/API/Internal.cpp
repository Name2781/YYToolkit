#include "API.hpp"
#include "../../Utils/Error/Error.hpp"
#include "../PluginManager/PluginManager.hpp"
#include "../../Utils/MH/hde/hde32.h"
#include "../UnitTests/UnitTests.hpp"
#ifdef min
#undef min
#endif

YYTKStatus API::Internal::Initialize(HMODULE hMainModule)
{
	// Don't forget this!
	gAPIVars.Globals.g_hMainModule = hMainModule;

	if (GetAsyncKeyState(VK_F8) & 1 || API::gAPIVars.Globals.g_bDebugMode)
	{
		// Allocate console
		AllocConsole();
		FILE* fDummy;
		freopen_s(&fDummy, "CONIN$", "r", stdin);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);
		freopen_s(&fDummy, "CONOUT$", "w", stdout);

		SetConsoleTitleA("YYToolkit Debug Console");
	}

	// Print the version into the log
#if _DEBUG
	Utils::Error::Message(CLR_GOLD, "YYToolkit %s (Debug) by Archie#8615", YYSDK_VERSION);
#else
	Utils::Error::Message(CLR_LIGHTBLUE, "YYToolkit %s (Release) by Archie#8615", YYSDK_VERSION);
#endif

	// Initialize Code_Function_GET_the_function
	{
		YYTKStatus Status = API::Internal::MmFindCodeFunction(reinterpret_cast<DWORD&>(gAPIVars.Functions.Code_Function_GET_the_function));

		if (Status && gAPIVars.Globals.g_bDebugMode)
			Utils::Error::Message(CLR_TANGERINE, "[Warning] API::Internal::MmFindCodeFunction() failed. Error: %s", 
				Utils::Error::YYTKStatus_ToString(Status).c_str());
	}

	// Initialize Code_Execute
	{
		YYTKStatus Status = API::Internal::MmFindCodeExecute(reinterpret_cast<DWORD&>(gAPIVars.Functions.Code_Execute));

		if (Status && gAPIVars.Globals.g_bDebugMode)
			Utils::Error::Message(CLR_TANGERINE, "[Warning] API::Internal::MmFindCodeExecute() failed. Error: %s",
				Utils::Error::YYTKStatus_ToString(Status).c_str());
	}

	// Initialize g_hwWindowHandle
	{
		CInstance tmp;
		memset(&tmp, 0, sizeof(tmp));

		YYRValue Result;
		bool Success = API::CallBuiltin(Result, "window_handle", &tmp, &tmp, {});

		if (!Success && gAPIVars.Globals.g_bDebugMode)
			Utils::Error::Message(CLR_TANGERINE, "[Warning] API::CallBuiltin(\"window_handle\") failed.");

		API::gAPIVars.Globals.g_hwWindowHandle = reinterpret_cast<HWND>(Result.As<RValue>().Pointer);
	}

	// Initialize g_pGlobalInstance
	{
		CInstance tmp;
		memset(&tmp, 0, sizeof(tmp));

		YYRValue Result;
		bool Success = API::CallBuiltin(Result, "@@GlobalScope@@", &tmp, &tmp, {});

		if (!Success && gAPIVars.Globals.g_bDebugMode)
			Utils::Error::Message(CLR_TANGERINE, "[Warning] API::CallBuiltin(\"@@GlobalScope@@\") failed.");

		API::gAPIVars.Globals.g_pGlobalObject = Result;
	}
	
	// Initialize g_WindowDevice
	{
		YYRValue Result;
		bool Success = API::CallBuiltin(Result, "window_device", nullptr, nullptr, {});

		if (!Success && gAPIVars.Globals.g_bDebugMode)
			Utils::Error::Message(CLR_TANGERINE, "[Warning] API::CallBuiltin(\"window_device\") failed.");

		API::gAPIVars.Globals.g_pWindowDevice = Result.As<RValue>().Pointer;
	}

	bool bPassedTests = Tests::RunUnitTests();

	if (!bPassedTests)
	{
		int nUserChoice = MessageBoxA(
			0,
			"YYToolkit's self-check failed to pass.\n"
			"This means that some features may be broken or completely unavailable.\n"
			"Plugins relying on this functionality may cease to work.\n\n"
			"Proceed anyway?",
			"YYToolkit",
			MB_TOPMOST | MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND | MB_DEFBUTTON2
		);

		if (nUserChoice == IDNO)
			exit(0);
	}

	PluginManager::Initialize(); // Remove me to turn off plugin functionality.

	return YYTK_OK;
}

YYTKStatus API::Internal::Unload()
{
	PluginManager::Uninitialize();

	ShowWindow(GetConsoleWindow(), SW_HIDE);
	FreeConsole();
	return YYTK_OK;
}

DllExport YYTKStatus API::Internal::MmGetModuleInformation(const char* szModuleName, CModule& outModule)
{
	using Fn = int(__stdcall*)(HANDLE, HMODULE, CModule*, DWORD);

	static HMODULE Module = GetModuleHandleA("kernel32.dll");
	static Fn K32GetModuleInformation = reinterpret_cast<Fn>(GetProcAddress(Module, "K32GetModuleInformation"));

	HMODULE hModule = GetModuleHandleA(szModuleName);

	if (hModule == nullptr)
		return YYTK_NOT_FOUND;

	K32GetModuleInformation(GetCurrentProcess(), hModule, &outModule, sizeof(CModule));
	
	return YYTK_OK;
}

YYTKStatus API::Internal::MmFindByteArray(const byte* pbArray, unsigned int uArraySize, unsigned long ulSearchRegionBase, unsigned int ulSearchRegionSize, const char* szMask, bool bStringSearch, DWORD& dwOutBuffer)
{
	if (gAPIVars.Globals.g_bDebugMode)
		Utils::Error::Message(CLR_DEFAULT, "%s(%p, %s);", __FUNCTION__, pbArray, szMask);

	dwOutBuffer = 0x00;

	if (!ulSearchRegionBase && !ulSearchRegionSize)
	{
		CModule CurrentModuleInformation;

		if (YYTKStatus _Status = MmGetModuleInformation(nullptr, CurrentModuleInformation))
			return _Status;

		ulSearchRegionBase = CurrentModuleInformation.Base;
		ulSearchRegionSize = CurrentModuleInformation.Size;
	}

	size_t PatternSize = std::min(strlen(szMask), uArraySize);

	for (unsigned i = 0; i < ulSearchRegionSize - PatternSize; i++)
	{
		int found = 1;
		for (unsigned j = 0; j < PatternSize; j++)
		{
			found &= szMask[j] == '?' || pbArray[j] == *(const byte*)(ulSearchRegionBase + i + j);
		}

		if (found)
		{
			if (bStringSearch && (*reinterpret_cast<char*>(ulSearchRegionBase + i - 1) != '\x00'))
				continue;

			dwOutBuffer = (ulSearchRegionBase + i);
			return YYTK_OK;
		}
	}

	return YYTK_NOMATCH;
	
}

YYTKStatus API::Internal::MmFindByteArray(const char* pszArray, unsigned int uArraySize, unsigned long ulSearchRegionBase, unsigned int ulSearchRegionSize, const char* szMask, bool bStringSearch, DWORD& dwOutBuffer)
{
	if (gAPIVars.Globals.g_bDebugMode)
		Utils::Error::Message(CLR_DEFAULT, "%s(%p, %s);", __FUNCTION__, pszArray, szMask);

	return MmFindByteArray(reinterpret_cast<byte*>(const_cast<char*>(pszArray)), uArraySize, ulSearchRegionBase, ulSearchRegionSize, szMask, bStringSearch, dwOutBuffer);
}

YYTKStatus API::Internal::MmFindCodeExecute(DWORD& dwOutBuffer)
{
	if (gAPIVars.Globals.g_bDebugMode)
		Utils::Error::Message(CLR_DEFAULT, "%s();", __FUNCTION__);

	unsigned long dwPattern = 0;
	
	if (YYTKStatus _Status = MmFindByteArray(
		"\x8A\xD8\x83\xC4\x14\x80\xFB\x01\x74",
		UINT_MAX,
		0,
		0,
		"xxxxxxxxx",
		false,
		dwPattern
	))
		return _Status;

	if (dwPattern == 0)
		return YYTK_INVALIDRESULT;

	while (*reinterpret_cast<WORD*>(dwPattern - 2) != 0xCCCC)
		dwPattern -= 1;

	dwOutBuffer = dwPattern;

	return YYTK_OK;
}

YYTKStatus API::Internal::MmFindCodeFunction(DWORD& dwOutBuffer)
{
	if (gAPIVars.Globals.g_bDebugMode)
		Utils::Error::Message(CLR_DEFAULT, "%s();", __FUNCTION__);

	return MmFindByteArray("\x8B\x44\x24\x04\x3B\x05\x00\x00\x00\x00\x7F", UINT_MAX, 0, 0, "xxxxxx????x", false, dwOutBuffer);
}

YYTKStatus API::Internal::VfGetFunctionPointer(const char* szFunctionName, EFPType ePointerType, DWORD& dwOutBuffer)
{
	if (gAPIVars.Globals.g_bDebugMode)
		Utils::Error::Message(CLR_DEFAULT, "%s(%s, %d);", __FUNCTION__, szFunctionName, ePointerType);

	if (!szFunctionName)
		return YYTK_INVALIDARG;

	bool bShouldFindAssemblyReference = (ePointerType == FPType_AssemblyReference);

	if (bShouldFindAssemblyReference)
	{
		DWORD dwStringReference = 0;

		std::string Mask(strlen(szFunctionName) + 1, 'x');

		if (YYTKStatus _Status = MmFindByteArray(
			reinterpret_cast<const byte*>(szFunctionName), 
			UINT_MAX, 
			0,
			0,
			Mask.c_str(),
			true,
			dwStringReference
		))
			return _Status;

		if (dwStringReference == 0)
			return YYTK_INVALIDRESULT;

		byte* pbNewPattern = new byte[5];
		pbNewPattern[0] = 0x68;

		memcpy(pbNewPattern + 1, &dwStringReference, sizeof(DWORD));

		if (YYTKStatus _Status = MmFindByteArray(
			const_cast<const byte*>(pbNewPattern),
			5,
			0,
			0,
			"xxxxx",
			false,
			dwOutBuffer
		))
		{
			delete[] pbNewPattern;
			return _Status;
		}
			

		if (dwOutBuffer == 0)
		{
			delete[] pbNewPattern;
			return YYTK_INVALIDRESULT;
		}

		dwOutBuffer = *reinterpret_cast<DWORD*>(dwOutBuffer - sizeof(DWORD));

		delete[] pbNewPattern;
		return YYTK_OK;
	}
	
	else
	{
		TRoutine pRoutine = nullptr;
		auto bSuccess = API::GetFunctionByName(szFunctionName, pRoutine);

		if (!bSuccess)
			return YYTK_FAIL;

		if (!pRoutine)
			return YYTK_INVALIDRESULT;

		dwOutBuffer = reinterpret_cast<DWORD>(pRoutine);
	}

	return YYTK_OK;
}

YYTKStatus API::Internal::VfGetFunctionEntryFromGameArray(int nIndex, TRoutine* pOutRoutine, int* pOutArgumentCount, char** pOutNameBuffer)
{
	if (!pOutRoutine)
		return YYTK_INVALIDARG;

	if (nIndex < 0)
		return YYTK_INVALIDARG;

	bool bShouldPopulateRoutine = (pOutRoutine != nullptr);
	bool bShouldPopulateArgc = (pOutArgumentCount != nullptr);
	bool bShouldPopulateName = (pOutNameBuffer != nullptr);
	
	if (!gAPIVars.Functions.Code_Function_GET_the_function)
		return YYTK_UNAVAILABLE;

	void*	pTempRoutine		= nullptr;
	char*	pTempNameBuffer		= nullptr;
	int		nTempArgumentCount	= 0;
	void*	pTempUnknown		= nullptr;

	gAPIVars.Functions.Code_Function_GET_the_function(nIndex, &pTempNameBuffer, &pTempRoutine, &nTempArgumentCount, &pTempUnknown);

	if (!pTempNameBuffer) // If the function doesn't have a name assigned at all
		return YYTK_INVALIDRESULT;

	if (*pTempNameBuffer == '\0') // Builtins shouldn't have no names
		return YYTK_INVALIDRESULT; 
	
	if (bShouldPopulateRoutine)
		*pOutRoutine = reinterpret_cast<decltype(*pOutRoutine)>(pTempRoutine);

	if (bShouldPopulateArgc)
		*pOutArgumentCount = nTempArgumentCount;

	if (bShouldPopulateName)
		*pOutNameBuffer = pTempNameBuffer;

	return YYTK_OK;
}

YYTKStatus API::Internal::VfLookupFunction(const char* szFunctionName, TRoutine& outRoutine, int* pOptOutIndex)
{
	if (gAPIVars.Globals.g_bDebugMode)
		Utils::Error::Message(CLR_DEFAULT, "%s(%s, %p);", __FUNCTION__, szFunctionName, pOptOutIndex);

	if (!szFunctionName)
		return YYTK_INVALIDARG;

	if (*szFunctionName == '\0')
		return YYTK_INVALIDARG;

	TRoutine		pTempRoutine	= nullptr;
	const char*		pTempName		= nullptr;

	int nIndex = 0;

	while (VfGetFunctionEntryFromGameArray(nIndex, &pTempRoutine, nullptr, const_cast<char**>(&pTempName)) == YYTK_OK)
	{
		if (_strnicmp(pTempName, szFunctionName, 64) == 0)
		{
			outRoutine = pTempRoutine;

			if (pOptOutIndex)
				*pOptOutIndex = nIndex;

			return YYTK_OK;
		}

		nIndex++;
	}

	return YYTK_NOT_FOUND;
}

YYTKStatus API::Internal::MmGetScriptArrayPtr(CDynamicArray<CScript*>*& outArray, const int& nMaxInstructions)
{
	if (gAPIVars.Globals.g_bDebugMode)
		Utils::Error::Message(CLR_DEFAULT, "%s(%d);", __FUNCTION__, nMaxInstructions);

	DWORD dwScriptExists = 0;

	if (YYTKStatus Status = VfGetFunctionPointer("script_exists", EFPType::FPType_DirectPointer, dwScriptExists))
		return Status;

	if (dwScriptExists == 0)
		return YYTK_INVALIDRESULT;

	// call script_exists
	// xor ecx, ecx
	// add esp, 0Ch
	unsigned long pPattern = FindPattern("\xE8\x00\x00\x00\x00\x00\xC9\x83\xC4\x0C", "x?????xxxx", dwScriptExists, 0xFF);

	if (!pPattern)
		return YYTK_NOMATCH;

	// Get 4 bytes from the JMP opcode (the relative offset)
	DWORD pJmpOffset = *reinterpret_cast<DWORD*>(pPattern + 1);

	// Calculate the jumped-to address, it's relative from the end of the jmp instruction, the size of which is 5 bytes.
	DWORD pFunction = (pPattern + 5) + pJmpOffset;

	hde32s hsInstruction;
	memset(&hsInstruction, 0, sizeof(hde32s));

	for (int nInstr = 0; nInstr < nMaxInstructions; nInstr++)
	{
		int nInstructionSize = hde32_disasm(reinterpret_cast<const void*>(pFunction), &hsInstruction);

		// If the opcodes match what we're searching for
		if ((hsInstruction.opcode == 0xA1 || (hsInstruction.opcode == 0x8B && hsInstruction.modrm)) && nInstructionSize > 4)
		{
			if (hsInstruction.imm.imm32)
				outArray = reinterpret_cast<CDynamicArray<CScript*>*>(hsInstruction.imm.imm32 - sizeof(long*));
			else
				outArray = reinterpret_cast<CDynamicArray<CScript*>*>(hsInstruction.disp.disp32 - sizeof(long*));

			return YYTK_OK;
		}
	
		// Skip to the next instruction
		pFunction += nInstructionSize;
	}

	return YYTK_NOT_FOUND;
}