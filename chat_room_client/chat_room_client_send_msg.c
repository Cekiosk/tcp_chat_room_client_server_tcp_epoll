#include "../header/my_func.h"

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

//------客户端独有结构

typedef struct server_node{
    char server_ip[MAX_IP];
    char server_port[MAX_PORT];
}server_node_t;

//没有负载均衡，客户端结点可以大幅简化
typedef struct alive_client_sys{
    //本次活跃信息结点
    char is_src;//是否有屏幕
    char is_c_msg;//是否被分配id
    char is_legal;//屏幕和自己都登录了才legal
    int scr_id;
    int server_fd;
    int epfd;//记录本进程epoll池子的fd
    int user_id;//登陆成功则填写唯一用户id
    char user_name[MAX_NAME];

}alive_client_sys_t;


//===========协议一致性：msg_t结构体完全相等
////[q]对这东西在不同机器对齐的合法性保持有一点疑问,但应该可以先用

//===========函数区


void traverse_array_int(int* a, int num, char *msg){
    printf("start to print %s array: \n",msg);
    for(int i=0; i<num ; i++){
        printf("%d ", a[i]);
    }
    printf("\n");

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
    printf("Connect SERVER success! \n ");
//    send(sfd,"hello",5,0);//（链接成功）相当于往sfd对应的文件缓冲区里写
//    char buf[128]={0};//buf
//    recv(sfd,buf,sizeof(buf),0);//相当于从recv对应的缓冲区里拿东西
//    printf("I am client get=%s\n",buf);
    //初始化一个socket描述符，用于tcp通信
    return 0;
}




int epoll_add(alive_client_sys_t* client_sys_p,int new_fd,int events){//注册epol与修改alve_msg
    //这里有的做法会封装结构体，我不加了，这个epoll并不复杂

    //--开始注册客户端//负载均衡不在这里，负载均衡仅发生在长任务阶段，epoll之后，负载均衡不会影响主服务器的链接(只能主决定从，不能从决定主)
    struct epoll_event ep_event={0};//这是epoll官方定义的结构体，epoll_event
    //准备注册新接入的客户端
    ep_event.data.fd=new_fd;
    ep_event.events=events;//监控读端//[Q]对监控写端还有好奇，之后看看
    //对epoll池的实际操作有并发可能，需要互斥锁
    //传进来了活跃结构体，但我还未用，因为main_server大几率就1个//但有必要把epoll池子装填
    //test
//    pthread_mutex_lock(&(user_msg_p->tcp_conn_mutex));//mutex锁上保护接下来的代码区//记得mutex一定是取地址的//这里锁不上会暂时阻塞
    int ret=epoll_ctl(client_sys_p->epfd,EPOLL_CTL_ADD, new_fd, &ep_event);//咩有看错，new_fd写了两次，因为epoll很懒
    ERROR_CHECK(ret,-1,"epoll_ctl");
//    pthread_mutex_unlock(&(user_msg_p->tcp_conn_mutex));//mutex锁上保护接下来的代码区//记得mutex一定是取地址的//这里锁不上会暂时阻塞
    /* printf("add %d in epoll success!\n",new_fd); */
    return 0;
}//test: epoll_add加锁测试无问题


int create_epoll_in_client(alive_client_sys_t* client_sys_p){

    //----创建epoll池 creant_epoll_in_client    
    int epfd=epoll_create(1);//后面的监控数量实际上已经是无用参数了
    printf("aftrer create epoll..\n");
    client_sys_p->epfd=epfd;
    return 0;
}

