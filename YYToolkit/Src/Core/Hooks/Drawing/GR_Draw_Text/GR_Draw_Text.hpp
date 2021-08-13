#pragma once

namespace Hooks
{
	namespace GR_Draw_Text
	{
		void Function(float x, float y, const char* str, int linesep, int linewidth);
		void* GetTargetAddress();

		inline decltype(&Function) pfnOriginal;
	}
}