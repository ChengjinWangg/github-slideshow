#include "common.h"
#define ETH "eth"
#define COMMENTS '#'


/**
 * [read_config description]    读取节点信息的配置文件
 * @param  fname [description]  配置文件
 * @return       [description]  节点的信息
 */
node *read_config( const char *fname) {
    printf("read_config begin............................\n");
	node *nodes = NULL;
	FILE *fp = fopen(fname,"r");
	if(!fp) return nodes;

	nodes =(node *) malloc(NodeNumTotal*sizeof(node));
	char strbuf[512]={'\0'};

	int i;
   //数据节点k个 4个 前几个节点就是数据节点 0-3节点作为数据节点
    for(i=0;i<K && !feof(fp);i++){
        while(fgets(strbuf,sizeof(strbuf),fp) && strbuf[0]==COMMENTS);
    	sscanf(strbuf,"%s%s",nodes[i].ip,nodes[i].dev);
    	 nodes[i].num = i;
    }
    /*   //读取备选节点n个 4-7节点作为备选节点
     for(i=K;i<2*K && !feof(fp);i++){
        while(fgets(strbuf,sizeof(strbuf),fp) && strbuf[0]==COMMENTS);
        sscanf(strbuf,"%s",nodes[i].ip);
        nodes[i].num = i;
     }

    //校验节点有r个  2个 8-9号节点作为校验节点
     for(i=2*K;i<2*K+R && !feof(fp);i++){
        while(fgets(strbuf,sizeof(strbuf),fp) && strbuf[0]==COMMENTS);
     	sscanf(strbuf,"%s",nodes[i].ip);
     	nodes[i].num = i;
     }
*/
     for(i=K;i<NodeNumTotal-2 && !feof(fp);i++){
        while(fgets(strbuf,sizeof(strbuf),fp) && strbuf[0]==COMMENTS);
        sscanf(strbuf,"%s%s",nodes[i].ip,nodes[i].dev);
        nodes[i].num = i;
     }


     //处理客户端节点 10号节点作为客户端节点
     while(fgets(strbuf,sizeof(strbuf),fp) && strbuf[0]==COMMENTS);
     sscanf(strbuf,"%s",nodes[NodeNumTotal-2].ip);
     nodes[NodeNumTotal-2].num = NodeNumTotal-2;

     //处理协调器节点  11号节点作为协调器节点
     while(fgets(strbuf,sizeof(strbuf),fp) && strbuf[0]==COMMENTS);
     sscanf(strbuf,"%s",nodes[NodeNumTotal-1].ip);
     nodes[NodeNumTotal-1].num = NodeNumTotal-1;
     fclose(fp);
     return nodes;
}

/**
 * [get_ipaddr description]  获取本机的IP地址
 * @param ip  [description]  
 * @param len [description]
 */
void get_ipaddr(char *ip,int len){
    printf("get_ipaddr begin......................................\n");
    int i=0,sockfd=-1,tlen;
   // printf("1\n");
    if(!ip || len<16)return;
    //printf("2\n");
    struct sockaddr_in *sip;
    struct ifreq *irp;
    struct ifconf ic;
    char buf[512],*iptemp,ethstr[4]={'\0'};
   // printf("3\n");
    ic.ifc_len = 512;
   // printf("4\n");
    ic.ifc_buf = buf;
    //printf("5\n");
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    //printf("6\n");
    ioctl(sockfd, SIOCGIFCONF, &ic);
   // printf("7\n");
    irp = (struct ifreq*)buf;
   // printf("8\n");
    for(i=(ic.ifc_len/sizeof(struct ifreq)); i>0; i--,irp++){
      //  printf("9\n");
        sip=(struct sockaddr_in*)&(irp->ifr_addr);
      //  printf("10\n");
        iptemp=inet_ntoa(sip->sin_addr);
     //   printf("11\n");
        tlen=1+strlen(iptemp);
     //   printf("12\n");
        memcpy(ethstr,irp->ifr_name,3);
     //   printf("13\n");
        if(0==strcmp(ETH,ethstr)){
     //       printf("14\n");
            if(len >= tlen)strncpy(ip,iptemp,tlen);
     //       printf("15\n");
            break;
        }
    }
  //  printf("16\n");
    close(sockfd);
}

/**
 * [current_node description]  获取当前节点编号
 * @param  p [description]
 * @return   [description]    当前节点编号 0-12
 */
