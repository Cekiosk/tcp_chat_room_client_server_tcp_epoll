#include "./header/my_func.h"

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
////----传送消息协议完

typedef void* (*thread_func_t)(void *);//定义一个返回值与参数如此的函数类型

typedef struct alive_user_node{//【用户活跃信息结点】作用范围：用户保持活跃期间(对应fd被关闭之前)
    char ip[MAX_IP];
    char port[MAX_PORT];//不开放用户多端登录，1用户1次登陆对应1ip
    int user_fd_id;//用户唯一标识符//同时也是在server的fd，也是下标
    int src_fd_id;//屏幕标识符
    char user_name[MAX_NAME];//用户名
}alive_user_node_t;//用户活跃信息结点，1用户配1个


typedef struct alive_user_sys{
    int user_num;//记录此时维持tcp连接的客户端数量
    alive_user_node_t alive_user_table[MAX_USER*2];//活跃用户信息数组
    //【存在】结点机制需要改in退出机制
    int is_alive[MAX_USER*2];//为啥*2因为1用户配1屏幕//这个退出机制要改
    int is_c_fd[MAX_USER*2];
    int is_src_fd[MAX_USER*2];//查询表放外面吧
    //查询表退出机制不用改，得基于【存在】才能改
    int query_c_fd_by_s[MAX_USER*2];//本次链接唯一标识符+本结点在alive_user_table里存放的位置//一个服务器对一个客户端，就是一对一
    int qurey_s_fd_by_c[MAX_USER*2];
    int tcp_sfd;//对外监听网络连接的fd号
    int tcp_epfd;//监听tcp连接的epoll池子对应的fd号
    pthread_mutex_t tcp_conn_mutex;//tcp操作的互斥锁//大概用不到，因为我只有一个epoll
    pthread_t pthread_id ;//负责连接事宜的子线程id,只有一个的话无需弄指针
    thread_func_t access_client_thread;
}alive_user_sys_t;

typedef struct msg{
    char termi_type;//c是【输入客户端】,s是【屏幕】，0是【未知/未注册】//其实一般都会填的
    char is_S_enroll;//是否注册屏幕
    char is_C_enroll;
    int id;
    int cont_len;
    time_t time_sec;
    char user_name[MAX_NAME];
    char content[MAX_MSG];
}msg_t;

//==============func
void traverse_array_int(int* a, int num, char *msg){
    printf("[%s] array: \n",msg);
    for(int i=0; i<num ; i++){
        printf("%d ", a[i]);
    }
    printf("\n");

}

void print_alive_sys_msg(alive_user_sys_t* sys_p,char *msg){
    printy("=========alive sys in [ %s ]==========\n",msg);
    PRINT_TEST_INT(sys_p->user_num,"user_num");
    PRINT_TEST_INT(sys_p->tcp_sfd,"sfd");
    traverse_array_int(sys_p->is_alive,20,"is_alive");
    traverse_array_int(sys_p->is_c_fd,20,"is_c_fd");
    traverse_array_int(sys_p->is_src_fd,20,"is_src_fd");
    for(int i=0 ; i< sys_p->user_num*2+10 ;i++){
        if(sys_p->is_c_fd[i]==1){
            //证明命中用户，输出用户信息
            printy("user_id\t user_name\t user_src_fd\t \tuser_c_msg_ip\t\n");
            printf("%d\t [%s]\t\t %d\t     \t%s\t\n",sys_p->alive_user_table[i].user_fd_id, sys_p->alive_user_table[i].user_name, sys_p->alive_user_table[i].src_fd_id, sys_p->alive_user_table[i].ip);
        }
    }
    printy("=========alive sys end======\n");
    
}