void get_user_name(alive_client_sys_t *sys_p,msg_t* msg_p){//有可能【命令行的那个回车真的是留在输入缓冲区的】
    /* getchar();//吸收一下命令行的回车//那也不应该 */
    /* printf("in\n"); */
    //---test:先测试，输入控制
    int max_name_size = MAX_NAME-2;//因为buf一个放0（刚需）一个放'\n'
    while(1){//输入控制：如果输入不达标就得卡在这一直输入
        /* rewind(stdin);//清空输入缓冲区//感觉linux下fflush和rewind都没用，得循环用一下getchar */
        printlc("print your user name within %d byte:\n",max_name_size);
        fgets(msg_p->user_name,max_name_size+2,stdin);//来到这已经是空缓冲区
        //[R]实验结果：解析本函数，意思就是【传入的数字对于接口来说设想是<你传入的buf大小>】，所以最后【1次可以接出来的如果正常最后一个绝对应该填0】//好耶！！可以借此判操作啦
        //[M]+读取输入包含回车，也就是说：
        //1-想限制【用户输入M个字符】，那么【缓冲区得<留一个给回车>，<留一个给\0>，实际上是<限制缓冲区大小为M+2>】//---而判定即buf[M](第M+1个字符，不为'\n'(刚合法)也不为'\0'(还有余裕))-->第一个也不能是\n,也不合法
        
        //[R]一个输入控制有这么多学问，绝了
        /* PRINT_TEST_STR(buf,"what you get");//这个还不能用，得把回车裁掉 */
        if(!(msg_p->user_name[max_name_size]=='\n'||msg_p->user_name[max_name_size]=='\0')){//过长不合法
            /* PRINT_TEST_STR(buf,"in too long");//这个还不能用，得把回车裁掉 */
            //[E]bug:要设复杂条件【取反】的时候，麻烦别忘了那个【取反】,也要理清楚关系
            //[E]复杂数组的边界条件还是在word写一下
            printlr("USER NAME TOO LONG, PLEASE INPUT AGAIN!\n");
            /* char c=0; */

            memset(msg_p->user_name,0,MAX_NAME);//最后的值就是name_buffer的大小
            //[E]且此处一定要清空buf，不然会影响之后的判定
            while((getchar())!='\n');//[E]输入缓冲区的结尾不是EOF,是'\n'
                /* printf("%c\n",c); */
                //用的时候已经取出来了，大概后面就是无了，不是EOF
        }
        else if(msg_p->user_name[0]=='\n'){//设定姓名为空不合法
            printlr("NULL NAME IS IILEGAL!\n");
        }
        else{
            //得修改一下把回车给去了
            int name_len = strlen(msg_p->user_name);//得到长度，然后把[长度-1]设为0//去回车
            msg_p->user_name[name_len-1]=0;
            printy("Set name SUCCESS! You new name is : [%s]\n",msg_p->user_name);
            printf("Please start");
            printy(" [screen.out] ");
            printf("and fill in the");
            printy(" [ID] ");
            printf("get next in it for your chatting! \n");

            strcpy(sys_p->user_name,msg_p->user_name);//名字存入系统

            break;
        }
    }  //while 格式不对会一直卡 
}


int get_msg_from_stdin(msg_t* msg_p, int max_msg_size){
    int is_ok=0;//=1为可以，=0为【不可以】
    //清空内容区（仅仅清空这个）
    memset(msg_p->content,0,MAX_MSG);
    fgets(msg_p->content,max_msg_size+2,stdin);//来到这已经是空缓冲区
        //1-想限制【用户输入M个字符】，那么【缓冲区得<留一个给回车>，<留一个给\0>，实际上是<限制缓冲区大小为M+2>】//---而判定即buf[M](第M+1个字符，不为'\n'(刚合法)也不为'\0'(还有余裕))-->第一个也不能是\n,也不合法
        
    //[max_msg_size] 
    if(!(msg_p->content[max_msg_size]=='\n'||msg_p->content[max_msg_size]=='\0')){//过长不合法
        //[E]复杂数组的边界条件还是在word写一下
        printlr("MSG TOO LONG, PLEASE INPUT AGAIN!\n");
        /* char c=0; */
        memset(msg_p->content,0,MAX_MSG);//最后的值就是name_buffer的大小
        //[E]且此处一定要清空buf，不然会影响之后的判定
        while((getchar())!='\n');//[E]输入缓冲区的结尾不是EOF,是'\n'
    }
        else if(msg_p->user_name[0]=='\n'){//设定姓名为空不合法
            printlr("NULL MSG, INPUT AGAIN!\n");
        }
        else{
            //得修改一下把回车给去了
            int name_len = strlen(msg_p->content);
            //得到长度，然后把[长度-1]设为0//去回车
            msg_p->content[name_len-1]=0;
            is_ok=1;
        }
        return is_ok;
}

