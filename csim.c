/*
 * csim.c
 *
 * This program performs a cache simulation over a given valgrind trace with
 * user specifications of the given number of lines, number of sets, and block
 * size. 
 *
 * This file is part of COMP 280, Project 5.
 * 
 * Authors:
 * 1. Scott Kolnes (skolnes@sandiego.edu)
 * 2. Eduardo Ortega (eortega@sandiego.edu)
 * 
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cachelab.h"
#include <math.h>
#include <getopt.h>
#include <string.h>

//forward declarations of memory address data type
//as a means to shorten code entry
typedef unsigned long int mem_addr;

//These are structs that will be used throughout the code
// We used these to define how our cache, sets, lines
struct Line
{
	int valid;
	int tag;
	int LRU;
};
typedef struct Line Line;

struct Set
{
	int num_lines;
	Line *line;
};
typedef struct Set Set;

struct Cache
{
	int num_sets;
	Set *set;
};
typedef struct Cache Cache;

// forward declarations
void simulateCache(char *trace_filename, int set_bits, int block_bits, int E, int *hit_count, int *miss_count, int *eviction_counter);

void makeEmptyCache(Cache *cache, int num_sets, int E);

void printCache(Cache cache);

void isInCache(Cache *cache, mem_addr set, mem_addr tag, 
		int *hit_counter, int *miss_counter, int *eviction_counter, int access_counter);

int findIndexLRU(Cache *cache, mem_addr set_number);


/**
 * Prints out a reminder of how to run the program.
 *
 * @param executable_name String containing the name of the executable.
 */
void usage(char *executable_name) {
	printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>", executable_name);
}