int tcp_init_server(char* pub_ip, char *pub_port, int *tcp_fd_addr)
{//是int因为所有error check都return int
    
/*
 * 函数封装：
 * [1]server公开tcp端口初始化函数 tcp_init_server：
 * #输入：1-char *pub_ip  2-char *pub_port 3-int *tcp_fd_addr(传出参数，传出端口fd) 
 * #输出：1-初始化好的端口fd 2-监听状态的端口
 * */
    int tcp_fd=socket(AF_INET,SOCK_STREAM,0);//ipv4, tcp,无其他协议
    ERROR_CHECK(tcp_fd,-1,"socket"); //socket初始化命令，失败返回1
    struct sockaddr_in ser_addr;//声明地址结构体

    memset(&ser_addr,0,sizeof(ser_addr));//memset的空指针就是什么类型都接受的意思，也许是去函数里强转
    //bzero(&ser_addr,sizeof(ser_addr));//部分编译器不支持，最好不要用
    ser_addr.sin_family=AF_INET;//ipv4
    ser_addr.sin_port=htons(atoi(pub_port));//端口转为网络字节序
    ser_addr.sin_addr.s_addr=inet_addr(pub_ip);//初始化地址

    int reuse=1;//定义reuse//为设置端口可重用做准备//这个属性只要不为0都行
    int ret;//为设置端口重用做准备
                //↓绑定端口 域
    ret=setsockopt(tcp_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(int));//设置地址端口重用
    ERROR_CHECK(ret,-1,"setsockopt");//端口属性设置

    // 开始绑定ip地址和端口到tcp_fd上
    ret=bind(tcp_fd,(struct sockaddr *)&ser_addr,sizeof(ser_addr));
    ERROR_CHECK(ret,-1,"bind");

    //tcp端口开始监听
    listen(tcp_fd,32);//最大同时处理32个
    *tcp_fd_addr=tcp_fd;
    
    printf("port : %s in %s start to listen, return fd %d ...\n",pub_port,pub_ip,*tcp_fd_addr);

    return 0;
}//先在main写，然后挪过来


int accept_client(int *new_fd_p, alive_user_sys_t* user_msg_p){
    //随时等待接收新函数
    //可能会睡觉的函数

    printf("start to sleep and wait for new client...\n");
    //--------------accept 多个客户端
    int new_fd=*new_fd_p;//提前定义接住【TCP交流管道】的【文件描述符】//[e]in tcp server deffers from sfd
    //传入传出参数常用技巧，先解引用再取引用
    struct sockaddr_in client_addr;//定义【客户端地址】//即：【连上以后想要获取的对面的地址】//不过这个也没必要，链接1建立好，直接从fd就可以读取
    memset(&client_addr,0,sizeof(client_addr));//memset的空指针就是什么类型都接受的意思，也许是去函数里强转
    socklen_t addrlen=sizeof(client_addr);//得到地址结构大小

    new_fd=accept(user_msg_p->tcp_sfd,(struct sockaddr *)&client_addr,&addrlen);
    
    //accept参数，【监听端口(打探是否有请求到来)】，【传入接收对面的结构体】，【地址结构体的长度】
    
    //[A]:功能更新：【分清接入的是屏幕还是客户端，并作出对应反应】//总之链接是会先建立的
    
    ERROR_CHECK(new_fd,-1,"accept");

    printf("new_fd:%d\n",new_fd);
    //激活用户表对应结点+配备链接//处理链接的线程只有1个，无并行性，所以不需要加锁//epoll的处理有并行性
    user_msg_p->is_alive[new_fd]=1;//好的，就这样就好//其他交给具体的
    //这里不做【记录活跃】以外的任何操作//也许会记录ip输出一下//方便追查
    sprintf(user_msg_p->alive_user_table[new_fd].ip,"%s",inet_ntoa(client_addr.sin_addr));//填写ip和port
    sprintf(user_msg_p->alive_user_table[new_fd].port,"%d",ntohs(client_addr.sin_port));//填写ip和port
    //    //accept最大的作用就是对【pub_socket】【光杀鸡不取蛋】，用它【另开新的new_fd做沟通】，而【不会占用监听端口】
    //    //client链接什么，不是server指定的，但server会把配对成功的client地址拿回来
    //    printf("get client success,client ip=%s,client port=%d\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));//获取对面地址并打印（不想打印的话取地址的参数就写NULL）
    user_msg_p->user_num++;//记得增加链接数目
    *new_fd_p =new_fd;
    return 0;
}