int recv_in_size(int trans_fd,void* recv_buf , int recv_data_len ){
    int recv_sum=0,recv_ret=0;//已经接收的byte，一轮接收的byte
    int is_error=0;//成功为0，失败为-1
    //清空一下接内容的数组//这里我打算直接接住一整个结构体
    memset(recv_buf,0,MAX_BUF);

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
int main(int argc,char*argv[])
{
    system("clear");
    //1-维护客户端所需信息
    alive_client_sys_t client_sys={0};
    //主函数存在期间都存在
    //初始化结点，未登录，其他全为0，在最初没什么好初始化的，随着之后添加
    alive_client_sys_t * client_sys_p=&client_sys;//test

#if 1
    //2-tcp连接
    char server_port[]="10006";//对端端口
    char server_ip[]="192.168.6.226";//准备连接对面
    int new_fd=0;   //接住内部传出的内容

    connect_server( server_ip, server_port, &new_fd );//同时维护登录信息
    client_sys_p->server_fd = new_fd;  //记录main_server
    
    
    //3-epoll相关
    //-----建立epoll池//并注册相关信息到活跃表
    create_epoll_in_client(client_sys_p);
    PRINT_TEST_INT(client_sys_p->epfd,"client epfd");
    //-----池内注册标准输入
    epoll_add(client_sys_p, STDIN_FILENO, EPOLLIN);
    //-----池内注册服务器链接读端
    epoll_add(client_sys_p, new_fd, EPOLLIN);

    //我不需要维护很多服务器相关信息，只有一个服务器
    /* PRINT_TEST_INT(client_sys_p->server_fd,"server_fd"); */    
#endif
    //4-准备信息传送结构体//我不用小火车结构，每次都传1个结构体
    msg_t msg={0};
    msg_t *msg_p = &msg;
    int msg_size=sizeof(msg);
    int ret=0;//接返回值

    //5-[开启准备1]:请用户输出信息
    get_user_name(client_sys_p,msg_p);//这个函数如果不合法是会循环卡住的
    
    //6-[开启准备2]: 发送【第1个请求注册包给服务器】
    msg_p->termi_type = 'c';
    msg_p->is_C_enroll=1;
    //用户名已填好//每次还是想初始化哪个特地弄吧

    ret = send(client_sys_p->server_fd,msg_p,msg_size,MSG_NOSIGNAL);//命令都往main_fd发//send (fd,(void*)addr, len, nosignal)
    ERROR_CHECK(ret,-1,"send");

    //7-立马准备接收返回name，都做完才能进入正常工序
    ret=recv_in_size(client_sys_p->server_fd,msg_p,msg_size);
    if(ret==-1){
        //退出机制
        PRINT_TEST_STR("error","recv id");
        exit(1);
    }
    /* PRINT_TEST_STR("get some message","from main_server"); */
    //接收id +注册
    if(msg_p->is_C_enroll==1){
        client_sys_p->user_id = msg_p->id;//接收id
        client_sys_p->is_c_msg=1;//已分配id成为可发送服务器
        printf("Get your id from server : [");
        printlr("  %d  ",client_sys_p->user_id);
        printf("].\n");

        printlr("Please fill it in your [screen.out] for further chatting...\n");
    };
    //还要server发来第二次信息才可以开始聊天
    //等待服务器返回【屏幕就绪】
    /* sleep(600); */
    printy("wait for check\n");
    ret=recv_in_size(client_sys_p->server_fd,msg_p,msg_size);
    if(ret==-1){
        //退出机制
        PRINT_TEST_STR("error","wait screen check");
        exit(1);
    }
    printy("get check: %s \n",msg_p->content);
    
    if(msg_p->is_S_enroll==1){//是屏幕的确认
        if(strcmp(msg_p->content,"s_ready")==0){
            //收到确认
            printlg("screen (id:[%d]) is ready, u can start chatting!^ ^\n",msg_p->id);
            client_sys_p->is_src=1;//scr就绪
            client_sys_p->is_legal=1;//合法
            client_sys_p->scr_id=msg_p->id;
        }

    }

    /* if();//屏幕接通，开始聊天 */

    //---#正式开始epoll监听+聊天机制
    int ready_event_num=0;
    struct epoll_event ready_events[MAX_EPOLL]={0};
    

    while(1)//[E]这个循环内的recv如果不处理，对端断开就会反复出于就绪可读状态
    {//epoll是要不断读取的，所以放进循环
        /* PRINT_TEST_STR("start to wait for epoll and sleep","main_thread"); */
        /* system("clear"); */



        printb("==============[INPUT MSG AND SEND WITH \"ENTER\"]==============="); //[M]此处如要加锁，就得加cond锁，待补完
        printf("\n");//把颜色改回白色
        ready_event_num = epoll_wait(client_sys_p->epfd,ready_events,MAX_EPOLL,-1);//(监听的epoll, 接住的数组，最大数目，超时机制(是无则等待))
        //这里不睡眠应该就总有机会拿到了
        /* PRINT_TEST_INT(ready_event_num,"ready_event_num"); */
        for(int i=0; i<ready_event_num; i++)
            //for循环：解决所有的epoll响应事件//这个响应和recv有相似之处
        {
            /*消息来自对端服务器*/
            if(ready_events[i].data.fd==client_sys_p->server_fd)
            {//消息来自对端server //此处得用recvn
            //得区分1-拿id 2-退出机制

                //无火车头，直接收1整个结构体
                ret=recv_in_size(client_sys_p->server_fd,msg_p,msg_size);
                if(ret==-1){
                    //退出机制
                    PRINT_TEST_STR("a peer is close","client");

                    return -1;
                }
                PRINT_TEST_STR("get some message","from main_server");
                //可能性：1-【id】 2-【退出机制】
                //1-接收id +注册
                if(msg_p->is_C_enroll==1){
                   client_sys_p->user_id = msg_p->id;//接收id
                   client_sys_p->is_c_msg=1;//已分配id成为可发送服务器
                   printf("Get your id from server : [");
                   printy("  %d  ",client_sys_p->user_id);
                   printf("].\n");

                   printlr("Please fill it in your [screen.out] for further chatting...\n");
                //等待服务器的回复               
                   ret=recv_in_size(client_sys_p->server_fd,msg_p,msg_size);
                   if(ret==-1){
                       //退出机制
                       PRINT_TEST_STR("the peer is close","client");
                       return -1;
                   }

                }


            }//if 对端server 结束
            //else if 输入缓冲区 //[M]在此添加解析//2021/3/17:目标完成这个解析命令
            else if(ready_events[i].data.fd== STDIN_FILENO){
                //从键盘接数据
                //必然要清空msg发送了
                memset(msg_p,0,msg_size);
                int max_msg_size=MAX_MSG;//MAX_MSG;//这个是buf大小，实际大小得+2
                int is_ok=0;
                while(1){
                      is_ok = get_msg_from_stdin( msg_p, max_msg_size); //接收字符+合法判断
                      if(is_ok==1){
                        break;//否则就要一直在里面
                      }
                }
                /* PRINT_TEST_STR_YELLOW(msg_p->content,"input msg"); */
                msg_p->termi_type='c';//来自客户端
                msg_p->is_C_enroll=2;//来自C+CENROLL是2是普通消息
                msg_p->time_sec=time(NULL);//记录时间戳
                strcpy(msg_p->user_name,client_sys_p->user_name);
                msg_p->id=client_sys_p->user_id;
                //把消息送到服务器
                ret = send(client_sys_p->server_fd,msg_p,msg_size,MSG_NOSIGNAL);
                ERROR_CHECK(ret,-1,"send");
                printf("\033[2J") ;
                /* system("cl") ; */           
            }//else键盘
        }//for 循环够epoll事件次数//里面的continue会跳掉自己被处理这次循环
    }//while循环监听处理epoll//vimplus标记尾巴很有必要
    return 0;
}



