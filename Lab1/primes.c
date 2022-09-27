#include "kernel/types.h"
#include "user.h"
void primes(int p[2]){
  // 父进程对数组进行筛选，将筛选完毕后的数组通过管道传入子进程继续递归
  // 对上一层递归传入的管道p，关闭写端口，从读端口中读出第一个数作为本轮筛选的基数
  close(p[1]);
  int num;
  read(p[0], &num, 1);
  // 若读到的为0，说明没有数了，直接退出
  if(num==0){
    exit(0);
  }
  // 否则存在数等待筛选，首先打印基数（本轮选出的质数）
  printf("prime %d\n", num);
  // 新建管道pp用于父子进程传数
  int pp[2];
  pipe(pp);
  if(fork()!=0){
    // 父进程
    // 关闭管道pp读端口
    close(pp[0]);
    // 从p管道读取剩余数，筛选后传入pp
    int n;
    do{
      read(p[0], &n, 1);
      if(n%num != 0){
        write(pp[1], &n, 1);
      }
    }while(n!=0);
    // 最后读取到0重新往后写入管道
    write(pp[1], &n, 1);
    wait(0);
    exit(0);
  }
  else{
    // 子进程
    // 继续递归
    primes(pp);
  }
}

int main(int argc, char *argv[]){
  // 检查参数数量
  if(argc>1){
    printf("wrong argument!\n");
    exit(-1);
  }
  // 创建管道用于传入需要筛选的数
  int p[2];
  pipe(p);
  if(fork()!=0){
    // 父进程
    // 关闭管道读端口
    close(p[0]);
    // 将2-35写入端口，并以0结束
    for(int i = 2; i < 36; i++){
      write(p[1], &i, 1);
    }
    int i = 0;
    write(p[1], &i, 1);
    wait(0);
    exit(0);
  }
  else{
    // 子进程
    // 递归调用primes，起始状态传入父进程中写入的数
    primes(p);
  }
  exit(0);
}