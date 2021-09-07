#include "Code_Execute.hpp"
#include "../../Features/API/API.hpp"
#include "../../Utils/Error.hpp"
#include "../../SDK/Plugins/YYTKEvent/YYTKEvent.hpp"

namespace Hooks::Code_Execute
{
	bool Function(CInstance* pSelf, CInstance* pOther, CCode* Code, RValue* Res, int Flags)
	{
		using YYTKCodeEvent = YYTKEvent<bool, bool(*)(CInstance*, CInstance*, CCode*, RValue*, int), EventType::EVT_CODE_EXECUTE, CInstance*, CInstance*, CCode*, RValue*, int>;

		YYTKCodeEvent Event = YYTKCodeEvent(&Function, pSelf, pOther, Code, Res, Flags);
		Plugins::RunCallback(&Event);

		if (Event.CalledOriginal())
			return Event.GetReturn();

		return pfnOriginal(pSelf, pOther, Code, Res, Flags);
	}

	void* GetTargetAddress()
	{
		return reinterpret_cast<void*>(gAPIVars.Code_Execute);
	}
}