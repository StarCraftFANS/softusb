typedef struct KEYDATA{
unsigned char key;
unsigned char time;
}KEYDATA_T;



inline void copy_word(void* pdst,void *psrc)
{
	*(uint16_t*)pdst = *(uint16_t*)psrc;
}

/*
union PACK_T{
uint16_t m_word;
KEYDATA keydata;
}PACK;
*/

/* pin assignments:
PC0	Key 1
PC1	Key 2
PC2	Key 3
PC3	Key 4
PC4	Key 5
PC5 Key 6

PB0	Key 7
PB1	Key 8
PB2	Key 9
PB3	Key 10
PB4	Key 11
PB5	Key 12

PD0	USB-
PD1	debug tx
PD2	USB+ (int0)
PD3	Key 13
PD4	Key 14
PD5	Key 15
PD6	Key 16
PD7	Key 17
*/
#define K_BACK    1
#define K_FORWORD 2

#define K_KEY     3
#define K_RECORD  4
#define K_TIME    5

#define K_X       7
#define K_CLEAR   8
#define K_AUTO    9 


