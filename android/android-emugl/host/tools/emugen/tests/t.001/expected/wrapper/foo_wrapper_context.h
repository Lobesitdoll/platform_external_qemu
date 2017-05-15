// Generated Code - DO NOT EDIT !!
// generated by 'emugen'
#ifndef __foo_wrapper_context_t_h
#define __foo_wrapper_context_t_h

#include "foo_server_proc.h"

#include "foo_types.h"


struct foo_wrapper_context_t {

	fooAlphaFunc_wrapper_proc_t fooAlphaFunc;
	fooIsBuffer_wrapper_proc_t fooIsBuffer;
	fooUnsupported_wrapper_proc_t fooUnsupported;
	fooDoEncoderFlush_wrapper_proc_t fooDoEncoderFlush;
	fooTakeConstVoidPtrConstPtr_wrapper_proc_t fooTakeConstVoidPtrConstPtr;
	fooSetComplexStruct_wrapper_proc_t fooSetComplexStruct;
	fooGetComplexStruct_wrapper_proc_t fooGetComplexStruct;
	fooInout_wrapper_proc_t fooInout;
	virtual ~foo_wrapper_context_t() {}

	typedef foo_wrapper_context_t *CONTEXT_ACCESSOR_TYPE(void);
	static void setContextAccessor(CONTEXT_ACCESSOR_TYPE *f);
	int initDispatchByName( void *(*getProc)(const char *name, void *userData), void *userData);
};

#endif
