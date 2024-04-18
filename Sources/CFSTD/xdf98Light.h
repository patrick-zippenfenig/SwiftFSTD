//
//  xdf98Light.h
//  
//
//  Created by Patrick Zippenfenig on 18.04.2024.
//

#ifndef xdf98Light_h
#define xdf98Light_h
#include "qstdir.h"

#include <stdio.h>

int c_xdfloc(int iun, int handle, uint32_t *primk, int nprim);
int c_xdfloc2(int iun, int handle, uint32_t *primk, int nprim, uint32_t *mskkeys);
int fnom_index(int iun);
int file_index(int iun);

int c_xdfcls(int iun);
int c_xdfput(int iun, int handle, buffer_interface_ptr buf);

int c_xdfcheck(const char* filename);

int c_xdfopn(int iun, char *mode, word_2 *pri, int npri,
             word_2 *aux, int naux, char *appl);
int c_xdfget2(const int handle, buffer_interface_ptr buf, int * const aux_ptr);

int c_xdfprm(int handle, int *addr, int *lng, int *idtyp,
         uint32_t *primk, int nprim);

int c_xdflnk(int *liste, int n);
int c_xdfunl(int *liste, int n);
int c_xdfdel(const int handle);

#endif /* xdf98Light_h */
