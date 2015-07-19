
%module opencore
%{
#include "opencore.hpp"
%}

%ignore REGISTRATION_INFO;
%ignore PubMessageFmt;
%ignore RemoteMessageFmt;
%ignore ReplyFmt;
%ignore ArenaMessageFmt;
%ignore ChatMessageFmt;
%ignore xmalloc;
%ignore xzmalloc;
%ignore xcalloc;

/* for user_data conversions */
/* python -> c */
%typemap (in) PyObject* {
       /* $1 = PyCObject_AsVoidPtr($input); */
        $1 = (void*)$input;
}

/* c -> python */
%typemap(out) void* {
        $result = (PyObject*)$1;
}

%callback("GameEvent");
%include "opencore.hpp"

