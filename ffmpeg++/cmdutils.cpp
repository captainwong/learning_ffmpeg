#ifdef _WIN32
#include <windows.h>
#endif


/**
 * Initialize dynamic library loading
 */
void init_dynload()
{
#ifdef _WIN32
	SetDllDirectory("");
#endif
}