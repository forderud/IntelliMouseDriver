#pragma once

#ifdef _KERNEL_MODE
	#define PAGED_CODE_SEG __declspec(code_seg("PAGE"))
	#define INIT_CODE_SEG  __declspec(code_seg("INIT"))

	#define PAGED_CODE_SEG_BEGIN \
		__pragma(code_seg(push)) \
		__pragma(code_seg("PAGE"))

	#define PAGED_CODE_SEG_END \
		__pragma(code_seg(pop))

	#define INIT_CODE_SEG_BEGIN \
			__pragma(code_seg(push)) \
			__pragma(code_seg("INIT"))

	#define INIT_CODE_SEG_END \
			__pragma(code_seg(pop))
#else
	#define PAGED_CODE_SEG
	#define INIT_CODE_SEG
	#define PAGED_CODE_SEG_BEGIN
	#define PAGED_CODE_SEG_END
	#define INIT_CODE_SEG_BEGIN
	#define INIT_CODE_SEG_END
#endif