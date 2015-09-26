
%include "stdint.i"

/* for user_data conversions */
/* python -> c */
%typemap (in) void* {
        if ($1 && $1 != Py_None) Py_DECREF($1);
        Py_INCREF($input);
        $1 = (void*)$input;
}

/* c -> python */
%typemap(out) void* {
        Py_INCREF($1);
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
%ignore strlcat;
%ignore strlcpy;

%callback("GameEvent");
%include "opencore.hpp"

