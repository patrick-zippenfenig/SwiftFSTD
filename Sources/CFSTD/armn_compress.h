#ifndef ARMN_COMPRESS_H
#define ARMN_COMPRESS_H

typedef void *(*PackFunctionPointer)(
    void *unpackedArrayOfFloat,
    void *packedHeader,
    void *packedArrayOfInt,
    int elementCount,
    int bitSizeOfPackedToken,
    int off_set,
    int stride,
    int opCode,
    int hasMissing,
    void *missingTag
);

int armn_compress(unsigned char *fld, int ni, int nj, int nk, int nbits, int op_code);

int compact_integer(
    void *unpackedArrayOfInt,
    void *packedHeader,
    void *packedArrayOfInt,
    int elementCount,
    int bitSizeOfPackedToken,
    int off_set,
    int stride,
    int opCode
);

void *compact_float(
    void *unpackedArrayOfFloat,
    void *packedHeader,
    void *packedArrayOfInt,
    const int elementCount,
    const int packedTokenBitSize,
    const int offset,
    const int stride,
    const int opCode,
    const int hasMissing,
    const void * const missingTag
);

void *compact_double(
    void *unpackedArrayOfFloat,
    void *packedHeader,
    void *packedArrayOfInt,
    const int elementCount,
    const int packedTokenBitSize,
    const int offset,
    const int stride,
    const int opCode,
    const int hasMissing,
    const void * const missingTag
);

#define powerSpan 65
static double powerOf2s[powerSpan];
static int powerOf2sInitialized = 0;
#define MAX_RANGE 1.0e+38

#endif