int current_node(node * nodes){
    printf("current_node begin................................\n");
    char ip[17];
   // printf("b\n");
    if(!nodes)return -1;
  //  printf("c\n");
    get_ipaddr(ip,16);
 //   printf("%s\n", ip);
  //  printf("d\n");
    int i;
  //  printf("e\n");
    for(i=0;i<NodeNumTotal;i++){
 //       printf("f\n");
    if(strcmp(ip,nodes[i].ip)==0) return nodes[i].num; //比较ip地址是否相同 相同的话返回节点编号
 //   printf("g\n");
    }
 //   printf("h\n");
    return -1;
}

/**
 * [get_dev_name description]   返回数据节点的设备地址
 * @param  nodes [description]  节点信息
 * @param  nid   [description]  节点编号
 * @return       [description]  节点设备名
 */
char *get_dev_name(node *nodes,int nid){
    if(!nodes||nid<1)return NULL;
    return nodes[nid].dev;
}

/**
 * [connect_try description]   client尝试连接server
 * @param  ip   [description]  服务端ip 地址
 * @param  port [description]  服务端端口号
 * @return      [description]  返回套接字sockfd
 */
int connect_try(char *ip,ushort port){//支持重连接
    printf("connect_try begin..................................\n");
    int nsec,sockfd;
    //struct linger lg={1,5};
    struct sockaddr_in sa;   //地址
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr=inet_addr(ip);

    sockfd=socket(AF_INET,SOCK_STREAM,0);  //socket 套接字

    if((-1 != sockfd))
    {
        for(nsec=1;nsec <= Max_Wait;nsec <<= 1)
        {
            if(0 == connect(sockfd,(struct sockaddr *)&sa,sizeof(sa))){  //客户端建立连接，若成功返回0，若出错返回-1

            int sendsize = 51200000;
            int svcsize=51200000;
            if(setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&sendsize, sizeof(sendsize)) < 0){//设置发送缓冲区大小
            perror("setsockopt");
            exit(1);
            }
            if(setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char*)&svcsize, sizeof(svcsize)) < 0){ //设置接收缓冲区大小
            perror("setsockopt");
            exit(1);
            }
            int flag = 1;   
            if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(int)) < 0){//设置端口复用
            perror("setsockopt");
            exit(1);
            }
            if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)) < 0){//屏蔽Nagle算法，避免不可预测的延迟
            perror("setsockopt");
            exit(1);
            }
             
            //printf("connect %c succeed\n", &ip);  //连接成功
            return sockfd;//连接Server 
            }
            if(nsec <= Max_Wait/2)sleep(nsec);
        }
        close(sockfd);
    }
    return -1;
}

/**
 * [server_accept description]      服务端建立连接  server
  * @param  port    [description]   服务端端口号
 *  @param  backlog [description]   socket可排队的最大连接个数
 *  @return         [description]    通信套接字
 */
int server_accept(ushort port,int backlog){
    printf("server_accept begin .........................................\n");
    int listen_sock;//创建监听套接字
    int connect_sock;//创建通信套接字

    int opt = SO_REUSEADDR;

    struct sockaddr_in sa;  //地址
    socklen_t slen = sizeof(sa);  //协议地址的长度
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);

    listen_sock=socket(AF_INET, SOCK_STREAM, 0); //套接字建立  域  套接字类型 0

    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));//SO_REUSEADDR 仅仅表示可以重用本地本地地址、本地端口
    
    bind(listen_sock, (struct sockaddr *)&sa, sizeof(sa));  //将套接字与地址绑定  套接字  地址 地址长度
   
   //服务端监听 成功返回0  出错返回-1 。c语言规定，任何非0的数像1 -1等都被认为是真，而0被认为是假
    if (listen(listen_sock, backlog)<0){  
        printf("portListen ~ listen() error.\n");
        return -1;
    }


    while (1){

        connect_sock = accept(listen_sock, (struct sockaddr *)&sa, &slen);  //通信套接字 成功返回套接字 出错返回-1

        if (-1 == connect_sock){
            printf("portListen ~ accept() error.\n");
        }else{
            printf("server_accept made .client ip ：%s connect to local port:%hu .\n", inet_ntoa(sa.sin_addr), port);
            break;
        }
    }

    int sendsize=51200000;
    int svcsize=51200000;
    if(setsockopt(connect_sock, SOL_SOCKET, SO_RCVBUF, (char*)&sendsize, sizeof(sendsize)) < 0){//设置发送缓冲区大小
        perror("setsockopt");
        exit(1);
    }
    if(setsockopt(connect_sock, SOL_SOCKET, SO_SNDBUF, (char*)&svcsize, sizeof(svcsize)) < 0){ //设置接收缓冲区大小
        perror("setsockopt");
        exit(1);
    }
    int flag = 1;   
    if(setsockopt(connect_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0){//设置端口复用
        perror("setsockopt");
        exit(1);
    }
    if(setsockopt(connect_sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)) < 0){//屏蔽Nagle算法，避免不可预测的延迟
        perror("setsockopt");
        exit(1);
    }


    return connect_sock;  //返回通信套接字  成功返回套接字 出错返回-1
}


