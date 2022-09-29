#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path){
  //static char buf[DIRSIZ+1];
  char *p;

  // 从路径末尾向前寻找到第一个 '/', p 指向 '/' 的下一位
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  // 进行字符串匹配以实现find时在文件名后不填充空格以实现定长（长度DIRSIZ=14）
  // 直接返回p即可
  return p;
  // ls.c中原代码如下
  // Return blank-padded name.
  //if(strlen(p) >= DIRSIZ)
  //  return p;
  //memmove(buf, p, strlen(p));
  //memset(buf+strlen(p), ' ', DIRSIZ-strlen(p)); 
  //return buf;
}

void find(char *path, char *filename){
  // buf 用来存储路径
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  // 打开文件路径
  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  // 读取文件信息
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  // 判断该路径是文件还是文件夹
  switch(st.type){
  // 如果是文件，调用fmtname提取文件名并判断是否为所找目标
  case T_FILE:
    if(strcmp(fmtname(path),filename)==0)
        printf("%s\n",path);
    break;
  
  //如果是文件夹
  case T_DIR:
    // 路径过长，退出
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    // 保存当前路径，其中buf包括：当前路径 + '/'，p 指向 '/' 的下一位
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    // 依次读取 struct dirent, 每一个 de 代表当前文件夹下的一个文件或子目录，直到读不了为止
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      // 若文件夹无文件则跳过
      if(de.inum == 0)
        continue;
      //跳过 . 和 .. ，防止递归进入死循环
      if(strcmp(de.name,".")==0 || strcmp(de.name,"..")==0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      // 递归查找下一个路径
      find(buf,filename);
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[]){
  if(argc<3){
    printf("not enough argument\n");
    exit(-1);
  }
  find(argv[1],argv[2]);
  exit(0);
}