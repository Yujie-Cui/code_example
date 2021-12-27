/*

Covert channel using Algorithm 1 under Hyper-threaded sharing

Usage: see README 
Authors: Yujie Cui (YujieCui@pku.edu.cn) 


*/



#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include<stdint.h>
#include<math.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#define gettid() syscall(SYS_gettid)
#include <dlfcn.h>
#include <assert.h>

int g_stride=8;// next set
void * chainSta[2];
void ** chainLast[2];

//this code uses set 48 to transfer info

char* chain_array[512*8]; // array to hold the pointer chasing chain  =way_size *8
unsigned int g_result[512*2*256];// to hold 100000 element, only use set 0-31 ((i/512)*1024+i%512)
uint64_t probe[64*35*8]; //64sets * 8 way * linesize
int perm[32]; 

int Tone=11000;
int Tzero=11000;
int Tmonitor=11000;
volatile uint8_t temp = 0;
int para_d=4;
char sender_mode='a';

uint8_t *chain;
uint64_t* way[64][10];
int cacheSetNum = 64;
int cacheLineNum = 8;
int cachelineSize =  64;
  int d ;

uint64_t timing(void)
{
    uint64_t val;

    /*
     * According to ARM DDI 0487F.c, from Armv8.0 to Armv8.5 inclusive, the
     * system counter is at least 56 bits wide; from Armv8.6, the counter
     * must be 64 bits wide.  So the system counter could be less than 64
     * bits wide and it is attributed with the flag 'cap_user_time_short'
     * is true.
     */
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));

    return val;
}


void  createPermutation(int arr[],int size){
//  create random permutation of probe size in perm[]
//  To avoid prefetcher
	srand(10);
	for(int i=0; i < size; i++){
		arr[i]=i;
	}
	for(int i=0; i < size; i++){
		int j = i + (rand()%(size-i));
		if(i!=j){
			int tmp=arr[i];
			arr[i]=arr[j];
			arr[j]=tmp;
		}
	}
}


void print_addr(void *addr){
	printf("The addr is 0x%x   %d \n ",addr,addr);
}

void**  creatChase(void *addr,unsigned int stride,int len){
	
	// DPRINTF("\n-----creatChase-----\n");
	len=len-1;
	void* memory=addr;
	print_addr(addr);
	void** last = (void**) memory;
	int perm[20];
	createPermutation(perm, len);
	for(int i=0;i<len;i++) {
		uint8_t* next = (uint8_t*) memory + (perm[i]+1)*stride; //stride depends to data type (uint8_t +1)
		print_addr(next);
		*last = (void*) next; 
		last = (void**) next;
		
	}
	*last = (void *)memory;
	return last;

}


void setWayArr( uint64_t* way[64][10],uint64_t probe[]){
	int perm[10];
	 for (int i = 0; i< 64;i++) {//64 sets
      createPermutation(perm,9);

      way[i][0]= &probe[cachelineSize/8*i + cachelineSize/8*64];
      for( int j = 0; j < 9;j++){
		way[i][j+1]= &probe[cachelineSize/8*(i+(perm[j]+1)*64) + cachelineSize/8*64];
      }
  }

}





void  sender() {
  	int message[128] = {0,0,0,0, 1,0,1,0, 1,1,1,1, 0,1,0,1};
	unsigned int junk=0;
	 void *sta;
  unsigned long long t=0,t1=0;

  if(sender_mode== '0'){
      for(int i =0; i< 128; i++){
            message[i] = 0;
      }
  }
  else if(sender_mode== '1'){
      for(int i =0; i< 128; i++){
            message[i] = 1;
      }
  }
  else if(sender_mode== 'r'){
        for(int i = 16;i < 128;i++){
        message[i]= rand()%2;
      } //generate random message
  }else{ // mode == 'a'
        for(int i =0; i< 128; i++){
            message[i] = i%2;
        } 
  }
    
  printf("Message to be sent: ");
  for(int i = 0;i < 128;i++){
    printf("%d",message[i]);
  }
  printf("\n");
    
//   unsigned long long t=0,t1=0;
  unsigned long long T_next=0;
    unsigned int No_test=1000;
  int cnt =0;
  for (int p=0;p<No_test/100;p++){
      for(int i=0;i<128;i++){
	
         if(message[i]==0){
			t1= timing();
			temp&=*(way[19][0]);

			do{
                t= timing();
              }
              while( (t-t1) < Tzero  );

         }
       //send zero for Tzero
         else{
     
			t1= timing();

		for(int p=0;p<3;p++){
                for(int k=0;k<d;k++){
                    *way[7][k]=*(way[7][k])+1;
                }
			}
			do{
                t= timing();
              }
              while( (t-t1) < Tone  );
         }

		sta=chainSta[i%2];
		t1=timing();

		asm __volatile__ (
		"DMB ISH   \n"
		"mrs  x7, cntvct_el0   \n"
		"mov  x6, %[addr]  \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"LDR x6, [x6]   \n"
		"DMB ISH   \n"
		"mrs %[result], cntvct_el0   \n"
		"sub %[result],%[result],x7  \n"
		: [result] "=r" (t)
		: [addr] "r" (sta)
		: "%x6","%x7");

		// "sub %[result],x7,x6  \n"
		g_result[((cnt/512)*1024+cnt%512)]= t;
		// printf("index=%d cnt=%d\n",((cnt/512)*1024+cnt%512),cnt);
		cnt++;

		do{
			t = timing();
		} while((t-t1) < Tmonitor );
		

       }
  }

  	for (cnt=0;cnt<No_test/100*128;cnt++){
		printf("%d %d\n",cnt, g_result[((cnt/512)*1024+cnt%512)]);
	}


}



int main(int argc, char *argv[]) {

	for(int i=0;i<sizeof(probe)/8;i++){
		probe[i] = i;
	}

	Tone = 11000;
	Tzero = 11000;
	d=8;

	
	srand(1234);

	setWayArr(way,probe);
	chainSta[0] = &probe[8*7 + 22*cachelineSize/8*64];
	chainLast[0] = creatChase(chainSta[0],64*64,10);
	printf("--------- first set ------------- \n");
	chainSta[1] = &probe[8*7 + 12*cachelineSize/8*64];
	chainLast[1] = creatChase(chainSta[1],64*64,10);

	sender();

  return 0;
}
