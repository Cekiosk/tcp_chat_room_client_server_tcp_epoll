#pragma once
//func.h打卡就有的东西是.h的默认模板
#include<time.h>
#include<sys/time.h>
//[E]stderr是标准IO里的东西，必须加进来
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<sys/stat.h>
#include<sys/types.h>
#include<sys/select.h>
#include<sys/wait.h>
#include <sys/epoll.h>

#include<fcntl.h>
#include<unistd.h>
#include<dirent.h>
//directory entries

#include<errno.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/sem.h>
//proc communication
#include<sys/mman.h>
//memory


//internet
#include<arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include<signal.h>
#include<grp.h>
#include<pwd.h>

//thread
#include <pthread.h>



#include"print_color.h"

#define ARGS_CHECK(argc,num){\
    if(argc != num){\
        fprintf(stderr,"Args Error!\n");\
        return -1;\
     }\
}
//error检查，何种返回值去读取全局变量需要自己判定
#define ERROR_CHECK(ret,num,msg){\
    if(ret == num){\
        perror(msg);\
        exit(-1);\
     }\
}
//在线程里的检查
#define ERROR_CHECK_RETNULL(ret,num,msg){\
    if(ret == num){\
        perror(msg);\
        return NULL;\
     }\
}

#define THREAD_ERROR_CHECK(ret,msg) {\
    if(ret!=0) {\
        printf("[%s] FAILED : %s\n",msg,strerror(ret));\
    }\
}
//无返回值，可以放任何地方，其实宏函数用exit也是常见的选择

//输出字符串和整数检查
#define PRINT_TEST_INT(num,msg) {\
        printf("%s : %d\n",msg,num);\
}

#define PRINT_TEST_LONG_INT(lnum,msg) {\
        printf("%s : %ld\n",msg,lnum);\
}
#define PRINT_TEST_STR(str,msg) {\
    printf("%s : \"%s\"\n",msg,str);\
}
#define PRINT_TEST_STR_YELLOW(str,msg) {\
    printy("%s : \"%s\"\n",msg,str);\
}
#define PRINT_TEST_STR_PINK(str,msg) {\
    printlp("%s : \"%s\"\n",msg,str);\
}
#define PRINT_TEST_CHAR(ch,msg) {\
printf("%s : \"%c\"\n",msg,ch);\
}

#define PRINT_STR(str) {\
    printf("%s\n",str);\
}

//空指针检查

#define IS_NULL_POINTER(p,msg) {\
    if(p==NULL) {\
        printf("%s pointer is NULL .\n",msg);\
    }\
    else{\
        printf("%s pointer is NOT NULL, content is: %p\n",msg,p);\
    }\
}

#define ERROR_CHECK_POINTER(ret_p,err_p,msg){\
    if(ret_p == err_p){\
        perror(msg);\
        exit(-1);\
     }\
}

#define ERROR_CHECK_PERROR(ret,err,msg){\
    if(ret == err){\
        perror(msg);\
        exit(-1);\
     }\
}
//用了exit就不要考虑return的范围了
//用了exit就不要考虑return的范围了
//输出数组
//试试以【注意到宏函数是文本替换】的角度写一个复杂一点的
#define TRAVERSE_INT_ARRAY(a,num,msg) {\
    printf("print array about [%s] ,there are %d elems:\n",msg,num);\
    for(int i=0; i<num; i++){\
        printf("%d ",a[i]);\
    }\
    printf("\n");\
}
//数组的一模一样文本替换应该可行//是的可行
#define TRAVERSE_CHAR_ARRAY(a,num,msg) {\
    printf("print char array about [%s] ,there are %d elems:\n",msg,num);\
    for(int i=0; i<num; i++){\
        printf("[%c]",a[i]);\
    }\
    printf("\n");\
}
