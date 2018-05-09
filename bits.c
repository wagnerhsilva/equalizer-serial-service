#include <bits.h>
#include <stdio.h>
#include <string.h>
#define nullptr NULL

/*
 * checks if a pointer is initialized or not, immediatly returns false if not
*/
#define PTR_VALID(x) { if(!(x)){ LOG("Bits:Not a valid pointer[%s]::%d\n", (char *)#x, __LINE__); return false; }}

/*
 * Reset the Bits structure, sets all data values to 0.
 * The maxlen to MAX_STIRNG_LEN.
 * The maxfill to zero.
*/
static void clear_mask(Bits *bits){
	memset(bits->data, 0, sizeof(bits->data));
	bits->maxlen = MAX_STRING_LEN;
	bits->maxfill = 0;
}

/*
 * Set the fields for a new Bits structure
 * just a simple clear
*/
bool bits_create_new(Bits *bits){
	PTR_VALID(bits);
	clear_mask(bits);
	return true;
}	

/*
 * A clear function, symbolic so code is readable
 * same as create.
*/
bool bits_clear(Bits *bits){
	PTR_VALID(bits);
	clear_mask(bits);
	return true;
}

/*
 * Sets the position 'position' of the array 'data' to the value 'value'
*/
bool bits_set_bit(Bits *bits, int position, bool value){
	PTR_VALID(bits); //check if the pointer is valid
	bool ret = false; 
	if(position < bits->maxlen && position > -1){ //check if in range
		bits->data[position] = (value ? 1 : 0); //set the value
		if(bits->maxfill < position) bits->maxfill = position; //update maxfill case we need
		ret = true;
	}
	return ret;
}

/*
 * Check if the field at 'position' is set (1)
 * return true in case afirmative or false otherwise
*/
bool bits_is_bit_set(Bits *bits, int position){
	PTR_VALID(bits); //check pointer
	bool ret = false;
	if(position < bits->maxlen && position > -1){ //check range
		if(bits->data[position] != 0){ //check value
			ret = true;
		}
	}
	return ret;
}

/*
 * Debug function, so that we can get a visual printf
 * of the Bits structure
*/
void bits_print(Bits *bits){
	if(bits){
		LOG("Binary:Printf = ");
		for(int i = 0; i <= bits->maxfill; i+= 1){
			LOG("%d", (int)(bits->data[i]));
		}
		LOG("\n");
	}
}