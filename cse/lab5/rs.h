#ifndef _RS_H
#define _RS_H

#include <math.h>
#include <stdio.h>

#define mm 9 /* RS code over GF(2**8) - change to suit */
#define nn 511 /* nn=2**mm -1 length of codeword */
#define tt 32 /* number of errors that can be corrected */ //lq
#define kk 447 /* kk = nn-2*tt */  


void generate_gf();
void gen_poly();

void encode_rs(int recd[nn], int data[kk], int  bb[nn-kk]);
void decode_rs(int recd[nn], int dataout[kk]);
void encode_rs_8(unsigned char recd[nn], unsigned char data[kk], unsigned char  bb[nn-kk]);
void decode_rs_8(unsigned char recd[nn], unsigned char dataout[kk]);


#endif