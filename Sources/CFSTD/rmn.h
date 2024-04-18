#ifndef _RMN_H
#define _RMN_H

#include "fnom.h"
#include "fstd98.h"
#include "fstdsz.h"
//#include "ezscint.h"
#include "fst_missing.h"
#include "c_wkoffit.h"
//#include "burp.h"
//#include "ezscint.h"
//#include "c_ccard.h"
//#include "ftnStrLen.h"
#include "convert_ip.h"

#ifdef __cplusplus
extern "C" {
#endif

// To put in respective inlude file
void cxgaig_(char *grtyp,int32_t *ig1,int32_t *ig2,int32_t *ig3,int32_t *ig4,float *xg1,float *xg2,float *xg3,float *xg4);
void cigaxg_(char *grtyp,float *xg1,float *xg2,float *xg3,float *xg4,int32_t *ig1,int32_t *ig2,int32_t *ig3,int32_t *ig4);
void convip_plus_(int32_t *ipnew,float *level,int32_t *fkind,int32_t *fmode,char *strg,int32_t *flag,F2Cl strglen);
int newdate_(int32_t *fdat1,int32_t *fdat2,int32_t *fdat3,int32_t *fmode);
int difdatr_(int32_t *fdat1,int32_t *fdat2,double *fnhours);
int incdatr_(int32_t *fdat1,int32_t *fdat2,double *fnhours);

#ifdef __cplusplus
}
#endif

#endif
