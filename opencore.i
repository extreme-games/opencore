
/* for user_data conversions */
/* python -> c */
%typemap (in) void* {
       /* $1 = PyCObject_AsVoidPtr($input); */
        $1 = (void*)$input;
}

/* c -> python */
%typemap(out) void* {
        $result = (PyObject*)$1;
}

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
%ignore strlwr;

%callback("GameEvent");
%include "opencore.hpp"

