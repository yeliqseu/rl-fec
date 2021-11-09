/* 
External documentation and recommendations on the use of this code is
available at http://www.cs.umass.edu/~rich/tiles.html.

This is an implementation of grid-style tile codings, based originally on 
the UNH CMAC code (see http://www.ece.unh.edu/robots/cmac.htm). 
Here we provide a procedure, "GetTiles", that maps floating-point and integer
variables to a list of tiles. This function is memoryless and requires no
setup. We assume that hashing colisions are to be ignored. There may be
duplicates in the list of tiles, but this is unlikely if memory-size is
large. 

The floating-point input variables will be gridded at unit intervals, so generalization
will be by 1 in each direction, and any scaling will have 
to be done externally before calling tiles.  There is no generalization
across integer values.

It is recommended by the UNH folks that num-tilings be a power of 2, e.g., 16. 

We assume the existence of a function "rand()" that produces successive
random integers, of which we use only the low-order bytes.
*/

#include <iostream>
#include "tiles.h"
#include "stdlib.h"
#include "math.h"

static int seed = 123;

static int local_rand()
{
	int a = 1103515245;
	int m = 2147483648;
	int c = 12345;
  	seed = (a * seed + c) % m;
  	return seed;
}

void tiles(
	int the_tiles[],               // provided array contains returned tiles (tile indices)
	int num_tilings,           // number of tile indices to be returned in tiles       
    int memory_size,           // total number of possible tiles
	double floats[],            // array of floating point variables
    int num_floats,            // number of floating point variables
    int ints[],				   // array of integer variables
    int num_ints)              // number of integer variables
{
	int i,j;
	int qstate[MAX_NUM_VARS];
	int base[MAX_NUM_VARS];
	int coordinates[MAX_NUM_VARS * 2 + 1];   /* one interval number per relevant dimension */
	int num_coordinates = num_floats + num_ints + 1;
	
	for (int i=0; i<num_ints; i++) coordinates[num_floats+1+i] = ints[i];
    
	/* quantize state to integers (henceforth, tile widths == num_tilings) */
    for (i = 0; i < num_floats; i++) {
    	qstate[i] = (int) floor(floats[i] * num_tilings);
    	base[i] = 0;
    }
    
    /*compute the tile numbers */
    for (j = 0; j < num_tilings; j++) {
    
		/* loop over each relevant dimension */
		for (i = 0; i < num_floats; i++) {
		
    		/* find coordinates of activated tile in tiling space */
			if (qstate[i] >= base[i])
				coordinates[i] = qstate[i] - ((qstate[i] - base[i]) % num_tilings);
			else
				coordinates[i] = qstate[i]+1 + ((base[i] - qstate[i] - 1) % num_tilings) - num_tilings;
				        
			/* compute displacement of next tiling in quantized space */
			base[i] += 1 + (2 * i);
		}
		/* add additional indices for tiling and hashing_set so they hash differently */
		coordinates[i] = j;
		
		the_tiles[j] = hash_UNH(coordinates, num_coordinates, memory_size, 449);
	}
	return;
}
			
/* hash_UNH
   Takes an array of integers and returns the corresponding tile after hashing 
*/


int hash_UNH(int *ints, int num_ints, long m, int increment)
{
	static unsigned int rndseq[2048];
	static int first_call =  1;
	int i,k;
	long index;
	long sum = 0;
	
	/* if first call to hashing, initialize table of random numbers */
    if (first_call) {
		//printf("inside tiles \n");
		for (k = 0; k < 2048; k++) {
			rndseq[k] = 0;
			for (i=0; i < int(sizeof(int)); ++i)
	    		rndseq[k] = (rndseq[k] << 8) | (local_rand() & 0xff);			// use local rand() for consistent hashing across simulations
			}
		first_call = 0;
    }

	for (i = 0; i < num_ints; i++) {
		/* add random table offset for this dimension and wrap around */
		index = ints[i];
		index += (increment * i);
		/* index %= 2048; */
		index = index & 2047;
		while (index < 0) index += 2048;
			
		/* add selected random number to sum */
		sum += (long)rndseq[(int)index];
	}
	index = (int)(sum % m);
	while (index < 0) index += m;
	
	/* printf("index is %d \n", index); */
		
	return(index);
}





