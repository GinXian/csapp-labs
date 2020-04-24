#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>


int _hit_ = 0;
int _evi_ = 0;
int _mis_ = 0;

FILE* infile = NULL;

typedef struct {
	unsigned long tag;
	bool valid;
} *pLine,line;

typedef struct node{
	pLine ptr;
	struct node* next;
	struct node* prev;
} *pNode, Node;


int main(int argc, char* argv[])
{


	if(!strcmp(argv[1],"-h")) {
		printf("-h: Optional help flag that prints usage info\n-v: Optional verbose flag that displays trace info\n-s <s>: Number of set index bits (S = 2 s is the number of sets)\n-E <E>: Associativity (number of lines per set)\n-b <b>: Number of block bits (B = 2 b is the block size)\n-t <tracefile>: Name of the valgrind trace to replay\n");
		return 0;
	}


	if(argc==1) {
		printf("Error! Too few arguments!\n");
		return 0;
	}
	bool verbose = !strcmp(argv[1],"-v");
	if(argc < verbose+9) {
		printf("Error! Too few arguments!\n");
		return 0;
	}

	unsigned int _s_ = 0;
	unsigned int _E_ = 0;
	unsigned int _b_ = 0;
	
	for(int i = 1+verbose; i < verbose+6; i+=2) {
		int temp = (int)strtol(argv[i+1],NULL,10);
		if(!temp) {
			printf("Error! Invalid settings!\n");
			return 0;
		}
		if(!strcmp(argv[i],"-s"))
			_s_ = temp;
		else if(!strcmp(argv[i],"-E"))
			_E_ = temp;
		else if(!strcmp(argv[i],"-b"))
			_b_ = temp;

	}
	//printf("%d, %d, %d\n",_s_,_E_,_b_);


	pNode* cache = (pNode*)malloc(sizeof(pNode)*(1<<_s_));
	pNode* tail = (pNode*)malloc(sizeof(pNode)*(1<<_s_));
	for(unsigned long i = 0;i < (1<<_s_); i++) {
		//printf("i=%ld\n",i);
		pNode head = (pNode)malloc(sizeof(Node));
		if(head == NULL) {
			printf("Allocation Fails!\n");
			return -1;
		}
		cache[i] = head;
		head->prev = head;
		pLine holder = (pLine)malloc(sizeof(line));
		head->ptr = holder;
		head->ptr->tag = 0;
		head->ptr->valid = false;
		for(unsigned long j = 1;j < _E_; j++) {
			//printf("j=%ld\t",j);
			head->next = (pNode)malloc(sizeof(Node));
			head->next->prev = head;
			//printf("curr=%p\tnext=%p\t",head,head->next);
			head = head->next;
			holder = (pLine)malloc(sizeof(line));
			head->ptr = holder;
			head->ptr->tag = 0;
			head->ptr->valid = false;
			//printf("curr=%p\tnext=%p\n",head,head->next);
		}
		tail[i] = head;
		head->next = NULL;
	}



	char instr[50] = {' '};
	char* token;
	char* operation;
	
	unsigned long ntag = 0;
	unsigned long nset = 0;

	unsigned long addr = 0;
	unsigned long mems = 0;

	infile = fopen(argv[argc-1],"r");
	if(infile == NULL) {
		printf("Error! Cannot open the file!\n");
		return 0;
	}
	//printf("%s\n",instr);
	while(fgets(instr,50,infile)!=NULL) {
		//printf("%s\n",instr);
		operation = strtok(instr," ,");
		if(!strcmp(operation,"I"))
			continue;
		token = strtok(NULL," ,");
		addr = strtol(token,NULL,16);
		token = strtok(NULL," ,");
		mems = strtol(token,NULL,10);
		//printf("%lx\n",addr);
		//printf("%s\n",instr);


	    unsigned long smask = ((1<<_s_)-1)<<_b_;
		unsigned long tmask = ~((1<<(_s_+_b_))-1);
		//printf("%lx\n%lx\n",smask,tmask);
		ntag = (addr & tmask)>>(_s_+_b_);
		nset = (addr & smask)>>_b_;
		//printf("%ld\n%ld\n",ntag,nset);
		bool isValid = true;
		bool isHit = false;
		
		pNode temp = cache[nset];
			
		while(temp != NULL) {
			isValid &= temp->ptr->valid;
			if(temp->ptr->tag == ntag && temp->ptr->valid) {
				_hit_++;
				isHit = true;
				if(temp == cache[nset]) { // if it is the first node that is hit
					printf("%s %lx,%ld hit",operation,addr,mems);
					break;	
				}
				if(temp == tail[nset]) { // if it is the rear that is hit
					temp = tail[nset]->prev;						
					cache[nset]->prev = tail[nset];
					tail[nset]->prev->next = NULL;
					tail[nset]->next = cache[nset];
					cache[nset] = tail[nset];
					cache[nset]->prev = cache[nset];
					// update the tail
					printf("%s %lx,%ld hit",operation,addr,mems);
					tail[nset] = temp;
					break;
				}
				temp->next->prev = temp->prev;
				temp->prev->next = temp->next;
				temp->next = cache[nset];
				temp->prev = temp;
				cache[nset]->prev = temp;
				cache[nset] = temp;
				printf("%s %lx,%ld hit",operation,addr,mems);
				break;
			}			
			temp = temp->next;
		} 


		if(!isHit) {
			printf("%s %lx,%ld miss",operation,addr,mems);
			_mis_++;
			if(isValid) {
				printf(" eviction");
				_evi_++;
			}
			// move the previous rear to the head and update the block
			if(_E_ != 1) { // if there is only one line, no need for operations
				temp = tail[nset]->prev;
				cache[nset]->prev = tail[nset];
				tail[nset]->prev->next = NULL;
				tail[nset]->next = cache[nset];
				cache[nset] = tail[nset];
				cache[nset]->prev = cache[nset];
				// update the tail
				tail[nset] = temp;	
			}
			cache[nset]->ptr->tag = ntag;
			cache[nset]->ptr->valid = true;
		}
	

		if(operation[0] == 'M')	{
			_hit_++;	
			printf(" hit");
		}
		printf("\n");
	}
	
	fclose(infile);
	


	pNode release = NULL;	
	pNode thePrev = NULL;
	for(int i = 0; i < (1<<_s_); i++) {
		release = cache[i];
		for(int j = 1; j < _E_; j++) {
			free(release->ptr);
			thePrev = release->next;
			free(release);
			release = thePrev;
		}		
	}
	free(cache);
	free(tail);
	cache = NULL;
	tail = NULL;
	
	
    printSummary(_hit_, _mis_, _evi_);
    return 0;

}
