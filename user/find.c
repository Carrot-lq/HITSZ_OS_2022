#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path){
  //static char buf[DIRSIZ+1];
  char *p;

  // ��·��ĩβ��ǰѰ�ҵ���һ�� '/', p ָ�� '/' ����һλ
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  // �����ַ���ƥ����ʵ��findʱ���ļ��������ո���ʵ�ֶ���������DIRSIZ=14��
  // ֱ�ӷ���p����
  return p;
  // ls.c��ԭ��������
  // Return blank-padded name.
  //if(strlen(p) >= DIRSIZ)
  //  return p;
  //memmove(buf, p, strlen(p));
  //memset(buf+strlen(p), ' ', DIRSIZ-strlen(p)); 
  //return buf;
}

void find(char *path, char *filename){
  // buf �����洢·��
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  // ���ļ�·��
  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  // ��ȡ�ļ���Ϣ
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  // �жϸ�·�����ļ������ļ���
  switch(st.type){
  // ������ļ�������fmtname��ȡ�ļ������ж��Ƿ�Ϊ����Ŀ��
  case T_FILE:
    if(strcmp(fmtname(path),filename)==0)
        printf("%s\n",path);
    break;
  
  //������ļ���
  case T_DIR:
    // ·���������˳�
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    // ���浱ǰ·��������buf��������ǰ·�� + '/'��p ָ�� '/' ����һλ
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    // ���ζ�ȡ struct dirent, ÿһ�� de ����ǰ�ļ����µ�һ���ļ�����Ŀ¼��ֱ��������Ϊֹ
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      // ���ļ������ļ�������
      if(de.inum == 0)
        continue;
      //���� . �� .. ����ֹ�ݹ������ѭ��
      if(strcmp(de.name,".")==0 || strcmp(de.name,"..")==0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      // �ݹ������һ��·��
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