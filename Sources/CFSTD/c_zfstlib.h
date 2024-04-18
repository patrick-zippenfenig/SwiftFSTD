//
//  Header.h
//  
//
//  Created by Patrick Zippenfenig on 18.04.2024.
//

#ifndef c_zfstlib_h
#define c_zfstlib_h

int c_fstzip_parallelogram(unsigned int *zfld, int *zlng, unsigned short *fld, int ni, int nj, int step, int nbits, uint32_t *header);
void calcule_entropie(float *entropie, unsigned short *bitstream, int npts, int nbits);
void packTokensSample(unsigned int z[], int *zlng, unsigned int zc[], int nicoarse, int njcoarse, int diffs[], int ni, int nj, int nbits, int step, uint32_t *header, int start, int end);
static void unpackTokensSample(unsigned int zc[], int diffs[], unsigned int z[], int nicoarse, int njcoarse,  int ni, int nj, int nbits, int step, uint32_t *header, int start);
void c_armn_compress_setlevel(int level);
int c_armn_compress_getlevel();
void c_armn_compress_setswap(int swapState);
int  c_armn_compress_getswap();
void c_fstzip(unsigned int *zfld, int *zlng, unsigned int *fld, int ni, int nj, int code_methode, int degre, int step, int nbits, int bzip);
void c_fstunzip(unsigned int *fld, unsigned int *zfld, int ni, int nj, int nbits);
void c_fstzip_minimum(unsigned int *zfld, int *zlng, unsigned short *fld, int ni, int nj, int step, int nbits, uint32_t *header);
void c_fstunzip_minimum(unsigned short *fld, unsigned int *zfld, int ni, int nj, int step, int nbits, uint32_t *header);
void c_fstunzip_parallelogram(unsigned short *fld, unsigned int *zfld, int ni, int nj, int step, int nbits, uint32_t *header);
void c_fstunzip_sample(unsigned short *fld, unsigned int *zfld, int ni, int nj, int step, int nbits, uint32_t *header);

void armn_compress_setlevel_(int32_t *level);

void fill_coarse_nodes_(int32_t *z, int32_t *ni, int32_t *nj, int32_t *zc, int32_t *nicoarse, int32_t *njcoarse, int32_t *istep);
void ibicubic_int4_(int32_t *izo, int32_t *ni, int32_t *nj, int32_t *step, int32_t *ajus_x,int32_t *ajus_y);

static void calcul_ninjcoarse(int *nicoarse, int *njcoarse, int ni, int nj, int ajus_x, int ajus_y, int istep);
static void packTokensMinimum(unsigned int z[], int *zlng, unsigned short ufld[], int ni, int nj, int nbits, int istep, uint32_t *header);
static void unpackTokensMinimum(unsigned short ufld[], unsigned int z[], int ni, int nj, int nbits, int istep, uint32_t *header);
static void calcul_ajusxy(int *ajus_x, int *ajus_y, int ni, int nj, int istep);
static void packTokensParallelogram(unsigned int z[], int *zlng, unsigned short ufld[], int ni, int nj, int nbits, int istep, uint32_t *header);
static void unpackTokensParallelogram(unsigned short ufld[], unsigned int z[], int ni, int nj, int nbits, int istep, uint32_t *header);

static int fstcompression_level = -1;
static int swapStream           =  1;
static unsigned char fastlog[256];
static int once = 0;

#endif /* c_zfstlib_h */
