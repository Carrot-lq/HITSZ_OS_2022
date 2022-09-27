#include "kernel/types.h"
#include "user.h"
void primes(int p[2]){
  // �����̶��������ɸѡ����ɸѡ��Ϻ������ͨ���ܵ������ӽ��̼����ݹ�
  // ����һ��ݹ鴫��Ĺܵ�p���ر�д�˿ڣ��Ӷ��˿��ж�����һ������Ϊ����ɸѡ�Ļ���
  close(p[1]);
  int num;
  read(p[0], &num, 1);
  // ��������Ϊ0��˵��û�����ˣ�ֱ���˳�
  if(num==0){
    exit(0);
  }
  // ����������ȴ�ɸѡ�����ȴ�ӡ����������ѡ����������
  printf("prime %d\n", num);
  // �½��ܵ�pp���ڸ��ӽ��̴���
  int pp[2];
  pipe(pp);
  if(fork()!=0){
    // ������
    // �رչܵ�pp���˿�
    close(pp[0]);
    // ��p�ܵ���ȡʣ������ɸѡ����pp
    int n;
    do{
      read(p[0], &n, 1);
      if(n%num != 0){
        write(pp[1], &n, 1);
      }
    }while(n!=0);
    // ����ȡ��0��������д��ܵ�
    write(pp[1], &n, 1);
    wait(0);
    exit(0);
  }
  else{
    // �ӽ���
    // �����ݹ�
    primes(pp);
  }
}

int main(int argc, char *argv[]){
  // ����������
  if(argc>1){
    printf("wrong argument!\n");
    exit(-1);
  }
  // �����ܵ����ڴ�����Ҫɸѡ����
  int p[2];
  pipe(p);
  if(fork()!=0){
    // ������
    // �رչܵ����˿�
    close(p[0]);
    // ��2-35д��˿ڣ�����0����
    for(int i = 2; i < 36; i++){
      write(p[1], &i, 1);
    }
    int i = 0;
    write(p[1], &i, 1);
    wait(0);
    exit(0);
  }
  else{
    // �ӽ���
    // �ݹ����primes����ʼ״̬���븸������д�����
    primes(p);
  }
  exit(0);
}