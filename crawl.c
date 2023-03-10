
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h> // for opendir, readdir
#include <unistd.h>
#include <stdlib.h> // for exit()
#include "fifoq.h"  // API for fifoq_t

//Define constants
#define MAXLINE 256
#define NAMESIZE 256
#define MAXTHREADS 8

char *searchstring;  // string to look for in files
fifoq_t jobq;        // FIFO queue of file names
int total;   // total # of occurrences found so far
int nfiles;  // # of files processed

 pthread_mutex_t totLock; //Lock for the total variable.
 pthread_mutex_t nFilLock; //Lock for the nfiles variable.

/*
 * Function to count the occurences of the string in the file.
 * Takes a file pointer and a string to search for as arguments.
 * Returns the total number of occurences found.
 */
int countOcc(FILE* fileP, char* strToFind){
	int strSize = 0;
	int retVal = 0;

	//Calculates the size of the string.
	while(strToFind[strSize] != '\0'){
		strSize++;
	}

	//Creates a buffer variable of size maxline to store the next line of input from the file.
	char* lineS = (char*)malloc(MAXLINE);
	lineS = fgets(lineS, MAXLINE, fileP);

	//Executes until the end of the file is reached. The following counts how many times 
	//the string is found in a given line, adds one to the return value each time, and gets a new line.
	while(!feof(fileP)){
		int index = 0; //Index for current line of the file
		int strIndex = 0; //Index for the string to search for.
		int run = 0;  //Tracks whenever you are on a "run", meaning at least the first character of the string to search for has been found and no inccorect characters have.
		int flipped = 0; //Tracks if the state of the run variable changed from one iteration to the next. This is to handle overlapping and adjacent finds of the string.
		while(lineS[index] != '\n'){
			if(lineS[index] == strToFind[strIndex]){
				strIndex++;
				run = 1;
			}else{
				if(run == 1){
					flipped = 1;
					run = 0;
				}
				strIndex = 0;
			}
			if(flipped == 0){
				index++;
			}else{
				flipped = 0;
			}
			if(strIndex == strSize){
				strIndex = 0;
				retVal++;
			}
		}
		free(lineS);
		lineS = (char*)malloc(MAXLINE);
		lineS = fgets(lineS, MAXLINE, fileP);
		if(feof(fileP)){
			free(lineS);
		}
	}
	return retVal;
}


/* 
 * Function that will be executed by worker threads.
 * do "forever":  Get a filename from the queue.
 * If it's a directory, walk through it and add a copy of
 * each name to the queue. If a text file read each line
 * and scan for occurrences of searchstring. Otherwise it can be ignored.
 */
void *worker(void *ignored) {
  char *name;
  struct stat finfo;
  FILE *f;
  struct dirent *dir;
  for (;;) {  // loop until main thread exits
 
    /* get a filename, stat it
     * - if it is a regular file, count the occurrences of the search string
     * - if a directory, add each filename it contains to the queue
     * - otherwise, ignore it.
     */
	int updateCount = 0;
	name = (char*)fq_get(&jobq);
	int errno = stat(name, &finfo);

	//If the file is a textfile, open it and call updateCount.
	if(S_ISREG(finfo.st_mode)){
		f = fopen(name, "r");
		if(f == NULL){
			printf("Error: Problem opening file %s", name);
			exit(9);
		}
		printf("scanning %s\n", name);
		updateCount = countOcc(f, searchstring);
		fclose(f);
		pthread_mutex_lock(&nFilLock);
        	nfiles++;
        	pthread_mutex_lock(&totLock);
		total = total + updateCount;
        	printf("scanned %d; found %d occurrences.\n",nfiles,total);
        	fflush(stdout);
        	pthread_mutex_unlock(&totLock);
        	pthread_mutex_unlock(&nFilLock);
	}else{

		//If the file is a directory add all subfiles to the queue.
		if(S_ISDIR(finfo.st_mode)){
			DIR* d = opendir(name);
			if(d == NULL){
				printf("Error: Problem opening directory %s", name);
				exit(9);
			}
			while((dir = readdir(d)) != NULL){
				//The following condition statement avoids pushing "." and ".." onto the queue.
				if(!(dir->d_name[0] == '.' && dir->d_name[1] == '\0') && !(dir->d_name[0] == '.' && dir->d_name[1] == '.' && dir->d_name[2] == '\0') ){
					char filename[NAMESIZE];
					snprintf(filename, NAMESIZE+1, "%s/%s", name, dir->d_name);
				        printf("adding %s\n", filename);
					char* add = malloc(MAXLINE);
					int j = 0;
					while(filename[j] != '\0'){
						add[j] = filename[j];
						j++;
					}
					add[j] = '\0';
					fq_add(&jobq, add);
				}
			}
			closedir(d);
		}
	}
  } 
}

/* main thread:
 *  argv[1] is # of threads
 *  argv[2] is search string - store pointer in global "searchstring"
 *  argv[3], argv[4], ... are names of files to search.
 */
int main(int argc, char *argv[]) {
  int i,N;
  pthread_t *tids;
  
  //Check args
  if (argc < 4) {
    fprintf(stderr,"Usage: %s <#threads> <target> <file1> [<file2> ...]\n",
	    argv[0]);
    exit(1);
  }

  /* argc >= 4 */
  N = atoi(argv[1]); /* check range */
  if(N < 1 || N > MAXTHREADS){
	  printf("Error: Number of threads must be between 1 and 8, inclusive.\n");
	  exit(2);
  }

  //Allocate memory for thread IDs and assign searchstring.
  tids = (pthread_t *)calloc(N,sizeof(pthread_t));
  searchstring = argv[2];

  //Initialize FIFO queue and mutexes.
  fq_init(&jobq, N);
  if (pthread_mutex_init(&totLock, NULL)) {
  	fprintf(stderr,"mutex init");
  	exit(9);
  }
  if (pthread_mutex_init(&nFilLock, NULL)) {
  	fprintf(stderr,"mutex init");
  	exit(9);
  }

  //Add all extra arguments to the queue as items to be searched.
  for (i=3; i<argc; i++) {
	  fq_add(&jobq, argv[i]);
  }
  
  //Create N threads.
  for (i=0; i<N; i++) {
	  if(pthread_create(&tids[i],NULL, worker, NULL)){
		  fprintf(stderr, "pthread_create_failed\n");
		  exit(9);
	  }
  }
  
  fq_finish(&jobq); //fq_finish blocks until the queue is empty and all threads are waiting.
  exit(0); // this kills all threads
}