/**
 * [read_bytes description]         一次读文件
 * @param  fd     [description]      套接字
 * @param  buf    [description]     字节缓冲区
 * @param  nbytes [description]     要读取的文件总数
 * @return        [description]     返回读取的文件总数
 */
ssize_t read_bytes(int fd,char *buf,size_t nbytes){
    size_t total=0;
    int once;
    for(once=0;total<nbytes;total+=once){
        once = read(fd,buf+total,nbytes-total);  //返回值大于0,表示读了部分或者是全部的数据
        if(once<=0)break;
    }
    return total;
}

/**
 * [write_bytes description]
 * @param  fd     [description]
 * @param  buf    [description]
 * @param  nbytes [description]
 * @return        [description]
 */
ssize_t write_bytes(int fd,char *buf,size_t nbytes){//一次写文件
    size_t total=0;
    int once;
    for(once=0;total<nbytes;total+=once){
        once = write(fd,buf+total,nbytes-total);  //返回值大于0,表示写了部分或者是全部的数据
        if(once<=0)break;
    }
    return total;
}


/**
 * [send_bytes description]       一次性发送文件
 * @param  sockfd [description]   套接字
 * @param  buf    [description]   缓冲区
 * @param  nbytes [description]   字节大小
 * @return        [description]   总的发送文件大小
 */
ssize_t send_bytes(int sockfd,char *buf,size_t nbytes){
    size_t total=0;
    int once;
    for(once=0;total<nbytes;total+=once){
    once=send(sockfd,buf+total,nbytes-total,0);
    if(once<=0)break;
    }
    return total;
}


/**
 * [recv_bytes description]      一次性接收文件
 * @param  sockfd [description]
 * @param  buf    [description]
 * @param  nbytes [description]
 * @return        [description]
 */
ssize_t recv_bytes(int sockfd,char *buf,size_t nbytes){
    size_t total=0;
    int once;
    for(once=0;total<nbytes;total+=once){
        once=recv(sockfd,buf+total,nbytes-total,0);
        if(once<=0)break;
    }
    return total;
}



/**
 * [calc_xor description]    XOR模拟纠删码  *************??????*********
 * @param d1  [description]  数据块D1
 * @param d2  [description]  数据块D2
 * @param out [description]  XOR运算后输出的数据块
 * @param len [description]
 */
void calc_xor(char *d1,char * d2,char *out,size_t len){
    size_t i,j;
    long *pd1=(long *)d1;
    long *pd2=(long *)d2;
    long *plout=(long *)out;
    if(!d1 || !d2 || !out)return;
    for(i=0; i < len/sizeof(long);i++) *(plout+i)= *(pd1+i)^ *(pd2+i);
    if(0 != len%sizeof(long)){
        for(j=i*sizeof(long);j<len;j++)  *(out+i)= *(d1+j) ^ *(d2+j);
    }
}

/**
 * [set_server_limit description] 设置服务器限制
 * @return  [description]
 */
int set_server_limit(void)
{
    int res;
    struct rlimit old_lim, new_lim;

    res = getrlimit(RLIMIT_NOFILE, &old_lim);//getrlimit()获得当前资源限制?
    if(res != 0){
        printf("Getrlimit(): error!\n");
        return -1;
    }
    
    //printf("Old soft limit: %ld, hard limit: %ld\n", old_lim.rlim_cur, old_lim.rlim_max);//获得当前资源的软限制和硬限制

    if(old_lim.rlim_max < SOCKMAX)
        old_lim.rlim_max = SOCKMAX;//设置硬限制为1MB

    new_lim.rlim_cur = old_lim.rlim_max;
    new_lim.rlim_max = old_lim.rlim_max;//设置软硬件限制相同 
    res = setrlimit(RLIMIT_NOFILE, &new_lim);
    if(res != 0){
        printf("Setrlimit(): error!\n");
        return -2;
    }

    //printf("new soft limit: %ld, hard limit: %ld\n", new_lim.rlim_cur, new_lim.rlim_max);

    return 1;
}
