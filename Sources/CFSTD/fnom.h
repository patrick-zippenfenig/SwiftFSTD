#ifndef FNOM_H
#define FNOM_H

#include <stdint.h>

#include "rpnmacros.h"

#define MAXFILES 1024

//! \todo Rename this type to something more specific.  Is it used by client apps?
typedef struct {
    unsigned int
        stream:1,
        std:1,
        burp:1,
        rnd:1,
        wa:1,
        ftn:1,
        unf:1,
        read_only:1,
        old:1,
        scratch:1,
        notpaged:1,
        pipe:1,
        write_mode:1,
        remote:1,
        padding:18;
} attributs;


//! \todo Rename this type to something more specific.  Is it used by client apps?
typedef struct {
    //! Complete file name
    char *file_name;
    //! Sub file name for cmcarc files
    char *subname;
    //! File type and options
    char * file_type;
    //! fnom unit number
    int32_t iun;
    //! C file descriptor
    int32_t fd;
    //! File size in words
    int32_t file_size;
    //! effective file size in words
    int32_t eff_file_size;
    //! Record length when appliable
    int32_t lrec;
    //! Open/close flag
    int32_t open_flag;
    attributs attr;
} general_file_info;

#if defined(FNOM_OWNER)
//! Fnom General File Desc Table
general_file_info FGFDT[MAXFILES];
#else
extern general_file_info FGFDT[MAXFILES];
#endif


#ifdef __cplusplus
extern "C" {
#endif

int c_fretour(
    const int iun
);
int32_t fretour_(
    const int32_t * const fiun
);

void d_fgfdt_();

int c_fnom(
    int * const iun,
    const char * const nom,
    const char * const type,
    const int lrec
);
int32_t fnom_(
    int32_t * const iun,
    const char * const nom,
    const char * const type,
    const int32_t * const flrec,
    F2Cl l1,
    F2Cl l2
);

int c_fclos(const int iun);
int32_t fclos_(const int32_t * const fiun);

int32_t qqqfnom_(
    const int32_t * const iun,
    char * const nom,
    char * const type,
    int32_t * const flrec,
    F2Cl l1,
    F2Cl l2
);

int c_waopen2(const int iun);
int32_t waopen2_(const int32_t * const iun);

void c_waopen(const int iun);
void waopen_(const int32_t * const iun);

int c_waclos2(const int iun);
int32_t waclos2_(const int32_t * const iun);

void c_waclos(const int iun);
void waclos_(const int32_t * const iun);

int c_wawrit2(
    const int iun,
    const void * const buf,
    const unsigned int offset,
    const int nwords
);
int32_t wawrit2_(
    const int32_t * const iun,
    const void * const buf,
    const uint32_t * const offset,
    const int32_t * const nwords
);

void c_wawrit(
    const int iun,
    const void * const buf,
    const unsigned int offset,
    const int nwords
);
void wawrit_(
    const int32_t * const iun,
    const void * const buf,
    const uint32_t * const offset,
    const int32_t * const nwords
);

int c_waread2(
    const int iun,
    void *buf,
    const unsigned int offset,
    const int nwords
);
int32_t waread2_(
    const int32_t * const iun,
    void * const buf,
    const uint32_t * const offset,
    const int32_t * const nwords
);

void c_waread(
    const int iun,
    void * const buf,
    const unsigned int offset,
    const int nwords
);
void waread_(
    const int32_t * const iun,
    void * const buf,
    const uint32_t * const offset,
    const int32_t * const nwords
);

int32_t c_wasize(const int iun);
int32_t wasize_(const int32_t * const iun);

int32_t c_numblks(const int iun);
int32_t numblks_(const int32_t * const iun);

int32_t existe_(const char * const nom, F2Cl llng);

void c_openda(const int iun);
void openda_(const int32_t * const iun);

void c_closda(const int iun);
void closda_(const int32_t * const iun);

void c_checda(const int iun);
void checda_(const int32_t * const iun);

void c_readda(
    const int iun,
    int * const buf,
    const int nwords,
    const int offset
);
void readda_(
    const int32_t * const iun,
    int32_t * const buf,
    const int32_t * const nwords,
    const int32_t * const offset
);

void c_writda(
    const int iun,
    const int * const buf,
    const int nwords,
    const int offset
);
void writda_(
    const int32_t * const iun,
    const int32_t * const buf,
    const int32_t * const nwords,
    const int32_t * const offset
);

int c_getfdsc(const int iun);
int32_t getfdsc_(const int32_t * const iun);

void c_sqopen(const int iun);
void sqopen_(const int32_t * const iun);

void c_sqclos(const int iun);
void sqclos_(const int32_t * const iun);

void c_sqrew(const int iun);
void sqrew_(const int32_t * const iun);

void c_sqeoi(const int iun);
void sqeoi_(const int32_t * const iun);

int c_sqgetw(
    const int iun,
    uint32_t * const buf,
    const int nwords
);
int32_t sqgetw_(
    const int32_t * const iun,
    uint32_t * const buf,
    const int32_t * const nwords
);

int c_sqputw(
    const int iun,
    const uint32_t * const buf,
    const int nwords
);
int32_t sqputw_(
    const int32_t * const iun,
    const uint32_t * const buf,
    const int32_t * const nwords
);

int c_sqgets(
    const int iun,
    char * const buf,
    const int nchar
);
int32_t sqgets_(
    const int32_t * const iun,
    char * const buf,
    const int32_t * const nchar,
    F2Cl llbuf
);

int c_sqputs(
    const int iun,
    const char * const buf,
    const int nchar
);
int32_t sqputs_(
    const int32_t *iun,
    const char * const buf,
    const int32_t * const nchar,
    F2Cl llbuf
);

void d_wafdt_();

uint32_t hrjust_ (uint32_t *str, const int32_t * const ncar);

uint32_t hljust_ (uint32_t *str, const int32_t * const ncar);

#ifdef __cplusplus
}
#endif

#endif
