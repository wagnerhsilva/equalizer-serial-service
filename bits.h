#ifndef _BITS_H_
#define _BITS_H_
#include <defs.h>

/*
 * This started as a bitmap API so we don't have crazy
 * bit manipulations on random files. But as I was writting this
 * i realized a bitmap was not enough and so we traded by a buffer.
 * However the API kept the same so that callers don't need to 
 * care how this is implemented.
*/

typedef struct{
	unsigned int data[MAX_STRING_LEN]; //array of values representing 'bits'
	int maxlen; //maximum lenth of the 'data' size = MAX_STRING_LEN
	int maxfill; //current maximum position filled
}Bits;

bool bits_create_new(Bits *bits); //creates a new Bits manager, it actually just clears it
bool bits_clear(Bits *bits); //same as create, symbolic
bool bits_set_bit(Bits *bits, int position, bool value); //set a bit in mask to 'value'
bool bits_is_bit_set(Bits *bits, int position); //check if a bit is set
void bits_print(Bits *bits); //prints its binary form up untill 'maxfill'

#endif