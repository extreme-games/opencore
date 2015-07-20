
#ifndef _OPENCORE_SWIG_HPP
#define _OPENCORE_SWIG_HPP 1

#include <Python.h>
#include "core.hpp"

#ifdef __cplusplus
extern "C" { 
#endif

void init_opencore();
/* Take the core data object and turn it into a python object */
PyObject *wrap_old_core_data(CORE_DATA *cd);

#ifdef __cplusplus
}
#endif

#endif

