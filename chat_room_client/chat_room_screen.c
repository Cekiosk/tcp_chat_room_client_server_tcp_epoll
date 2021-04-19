#include "../header/my_func.h"

//----公共协议结构
//-----macro num---
#define MAX_USER 128
#define MAX_IP 20
#define MAX_PORT 10
#define MAX_EPOLL 20
//----传送消息协议，以下两边统一
#define MAX_BUF 1024
#define MAX_TOKEN 11 //TOKEN串长
#define MAX_TRUST 16 //信任头部长
#define MAX_MSG 256 //命令长
#define MAX_PATH 128 //最大路径长度
#define MAX_WORD 4 //短信息最大词数
#define MAX_NAME 64
////----传送消息协议数值要求完

typedef struct msg{
    char termi_type;//c是【输入客户端】,s是【屏幕】，0是【未知/未注册】//其实一般都会填的
    char is_S_enroll;//是否注册屏幕
    char is_C_enroll;
    int id;
    int cont_len;
    time_t time_sec;
    char user_name[MAX_NAME];
    char content[MAX_MSG];
}msg_t;//这个顺序不能改

//----屏幕独有结构

typedef struct screen_system{//server可以获取到与所有设备的联系方式
    int server_fd;
    int c_msg_fd_fd;
}scr_sys_t;

//-----sys

int recv_in_size(int trans_fd,void* recv_buf , int recv_data_len ){
    int recv_sum=0,recv_ret=0;//已经接收的byte，一轮接收的byte
    int is_error=0;//成功为0，失败为-1
    //清空一下接内容的数组//这里我打算直接接住一整个结构体
    memset(recv_buf,0,recv_data_len);//这里是不是清空过头了草//八成就是//如果不这么打印还查不出，打印函数真的很重要
    //in my logic can be set by this, please send len para when you want to operate on space
    //接受结构体专用//[e]不要乱用宏自定义未传进去的长度

    while(recv_sum<recv_data_len)
    {
        recv_ret=recv(trans_fd,(void*) recv_buf,recv_data_len,MSG_WAITALL);
        if(recv_ret==0){
            PRINT_TEST_INT(recv_sum,"recv_sum");
            PRINT_TEST_STR("socket peer is close","recv");
            is_error=-1;
            break;
        }//if为0则对端断开
        else{
            recv_sum += recv_ret;//非0则把本次接收的传进去
            is_error=0;
        }//else则正常接受

    }   //while 接收，出来的时候必然recv_sum==data_len or 错了break了
    //本函数保证长内容一定能传完//虽然其实消息级别的并没这个顾虑
    return is_error;
}


int connect_server( char *server_ip, char *server_port, int *new_fd_p)
{
    int new_fd=socket(AF_INET,SOCK_STREAM,0);//本次socket端口用于主动出击，返回值是自己的文件描述符（以文件对象的形式存在于进程）
    //client端无需维持tcp的socket，一旦连上就是信息通信socketi//所以客户端的new_fd在这一步就定了
    //不bind的话客户端端口是随机选择的
    ERROR_CHECK(new_fd,-1,"socket");//错误检查

    struct sockaddr_in ser_addr;//被出击端（server端）的地址
    memset(&ser_addr,0,sizeof(ser_addr));//memset的空指针就是什么类型都接受的意思，也许是去函数里强转

    ser_addr.sin_family=AF_INET;//ipv4
    ser_addr.sin_port=htons(atoi(server_port));//端口char*->int->转为网络字节序//为绑定对端做准备
    ser_addr.sin_addr.s_addr=inet_addr(server_ip);//char*->一步到位转化为网络ip地址形式
  
    int ret=connect(new_fd,(struct sockaddr*)&ser_addr,sizeof(ser_addr));//主动出击，链接对面，如果对面也在accept(accept其实不用指定特定对象)，它只表示【我支持一次链接】，并不在意具体是谁来链接
    ERROR_CHECK(ret,-1,"connect");//检查connet成功，若不成功，这个sever口是直接凉凉的，要关掉重启

    *new_fd_p=new_fd;//接回最后值
    printf("connect success! client new_fd is %d \n",*new_fd_p);
//    send(sfd,"hello",5,0);//（链接成功）相当于往sfd对应的文件缓冲区里写
//    char buf[128]={0};//buf
//    recv(sfd,buf,sizeof(buf),0);//相当于从recv对应的缓冲区里拿东西
//    printf("I am client get=%s\n",buf);
    //初始化一个socket描述符，用于tcp通信
    return 0;
}