int main(int argc, char *argv[]) {
	//This will become the number of set bits used when looking into the
	//address
	int set_bits;
	//This will become the number of off set block bits
	int block_bits;
	//This will become the number of lines per set
	int E;
	char *trace_filename = NULL;
	int verbose_mode = 0;
	int help_flag = 0;

	opterr = 0;

	// arg variables for the switch statement dealing with optional args for
	// runnning c-sim.c
	int c = -1;

	// Note: adding a colon after the letter states that this option should be
	// followed by an additional value (e.g. "-s 1")
	while ((c = getopt(argc, argv, "vs:E:b:t:")) != -1) {
		switch (c) {
			case 'v':
				// enable verbose mode
				verbose_mode = 1;
				break;
			case 's':
				// specify the number of sets
				// Note: optarg is set by getopt to the string that follows
				// this option (e.g. "-s 2" would assign optarg to the string "2")
				set_bits = strtol(optarg, NULL, 10);

				//num_s = 1 << strtol(optarg, NULL, 10);
				break;

			case 't':
				// specify the trace filename
				trace_filename = optarg;
				break;
			case 'b':
				//Specfiies the number of block bits used for reading the
				//address, once again this is user specified 
				block_bits = strtol(optarg, NULL, 10);
				break;
			case 'E':
				//User specified number of lines for cache
				E = strtol(optarg, NULL, 10);
				break;
			case 'h': 
				//help flag set to 1 if help is needed
				help_flag = 1;
			case '?':
			default:
				printf("Please follow the formatting of the usage of the csim-ref executable!");
				printf("\n");
				usage(argv[0]);
				exit(1);
		}

		if (help_flag) {
			// Optional help flag that prints out what each specifier does
			printf("To run the Cache Simulator, you will need to include specifiers after ./csi    m-ref \n");
			printf("For example here is what the Usauge of the headline would look like: \n");
			printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile> \n");
			printf("The Specifiers you can use and their function are described below \n");
			printf("\n");
		    printf("-h: Optional help flag that prints usage info\n");
		    printf("-v: Optional verbose flag that displays trace info\n");
		    printf("-s <s>: Number of set index bits\n");
	        printf("-E <E>: Associativity (number of lines per set)\n");
		    printf("-b <b>: Number of block bits\n");
		    printf("-t <tracefile>: Name of the Valgrind trace to replay\n");
	        printf("\n");
		    printf("Please note that the -t and -s specifiers are required\n");
		}

		if (verbose_mode) {
			printf("Verbose mode enabled.\n");
		 	printf("Trace filename: %s\n", trace_filename);
		}
	}

	//Variable counters used to keep track of number of hits, misses, and
	//evictions.
	int hit_count = 0;
	int miss_count = 0;
	int eviction_count = 0;

	simulateCache(trace_filename, set_bits, block_bits, E, 
			&hit_count, &miss_count, &eviction_count);
	printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

/**
 * Simulates cache with the specified organization (s, E, b) on the given
 * trace file.
 *
 * @param trace_file 		Name of the file with the memory addresses.
 * @param set_bits 			Number of set bits passed into the simulator.
 * @param block_bits 		Number of block bytes passed into the simulator.
 * @param E 				Number of lines in each cache set.
 * @param hit_counter 		Keep track of the hits we get.
 * @param miss_counter 		Keeps track of the misses we get.
 * @param eveicton_counter 	Keeps track of the evictions we have.
 *
 */
void simulateCache(char *trace_filename, int set_bits, int block_bits, int E, 
		int *hit_counter, int *miss_counter, int *eviction_counter) {
	
 	//Opens up a the trace file and makes an empty cache to be filled
	FILE *trace = fopen(trace_filename, "r");
	int num_sets = pow(2, set_bits);
	Cache cache;
	makeEmptyCache(&cache, num_sets, E);
	printCache(cache);
	
	// Converts read in specifications to the memory address format by casting
	mem_addr addr;
	mem_addr s_bits = (mem_addr)set_bits;
	mem_addr t_bits = (mem_addr)block_bits;

	//Other variables that may be changed
	int read;
	char oper[2];
	int size;

	//this variable will keep track of how many access we make to a given
	//cache
	int access_counter = num_sets;
	 
	//Masks to be later used to unpack memory address into the tag,
	//set index, and block offset, cast from a int value to a unisgned long
	//int
	mem_addr s_mask = (mem_addr)((1 << s_bits) - 1);
	mem_addr t_mask = (mem_addr)((1 << (block_bits + set_bits)) - 1);
	//scans the open file to read in the entries per line, they will be our
	//operation, address, and size
	read = fscanf(trace, "%s %lx, %d", oper, &addr, &size);
	while(read == 3) {
    	if(strcmp(oper, "I") != 0) {
			//masks shifted address bits to get the set and tag
			//bits
			s_bits = s_mask & (addr >> block_bits);
    	    t_bits = t_mask & (addr >> (set_bits + block_bits));
			printf("set = %lu\n", s_bits);
			printf("tag = %lu\n", t_bits);
			//checks the line if it is in the cache
        	isInCache(&cache, s_bits, t_bits, hit_counter, miss_counter, 
					eviction_counter, access_counter);
			//Everytime we access the line we must increment access_counter
			access_counter++;
          }
        if(strcmp(oper, "M") == 0) {
          	//Since M is a load or a save, we would check the line again
		  	isInCache(&cache, s_bits, t_bits, hit_counter, miss_counter, 
					eviction_counter, access_counter);
			//increments access_counter
			access_counter++;
		  }
      	
      	//checks the read value again to ensure that we are still using the
      	//same format in the trace file for addresses
		read= fscanf(trace, "%s %lx, %d", oper, &addr, &size);
    }


    //close the open file
    fclose(trace);
	//frees the memory we used for each line of the sets
	int i;
	for(i = 0; i < cache.num_sets; i++){
		free(cache.set[i].line);
	}
	//freed the memory we used for the set in the cache
	free(cache.set);

}


/*
* Initializes the slots on all of the shelves in our bookcase
*
* @param cache 				The cache we are initializing.
* @param number_of_sets 	The number of sets used to create the cache
* @param E 					The number of lines in each set.
*/
void makeEmptyCache(Cache *cache, int num_sets, int E) {
	//makes memory for the sets of the cache
	cache->num_sets = num_sets;
	cache->set = calloc(num_sets, sizeof(Set));
	 
	//Fills each set with lines, while intializing memory for each of the lines
	int i;
	for (i = 0; i < cache->num_sets; i++) {
		cache->set[i].num_lines = E;
	    cache->set[i].line = calloc(E, sizeof(Line));

		//Sets Valid bit for every line to 0 as well as numbering LRU to be
		//used later
	    for (int j = 0; j < cache->set[i].num_lines; j++) {
			cache->set[i].line[j].valid = 0;
		    cache->set[i].line[j].LRU = j;
	    }
	}
}

/**
 *Prints the cache
 *
 * @param cache 	The cache we are printing
 */
void printCache(Cache cache) {
	int i;
	//Uses i to loop through the number of sets in the cache
	for (i = 0; i < cache.num_sets; i++) {
		int j;
		//Uses j to loop through the number of lines in a set in the cache
		for (j = 0; j < cache.set[i].num_lines; j++) {
			//prints the set, line, valid, LRU of the specific ith set and the
			//jth line
			printf("set: %d; line: %d, valid: %d, LRU: %d\n", i, j, 
					cache.set[i].line[j].valid, cache.set[i].line[j].LRU);		
		}
	}
}

/**
 * finds the line index of the LRU of the given set in the given cache.
 *
 * @param cache 		The cache we are using to find the line index of the LRU
 * @param set_member 	The set number in which we are finding the line index of the
 * LRU 
 *
 * @return 				The line index of the LRU via a int
 */
int findIndexLRU(Cache *cache, mem_addr set_number) {
	//intiatilzes the variable compare_LRU and sets it to the first LRU of the
	//set, this will be used and compared to other lines LRU values
	int compare_LRU = cache->set[set_number].line[0].LRU;
	//intializes LRU_index to keep track of the line index of the LRU value
	int LRU_index = 0;
	//uses i to loop the lines in a given set then
	int i;
	for (i = 0; i < cache->set[set_number].num_lines; i++) {
		//compares the variable compare_LRU to the next lines LRU value, will
		//switch the values if the next lines LRU value is less than the
		//compare_LRU's value then set the LRU_index to the value of i which
		//is the line index
		if (compare_LRU > cache->set[set_number].line[i].LRU) {
			compare_LRU = cache->set[set_number].line[i].LRU;
			LRU_index = i;
		}
	}
	return LRU_index;
}

/**
* Determines if the cache access with the given tag is in the cache in the given set.
*
*
* @param cache 				The cache we are searching.
* @param set__number 		The number of the set we are looking in.
* @param tag 				The tag of the cache access we are looking for.
* @param hit_counter 		The hit_counter will keep track of the hits.
* @param miss_counter		The miss_counter will keep track of the misses.
* @param eviciton_counter	The eveistion_counter will keep track of the
* evictions
*/
void isInCache(Cache *cache, mem_addr set_number, mem_addr tag, 
		int *hit_counter, int *miss_counter, int *eviction_counter, int access_counter) {
	//sets i and flag to 0, i is used to lopp through the number of lines and
	//flag is used to ensure that if we get a hit, do not go to the miss or
	//evict call
	int i;
	int flag = 0;

	for (i = 0; i < cache->set[set_number].num_lines; i++) {
		if (cache->set[set_number].line[i].valid == 1) {
			if (cache->set[set_number].line[i].tag == tag) {
				//HIT!
				*hit_counter += 1;
				flag = 1;
				//Updates LRU here after hit
				cache->set[set_number].line[i].LRU = access_counter;
				break;
			}
		}
	}

	if (flag == 0){
		*miss_counter += 1;
		for (i = 0; i < cache->set[set_number].num_lines; i++) {
			//ONLY RELEVANT FOR FILLING THE CACHE!!!!!!!
			if (cache->set[set_number].line[i].valid == 0) {
				//Update cache and LRU
				cache->set[set_number].line[i].valid = 1; 
				cache->set[set_number].line[i].tag = tag;
				//Updates the LRU here after the miss
			   	cache->set[set_number].line[i].LRU = access_counter;
				break;
			}
			//EVICTS ONLY HAPPEN IF SET IS FILLED !!!!!!!
			//
			//
			//Evict the least recently used if its valid = 1, checking in case
			//the cache is not full
			//If the last recently used is the lowest ... must replace 0
			if (i == (cache->set[set_number].num_lines)-1) {
				if(cache->set[set_number].line[i].valid == 1){
					*eviction_counter += 1;
					//Update cache and LRU with the findIndexLRU function, this
					//will give us the line index of the line that has the LRU
					//of the set.
					int LRU_index = findIndexLRU(cache, set_number);
					cache->set[set_number].line[LRU_index].valid = 1;
					cache->set[set_number].line[LRU_index].tag = tag;
					//line to update LRU;
					cache->set[set_number].line[LRU_index].LRU = access_counter;
					break;
				}
			}
		}
	}
}

