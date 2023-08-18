#include "user/user.h"
#include "kernel/types.h"

int
main(int argc, char* argv[]){

  if(argc != 2){
    fprintf(2, "usage: sleep pattern [file ...]\n");
    exit(1);
  }
  //fprintf(1,"main\n");
  char* str = argv[1];
  int s = atoi(str);
 
  exit(sleep(s));

}