void get_screen_id(msg_t* msg_p,int msg_size, scr_sys_t *sys_p){
    char buf[MAX_IP]={0};
    int id=0;

    int ret=0;

    char server_ip[MAX_IP]="192.168.6.226";;
    char server_port[MAX_IP]="10006";;

    connect_server( server_ip, server_port, &(sys_p->server_fd));//先试图链接

    while(1){//输入控制：如果输入不达标就得卡在这一直输入
        /* rewind(stdin);//清空输入缓冲区//感觉linux下fflush和rewind都没用，得循环用一下getchar */
        memset(buf,0,sizeof(buf));
        printy("Start the [send_msg.out] ,print your [ID] get from user and start your chat :\n");
        fgets(buf,4,stdin);//来到这已经是空缓冲区
        id = atoi(buf);//只转第一个数字
        
        /* PRINT_TEST_INT(id,"what you input"); */

        //接下来确认得到正确才是合法条件
        
        //1-填写id + is_S_enroll
        msg_p->id=id;
        msg_p->is_S_enroll=1;
        msg_p->termi_type='s';

        ret = send(sys_p->server_fd,msg_p,msg_size,MSG_NOSIGNAL);
        ERROR_CHECK(ret,-1,"send");

        ret = recv_in_size(sys_p->server_fd,msg_p,msg_size);

        /* PRINT_TEST_STR_PINK(msg_p->content,"CONT"); */

        if(strcmp(msg_p->content,"ok")==0){//[e]使用字符串比较记得用完整
            //匹配成功
            printg("id match success! start your chatting!\n");
            
            //---修改is_S_enroll=1,content =ready+发送
            
            memset(msg_p,0 ,msg_size);
            msg_p->is_S_enroll=1;
            strcpy(msg_p->content,"ready");
            
            ret = send(sys_p->server_fd,msg_p,msg_size,MSG_NOSIGNAL);
            ERROR_CHECK(ret,-1,"send");

            usleep(1000);
            break;
        }
        else{
            printlr("illegal id ,please try again!\n");
            memset(buf,0,sizeof(buf));//重新接受数字
            memset(msg_p,0,msg_size);//重新填写信息//所以上面一定要重填
            //[E]且此处一定要清空buf，不然会影响之后的判定
            while((getchar())!='\n');//清空输入缓冲区
            //[E]输入缓冲区的结尾不是EOF,是'\n'
            //用的时候已经取出来了，大概后面就是无了，不是EOF
        }//不合法返回
    }//while合法才能出来
}//func



int main(int argc,char*argv[])
{
    system("clear");
    scr_sys_t scr_sys;
    scr_sys_t* scr_sys_p=&scr_sys;//屏幕系统结构体
    msg_t msg;
    msg_t *msg_p=&msg;
    int msg_size = sizeof(msg);
    int ret=0;
    
    get_screen_id( msg_p, msg_size, scr_sys_p);
    //出来目标：拿到正确id，or【出不来】
    
    system("clear");

    printg("==========welcome to the chatting room! ^ ^==========\n");
    while(1){//循环到被退出，一直打印信息
        ret = recv_in_size(scr_sys_p->server_fd,msg_p,msg_size);
        if(ret==-1){
            printlp("peer is close , bye!\n");
            exit(1);
        }
        //打印输出
        printlc("[ID]: [%d] | [",msg_p->id);
        printf(" %s ",msg_p->user_name);
        printlc("] %s",ctime(&(msg_p->time_sec)));
        printf("%s\n",msg_p->content);
    }

    return 0;
}

