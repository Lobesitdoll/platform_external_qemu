// Generated Code - DO NOT EDIT !!
// generated by 'emugen'
#ifndef __foo_client_proc_t_h
#define __foo_client_proc_t_h



#include "foo_types.h"
#ifndef foo_APIENTRY
#define foo_APIENTRY 
#endif
typedef void (foo_APIENTRY *fooAlphaFunc_client_proc_t) (void * ctx, FooInt, FooFloat);
typedef FooBoolean (foo_APIENTRY *fooIsBuffer_client_proc_t) (void * ctx, void*);
typedef void (foo_APIENTRY *fooUnsupported_client_proc_t) (void * ctx, void*);
typedef void (foo_APIENTRY *fooDoEncoderFlush_client_proc_t) (void * ctx, FooInt);
typedef void (foo_APIENTRY *fooTakeConstVoidPtrConstPtr_client_proc_t) (void * ctx, const void* const*);
typedef void (foo_APIENTRY *fooSetComplexStruct_client_proc_t) (void * ctx, const FooStruct*);
typedef void (foo_APIENTRY *fooGetComplexStruct_client_proc_t) (void * ctx, FooStruct*);
typedef void (foo_APIENTRY *fooInout_client_proc_t) (void * ctx, uint32_t*);


#endif
