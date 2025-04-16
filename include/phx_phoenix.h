#ifndef _PHOENIX
  #define _PHOENIX

  #include <phx_api.h>
#ifdef __cplusplus
extern "C" {
#endif
  int PHX_str_to_etParam(char*, etParam*);
  int PHX_str_to_etParamValue(char*, etParamValue*);
  int PHX_str_to_etParamValues(char*, etParamValue*);
#ifdef __cplusplus
}
#endif
#endif   /* _PHOENIX */