int epoll_add(alive_user_sys_t *user_msg_p,int new_fd,int events){//注册epol与修改alve_msg
    //这里有的做法会封装结构体，我不加了，这个epoll并不复杂

    //--开始注册客户端//负载均衡不在这里，负载均衡仅发生在长任务阶段，epoll之后，负载均衡不会影响主服务器的链接(只能主决定从，不能从决定主)
    struct epoll_event ep_event={0};//这是epoll官方定义的结构体，epoll_event
    //准备注册新接入的客户端
    ep_event.data.fd=new_fd;
    ep_event.events=events;//监控读端//[Q]对监控写端还有好奇，之后看看
    //对epoll池的实际操作有并发可能，需要互斥锁
    //[r]感觉：大结构体可以便利的解决很多问题
    pthread_mutex_lock(&(user_msg_p->tcp_conn_mutex));//mutex锁上保护接下来的代码区//记得mutex一定是取地址的//这里锁不上会暂时阻塞
    int ret=epoll_ctl(user_msg_p->tcp_epfd,EPOLL_CTL_ADD, new_fd, &ep_event);//咩有看错，new_fd写了两次，因为epoll很懒
    ERROR_CHECK(ret,-1,"epoll_ctl");
    pthread_mutex_unlock(&(user_msg_p->tcp_conn_mutex));//mutex锁上保护接下来的代码区//记得mutex一定是取地址的//这里锁不上会暂时阻塞
    printf("add %d in epoll success!\n",new_fd);
    return 0;
}//test: epoll_add加锁测试无问题//操作epoll都加一下锁//那epollwait前后也要加锁

void* access_client_conn_thread(void *arg){//线程函数
    alive_user_sys_t *user_msg_p =(alive_user_sys_t*)arg;//指针强转与赋值
    int new_fd=0;//每轮添加新conn要用
    printf("thread for tcp connect success start...\n");
    while(1){//循环执行accept+add_epoll
        accept_client(&new_fd, user_msg_p);//可能会睡觉的函数//accept新客户端的部分
        printf("get client in fd [%d] success,client ip=%s,client port=%s\n",new_fd,user_msg_p->alive_user_table[new_fd].ip,user_msg_p->alive_user_table[new_fd].port);//获取对面地址并打印（不想打印的话取地址的参数就写NULL）
        epoll_add(user_msg_p,new_fd,EPOLLIN);
    }
    return NULL;
}

int user_msg_init(int tcp_epfd,int tcp_sfd ,alive_user_sys_t* user_msg_p){//负责listen的端口fd要写出:sfd

    //----初始化user_msg表
    user_msg_p->access_client_thread = access_client_conn_thread;//声明好对应线程函数
    user_msg_p->tcp_epfd=tcp_epfd;
    user_msg_p->tcp_sfd=tcp_sfd;
    int ret = pthread_mutex_init(&(user_msg_p->tcp_conn_mutex),NULL);//必须取地址，不然不是同一个互斥锁，互斥锁都要取地址（也有些跟着结构体的取地址一起）
    THREAD_ERROR_CHECK(ret,"pthread_mutex_init");
    //初始化互斥锁//epoll_add加红黑树，epoll_wait加双向链表，没有冲突
    //其他都在最开始默认初始化为0了
    printf("alive msg init success! \n");
    return 0;
}



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



