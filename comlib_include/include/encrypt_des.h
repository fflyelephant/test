#ifndef __ENCRYPT_DES_H
#define __ENCRYPT_DES_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#define BYTE  unsigned char
#define WORD  unsigned short
#define ENCRYPT   0
#define DECRYPT   1


extern int ByteToAsc(int inbuflen,char* inbuf,char* outbuf);
extern int AscToByte(char* inbuf,int* outbuflen,char* outbuf);

extern void midd_rxDES( BYTE Decrypt, BYTE *Key, BYTE *Data, BYTE *Result );
extern void midd_Triple_DES( int Decrypt, BYTE* Key, BYTE* Data, BYTE* X_Data );

extern void midd_DES_setkey( BYTE   Decrypt , BYTE * Key );
extern void midd_DES_calcul( BYTE * Key_In  , BYTE * KeyOut );

int ri_DESenc(int keylen,char *key,int inbuflen,char *inbuf,int *outbuflen,char *outbuf);
int ri_DESdec(int keylen,char *key,int inbuflen,char *inbuf,int *outbuflen,char *outbuf);
int ri_DESenc_asc(char *key,char *inbuf,char *outbuf);
int ri_DESdec_asc(char *key,char *inbuf,char *outbuf);

void hextostring(unsigned char *dd,char *hh,int len);
void stringtohex(char *dd,unsigned char *hh,int len);
#endif