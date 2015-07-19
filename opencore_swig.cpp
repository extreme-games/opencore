
#include "opencore_swig.hpp"
#include "opencore_wrap.c" // include the c file directly, because the below code depends on swig internals

PyObject *wrap_old_core_data(CORE_DATA *cd) {
  return SWIG_NewPointerObj(SWIG_as_voidptr(cd), SWIGTYPE_p_core_data,  0);
}