int main(int argc,char*argv[])
{

    //--0-准备好监听

    char pub_port[]="10006";//准备公开的端口
    char pub_ip[]="192.168.6.226";//准备公开接听电话的ip
    int tcp_sfd=0;//接住传出来的fd
    //↑这部分是外面传进来的
    tcp_init_server( pub_ip, pub_port, &tcp_sfd);
    printf("the listen fd is : %d\n",tcp_sfd);

    //--1-准备epoll的事件池子

    int tcp_epfd=epoll_create(1);//后面的监控数量实际上已经是无用参数了
    printf("aftrer create epoll..\n");
    //----初始化用户表
    alive_user_sys_t alive_user_sys = {0} ;//用户表
    alive_user_sys_t* alive_user_sys_p=&alive_user_sys;
    user_msg_init(tcp_epfd,tcp_sfd,&alive_user_sys);//[E]被监听的fd必须加入
    printf("test: epfd %d \n",alive_user_sys.tcp_epfd);
    //--3-打开监听tcp线程
    //   user_msg.access_client_thread(&user_msg);  
    // int ret=pthread_create(&(user_msg.pthread_id),NULL,user_msg.access_client_thread,&user_msg);//接住线程id地址，属性NULL，函数，最后是参数，
    //[E]记得引入线程动态库那个makefile
    int ret=pthread_create(&(alive_user_sys.pthread_id),NULL,alive_user_sys.access_client_thread,&alive_user_sys);//接住线程id地址，属性NULL，函数，最后是参数，
    THREAD_ERROR_CHECK(ret,"pthread_create");
    printf("main thread after start tcp thread:\n");

    //--4-开始监//注册都发生在线程了
    epoll_add(alive_user_sys_p,STDIN_FILENO,EPOLLIN);
    //注册标准输入

    //---准备好接住传出事件的epoll数组

    struct epoll_event ready_events[MAX_EPOLL]={0};
    int ready_event_num=0;
    //---准备好接住每次传出的数据//直接接一个结构体

    msg_t msg ={0};//是某一个用户传进来的数据
    msg_t *msg_p = &msg;
    int msg_size=sizeof(msg);

    int wake_fd=-1;//填写【每轮哪个激活fd被读出了】，如果不设置，就是不合法的
    int new_fd=5;
    while(1)
    {//epoll是要不断读取的，所以放进循环
        // printf("222\n");
        PRINT_TEST_STR("start to wait for epoll and sleep","main_thread");
        //这里要加锁我怀疑加的条件变量锁//完成注册以后才能激活//但按理来说那边在注册这边应该还是在睡觉啊
        ready_event_num = epoll_wait(tcp_epfd,ready_events,MAX_EPOLL,-1);//(监听的epoll, 接住的数组，最大数目，无则等待)
        //这里不睡眠应该就总有机会拿到了
         struct epoll_event event={0};//移除专用
        char buf[MAX_BUF]={0};
        PRINT_TEST_INT(ready_event_num,"ready_event_num");

        printy("epoll awake in fd [%d] success,client ip=%s,client port=%s\n",new_fd,alive_user_sys_p->alive_user_table[new_fd].ip,alive_user_sys_p->alive_user_table[new_fd].port);//获取对面地址并打印（不想打印的话取地址的参数就写NULL）
        for(int cur_ready=0; cur_ready<ready_event_num; cur_ready++)
        {//ready_num>0才进得来
            //感觉退出机制一定要在外层了
            wake_fd = ready_events[cur_ready].data.fd;//分支之前记录【本轮判定的fd】
            /*1-来自活跃对端/非键盘*/
            if(alive_user_sys_p->is_alive[wake_fd]==1){//确定是活跃结点
                /* printy("is_alive in fd [%d] success,client ip=%s,client port=%s\n",new_fd,alive_user_sys_p->alive_user_table[new_fd].ip,alive_user_sys_p->alive_user_table[new_fd].port);//获取对面地址并打印（不想打印的话取地址的参数就写NULL） */
                //[r]debug经验，包围错误点输出法
                
                //先接受返回值+退出机制
                //1-----先接受内容//这个会导致退出机制不便被接到
                ret = recv_in_size(wake_fd, msg_p, msg_size);//内容接出来放进结构体了
                if(ret == -1){//暂用
                    int other_fd = -1;//可能不合法
                    printlr("%d is exit .\n",wake_fd);
                    if(alive_user_sys_p->is_c_fd[wake_fd]==1){//是客户端关闭
                        other_fd =alive_user_sys_p->qurey_s_fd_by_c[wake_fd];
                    }else{

                        other_fd =alive_user_sys_p->query_c_fd_by_s[wake_fd];
                    }
                        //关闭自己的
                        event.events=EPOLLIN;
                        event.data.fd=wake_fd;
                        epoll_ctl(alive_user_sys_p->tcp_epfd,EPOLL_CTL_DEL,wake_fd,&event);
                        close(wake_fd);

                        alive_user_sys_p->is_alive[wake_fd]=0;
                        alive_user_sys_p->is_c_fd[wake_fd]=0;//query表不用改
                        alive_user_sys_p->is_src_fd[wake_fd]=0;//query表不用改
                        //虽然不推荐写一起图省事先这么写了

                        //关闭别人的
                        event.events=EPOLLIN;
                        event.data.fd=other_fd;
                        epoll_ctl(alive_user_sys_p->tcp_epfd,EPOLL_CTL_DEL,other_fd,&event);
                        close(other_fd);

                        alive_user_sys_p->is_alive[other_fd]=0;
                        alive_user_sys_p->is_c_fd[other_fd]=0;//query表不用改
                        alive_user_sys_p->is_src_fd[other_fd]=0;//query表不用改
                        
                        break;
                    }
                /*1.1-未注册的对端/连是什么都不知道 */
                if(alive_user_sys_p->is_c_fd[wake_fd]==0 && alive_user_sys_p->is_src_fd[wake_fd]==0){//未注册肯定都为0
                
                    //这里收一下返回值
                    printy("type: %c\n",msg_p->termi_type);
                    //-----接收完毕
                    
                    /* printy("c msg enroll in fd [%d] success,client ip=%s,client port=%s\n",new_fd,alive_user_sys_p->alive_user_table[new_fd].ip,alive_user_sys_p->alive_user_table[new_fd].port);//获取对面地址并打印（不想打印的话取地址的参数就写NULL） */
                    if(msg_p->termi_type=='c' && msg_p->is_C_enroll==1){//
                       //是客户端+是来注册的+已经带上了用户名
                        //1-注册用户
                        alive_user_sys_p->is_c_fd[wake_fd]=1;//激活【客户端系统】位图
                        //USER_TABLE的下标以id / 客户端的fd 为准
                        alive_user_sys_p->alive_user_table[wake_fd].user_fd_id=wake_fd;

                        //一定别忘了+用户数量
                        (alive_user_sys_p->user_num)++;
                        strcpy(alive_user_sys_p->alive_user_table[wake_fd].user_name,msg_p->user_name);
                        PRINT_TEST_STR_PINK(msg_p->user_name,"name");//注册完毕，注册【已有屏幕】也完毕
                        /* sleep(1);//看一下是不是并行填写时机的问题-->并不是 *///是内存操作失误
                        print_alive_sys_msg( alive_user_sys_p, "after c enroll");

                        //2-填了id+c_enroll返回去
                        msg_p->is_C_enroll=1;
                        msg_p->id = wake_fd;//就是c_msg的对应fd

                        ret = send(wake_fd,msg_p,msg_size,MSG_NOSIGNAL);
                        ERROR_CHECK(ret,-1,"send");
                        
                    }//if客户端注册

                    else if(msg_p->termi_type=='s' && msg_p->is_S_enroll==1){//屏幕注册

                        //屏幕发来确认//if:id对上活跃结点
                        if(alive_user_sys_p->is_c_fd[msg_p->id]==1){
                            //对上了，返回成功
                            PRINT_STR("screen match");
                            strcpy(msg_p->content,"ok");
                            //填写活跃屏幕与对应关系
                            alive_user_sys_p->is_src_fd[wake_fd] = 1;
                            alive_user_sys_p->qurey_s_fd_by_c[msg_p->id]=wake_fd;
                            alive_user_sys_p->query_c_fd_by_s[wake_fd]=msg_p->id;//互相查询
                            print_alive_sys_msg(alive_user_sys_p,"after screen");
                        }//准备返回
                        else{//没有的话就是你乱输入的
                            PRINT_STR("screen NOT match");
                            strcpy(msg_p->content,"no");
                        }//核对发来的id
                        //发送核对结果
                        ret = send(wake_fd,msg_p,msg_size,MSG_NOSIGNAL);
                        ERROR_CHECK(ret,-1,"send");
                    }//if是屏幕注册
                }//if对端未注册
                /*1.1-已注册的对端 *///1-处理确认 2-转发信息 
                if(alive_user_sys_p->is_c_fd[wake_fd]==1 || alive_user_sys_p->is_src_fd[wake_fd]==1){
                    printy("alive + erolled \n");
                    //if已注册对端屏幕
                    if(alive_user_sys_p->is_src_fd[wake_fd]==1 && msg_p->is_S_enroll==1){
                    printy("alive + erolled + screen \n");
                        //是【屏幕】发来的【准备好确认】
                        if(strcmp(msg_p->content,"ready")==0){
                            printlp("[%d] s screen is ready \n",alive_user_sys_p->query_c_fd_by_s[wake_fd]);
                            //给聊天发送口【确认】
                            memset(msg_p,0,msg_size);
                            msg_p->is_S_enroll=1;
                            msg_p->id = wake_fd;
                            strcpy(msg_p->content,"s_ready");

                            usleep(500);
                            //注意，此处是【发送给对应的发送信息口】，而非【发回给屏幕】
                            ret = send(alive_user_sys_p->query_c_fd_by_s[wake_fd],msg_p,msg_size,MSG_NOSIGNAL);
                            ERROR_CHECK(ret,-1,"send");
                        }

                    }//已注册对端屏幕
                    else if(alive_user_sys_p->is_c_fd[wake_fd] ==1 && msg_p->termi_type=='c'){
                        if(msg_p->is_C_enroll==2){
                            PRINT_TEST_STR_PINK(msg_p->content,"MSG GET");
                            //顺利收到+转发
                            for(int i =0 ; i < (alive_user_sys_p->user_num*2+10) ;i++){
                                //保证可遍历完屏幕的范围
                                if(alive_user_sys_p->is_src_fd[i]==1){
                                    //确认是屏//直接转发信息
                                    ret = send(i,msg_p,msg_size,MSG_NOSIGNAL);
                                    ERROR_CHECK(ret,-1,"send");
                                }
                            }
                        }//if类型是消息，广播转发

                    }//已注册对端c_msg
                }
            }//if是活跃结点
            if(wake_fd == STDIN_FILENO){//来自键盘

        /* rewind(stdin);//清空输入缓冲区//感觉linux下fflush和rewind都没用，得循环用一下getchar */
                memset(buf,0,sizeof(buf));
                printy("server op:\n");

                int size = read(STDIN_FILENO, buf , sizeof(buf));//最后一个参数要放可以使用的最大内存
                ERROR_CHECK(size,-1,"read");
                int fd = atoi(buf);//只转第一个数字
                printf("get fd %d \n",fd);
        
                /* PRINT_TEST_INT(id,"what you input"); */

                //接下来确认得到正确才是合法条件
                /* PRINT_TEST_STR_PINK(msg_p->content,"CONT"); */
                print_alive_sys_msg(alive_user_sys_p,"before delete");

                if(alive_user_sys_p->is_c_fd[fd]==1||alive_user_sys_p->is_src_fd[fd]==1){//暂用
                    printlr("WARNING :%d will be deleted, sure?(y / n)\n",fd);
                    scanf("%c",&buf[0]);
                    
                    while((getchar())!='\n');//清空输入缓冲区

                    if(buf[0]=='y'||buf[0]=='Y'){
                        int other_fd = -1;//可能不合法
                        wake_fd=fd;//为了复用代码偷懒
                        printlr("%d, whose name is [%s] is exit .\n",wake_fd,alive_user_sys_p->alive_user_table[fd].user_name);
                        if(alive_user_sys_p->is_c_fd[wake_fd]==1){//是客户端关闭
                            other_fd =alive_user_sys_p->qurey_s_fd_by_c[wake_fd];
                        }else{
                            other_fd =alive_user_sys_p->query_c_fd_by_s[wake_fd];
                        }
                        //关闭自己的
                        event.events=EPOLLIN;
                        event.data.fd=wake_fd;
                        epoll_ctl(alive_user_sys_p->tcp_epfd,EPOLL_CTL_DEL,wake_fd,&event);
                        close(wake_fd);

                        alive_user_sys_p->is_alive[wake_fd]=0;
                        alive_user_sys_p->is_c_fd[wake_fd]=0;//query表不用改
                        alive_user_sys_p->is_src_fd[wake_fd]=0;//query表不用改
                        //虽然不推荐写一起图省事先这么写了

                        //关闭别人的
                        event.events=EPOLLIN;
                        event.data.fd=other_fd;
                        epoll_ctl(alive_user_sys_p->tcp_epfd,EPOLL_CTL_DEL,other_fd,&event);
                        close(other_fd);

                        alive_user_sys_p->is_alive[other_fd]=0;
                        alive_user_sys_p->is_c_fd[other_fd]=0;//query表不用改
                        alive_user_sys_p->is_src_fd[other_fd]=0;//query表不用改

                        break;
                    }
                    memset(buf,0,sizeof(buf));//重新接受数字
                    while((getchar())!='\n');//清空输入缓冲区
                }//if确认删除

            }//if键盘响应
        }//for 循环够epoll事件次数
    }//while循环监听处理epoll//vimplus标记尾巴很有必要

    return 0;
}

