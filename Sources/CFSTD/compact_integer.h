//
//  Header.h
//  
//
//  Created by Patrick Zippenfenig on 18.04.2024.
//

#ifndef compact_integer_h
#define compact_integer_h


int  compact_integer( void *unpackedArrayOfInt, void *packedHeader, void *packedArrayOfInt,
                       int elementCount, int bitSizeOfPackedToken, int off_set,
                       int stride, int opCode);
int  compact_short( void *unpackedArrayOfShort, void *packedHeader, void *packedArrayOfInt,
                       int elementCount, int bitSizeOfPackedToken, int off_set,
                       int stride, int opCode);

int  compact_char( void *unpackedArrayOfBytes, void *packedHeader, void *packedArrayOfInt,
                       int elementCount, int bitSizeOfPackedToken, int off_set,
                  int stride, int opCode);

#endif /* compact_integer_h */
