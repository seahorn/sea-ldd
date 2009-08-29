/** 
 * TVPI Theory. Constraints of the form   x<=k and -x<= k
 */

#ifndef __TVPI__H_
#define __TVPI__H_
#include "tdd.h"

#ifdef __cplusplus
extern "C" {
#endif

  theory_t *tvpi_create_theory (size_t vn);
  void tvpi_destroy_theory (theory_t*);

#ifdef __cplusplus
}
#endif
#endif