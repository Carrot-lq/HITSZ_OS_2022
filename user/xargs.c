#include "kernel/types.h"
#include "kernel/param.h"
#include "user.h"
int main(int argc,char* argv[]){
  char buffer[MAXARG];
  // argv�ڶ��������ǽ�ִ�еĳ���
  char *command = argv[1];
  // �����б�
  char *params[MAXARG];
  // �洢����
  for(int i=1; i<argc; i++){
    params[i-1]=argv[i];
  }
  // ѭ��
  while(1){
    int i=0,len;
    for(i=0;i<MAXARG;i++){
      len = read(0, &buffer[i], 1);
      if(buffer[i] == '\n' || len == 0){
        break;
      }
    }
    // ��û�ж����ֻ���� '\n'���˳�
    if(i==0){
      break;
    }
    // ���������Ч����
    // ��buffer���һλ����Ϊ0
    buffer[i]='\0';
    params[argc-1] = buffer;
    params[argc] = 0;
    
    if(fork()==0){
      // �����ӽ���ִ�г���
      exec(command, params);
      exit(0);
    }
    else{
      wait(0);
    }
  }
  exit(0);
}