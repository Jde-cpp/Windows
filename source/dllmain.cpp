#ifndef DECLARE_HANDLE
	#define DECLARE_HANDLE(name) struct name##__{int unused;}; typedef struct name##__ *name
	DECLARE_HANDLE(HINSTANCE);
typedef HINSTANCE HMODULE;
#endif

int __stdcall DllMain( HMODULE /*hModule*/, unsigned long  ul_reason_for_call, void* /*lpReserved*/ )
{
	switch (ul_reason_for_call)
	{
	case 1:
	case 2:
	case 3:
	case 0:
		break;
	}
	return 1;
}

