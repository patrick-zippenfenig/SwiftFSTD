//
//  Header.h
//  
//
//  Created by Patrick Zippenfenig on 18.04.2024.
//

#ifndef Float_packer_h
#define Float_packer_h


void c_float_packer_params(
    //! Pointer to size of header part
    int32_t *header_size,
    //! Pointer to size of stream part
    int32_t *stream_size,
    //! Reserved for future expansion, a value of zero is returned now
    int32_t *p1,
    //! Reserved for future expansion, a value of zero is returned now
    int32_t *p2,
    //! Pointer to number of values
    int32_t npts
                           );

int32_t c_float_packer(
    //! Pointer to input array of floating point numbers
    float *source,
    //! Pointer to number of useful bits in token
    int32_t nbits,
    //! Pointer to 64 bit header for this block
    int32_t *header,
    //! Pointer to packed stream (16 bits per token, 32 bit aligned at start)
    int32_t *stream,
    //! Number of values to unpack
    int32_t npts
                       );

int32_t c_float_unpacker(
    float *dest,
    int32_t *header,
    int32_t *stream,
    int32_t npts,
    int32_t *nbits
                         );

#endif /* Float_packer_h */
