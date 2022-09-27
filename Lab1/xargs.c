#include "kernel/types.h"
#include "kernel/param.h"
#include "user.h"
int main(int argc,char* argv[]){
  char buffer[MAXARG];
  // argv第二个参数是将执行的程序
  char *command = argv[1];
  // 参数列表
  char *params[MAXARG];
  // 存储参数
  for(int i=1; i<argc; i++){
    params[i-1]=argv[i];
  }
  // 循环
  while(1){
    int i=0,len;
    for(i=0;i<MAXARG;i++){
      len = read(0, &buffer[i], 1);
      if(buffer[i] == '\n' || len == 0){
        break;
      }
    }
    // 若没有读入或只读到 '\n'，退出
    if(i==0){
      break;
    }
    // 否则存在有效数据
    // 将buffer最后一位设置为0
    buffer[i]='\0';
    params[argc-1] = buffer;
    params[argc] = 0;
    
    if(fork()==0){
      // 创建子进程执行程序
      exec(command, params);
      exit(0);
    }
    else{
      wait(0);
    }
  }
  exit(0);
}