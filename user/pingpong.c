#include "kernel/types.h"
#include "user.h"

int main(int argc,char* argv[]){
  if(argc != 1){
        printf("Wrong! Check argument!\n"); //检查参数数量是否正确
        exit(-1);
    }
	int p1[2],p2[2];
	pipe(p1);
	pipe(p2);
	if(fork()!=0){
    // 父进程向管道p1写入ping
    // 然后从管道p2等待读出
		close(p1[0]);
		write(p1[1],"ping",4);
		close(p1[1]);
		char* buffer = (char*)malloc(5*sizeof(char));
		read(p2[0],buffer,4);
		printf("%d: received %s\n",getpid(),buffer);
		close(p2[0]);
	} else {
    // 子进程从管道p1读出
    // 然后向管道p2写入pong
		close(p2[0]);
		char* buffer = (char*)malloc(5*sizeof(char));
		read(p1[0],buffer,4);
		printf("%d: received %s\n",getpid(),buffer);
		close(p1[0]);
		write(p2[1],"pong",4);
		close(p2[1]);
	}
	exit(0); 
}