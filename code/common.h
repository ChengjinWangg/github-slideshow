 #ifndef _COMMON_H
#define _COMMON_H
#include <assert.h>

#include <time.h>
#include <sys/time.h>    //gettimeofday 
#include <sys/stat.h>    //定义在打开和创建文件时用到的一些符号常量
#include <unistd.h>      // linux  read  write
#include <string.h>
#include <stdio.h>       // linux input output 
#include <stdlib.h>      //standard library 
#include <stdint.h>
#include <signal.h>      
#include <unistd.h>      //linux/unix的系统调用,包含了许多 U N I X系统服务的函数原型,例如 r e a d,w r i t e和getpid函数
#include <math.h>
#include <malloc.h>
#include <fcntl.h>       //定义 open,fcntl函数原型
#include <error.h>  

#include <sys/types.h>    //off_t
#include <sys/socket.h>   //socket ,blind ,listen ,accept,...
#include <netinet/in.h>   //sockaddr_in ,struct sockaddr,...
#include <arpa/inet.h>    //inet_addr ,inet_ntoa ,htonl,...
#include <net/if.h>       // struct ifreq ,struct ifconf
#include <sys/ioctl.h>    //setsockopt
#include <pthread.h>      //pthread_create
#include <netinet/tcp.h>  //
#include <sys/resource.h>  

/*#include <gf_rand.h>
#include "jerasure.h"
#include "reed_sol.h"
#include "cauchy.h"
#include "liberation.h"
#include "timing.h"*/
#define Stripe_Num  139       //条带个数
#define TimeWindow 10     //时间窗口时间 为10s 
#define Max_Wait_Length 10      //等待队列中能容纳的个数，当超过这个个数的时候，就要进行替换
#define ShackFactor    1  //抖动因子



#define ULEN 16   // IP长度
#define K 6       //数据分块个数
#define R 2       // 校验分块个数
#define NodeNumTotal 14       //节点总个数
#define W 8         //编码字长
#define Packet_Size  16384   //packsize 
#define ERR -1    //错误标记
#define Max_Duplications   2      //最大副本数
//#define Stripe_Num   100            //条带个数

#define Block_Size  32*1024       //数据块大小是32KB
//#define Max_Block_Num 1000          //数据分块的最多个数
//#define Max_Parity_Num 500    //校验分块个数
#define Max_Block_Num 5000          //数据分块的最多个数  是请求的所有1-1000的信息
//#define Max_Parity_Num 20     //校验分块个数=R*Stripe_Num

//#define Repalce_Ratio    0.20 // 替换比例5%，冷数据
#define Hot_Ratio   0.1  //热数据比例
#define Max_Wait 8                //socke最大连接等待时间
#define SOCKMAX (1024*1024)       //socket 最大限制
#define Max_Length_backlog  10            //socket可排队的最大连接个数
#define Bandwith  100    //100 KB/ms   下边的用于计算  32/100
#define Sblk  32    
#define ArriveRatio 0.003  //到达率  平均每毫秒0.03个。然后大概10个节点，所以每个0.003 单位是毫秒
//#define BandwithBlockSize  0.32   //Block_Size/Bandwith 

#define CONFIG "config"
#define Trace_File "trace"  //trace的总记录数目
#define Max_Trace 250000       
//#define Max_Trace 20

#define Client2CoordinatorPort  40000      //client 与 coodinator通信端口
#define Clinet2NodePort     41000         //client 与 datanode 通信端口
#define Coordinator2NodePort  43000       //datanode 与 coodinator 通信端口
#define Node2NodePort       45000         //basenode 与  datanode 通信端口
#define Coordinator2NodeLayoutPort 46000  //布局更新时候协调器和服务器端的通信端口
#define SET_NONBLOCK(fd)  fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0)|O_NONBLOCK)  









typedef struct _node {    //节点信息
	int num;             //节点编号 0-12
	char ip[ULEN];       //节点IP
    char dev[ULEN];      //节点磁盘地址 /dev/sda
}node;

//typedef char * Block;

typedef struct _Block
{
	long blockID;
	//char *blockBuf;  // 实际内容
	char blockBuf[Block_Size];  //实际内容
	int isParity;  //是否是校验分块，初始化为-1；当等于0的时候，不是校验分块，当等于1的时候是校验分块
}Block;



typedef struct _BlockMeta{//数据块元数据
  long blockID;  //块ID
  long freq;//从一开始的所有
  int preFreq;//时间段之前，两者的差就等于当前一段时间的访问频次
  int hot_level;//HL-0=0 冷数据，HL-1=1温数据，HL-2=2热数据  
  long RG_ID;//数据分块所在冗余组ID
  int isParity;//是否为校验分块 isParity=0 为数据分块，否则为校验分块
  int flag;//flag=0在冗余组中，flag=1在队列中，flag=2在磁盘中
  int position;//数据分块在冗余组中的位置编号
  long memLocation;//内存地址,可能是等待队列或者内存集合索引
  int nodeNum;//节点号
 // double accessTime; //数据分块的访问时间

}BlockMeta;

typedef struct _ParityMeta
{
   long blockID;  //块id
   long RG_ID; //校验分块所在冗余组id
   int position; // 校验分块在冗余组中的位置编号
   long memLocation; //内存地址
   int nodeNum; // 节点号
   double createTime; //校验分块的创建时间

}ParityMeta;

typedef struct _Redundancy_Group
{
	long RG_ID;  //冗余组id
	BlockMeta duplications[K][Max_Duplications];  //数据分块以及副本源数据信息
    ParityMeta parityMetas[R]; //校验分块元数据信息

}RGroup;

typedef struct _Client2CoordinatorRequest       //客户端协调器通信
{
	int isPlayingTraces;//客户端状态
	long requestID;
	long blockID;
	int nodeNum;
}Client2CoordinatorRequest;

typedef struct _Client2CoordinatorResponse{    //用户请求即Trace
	long requestID;
	long blockID;  //待访问的块ID
	long memLocation;//内存地址
	int nodeNum;//节点号
	int flag;//数据分块位置
 	long freq;//该数据分块的访问频次
	int isConstructingStripe;//整个系统初始化是否完成,决定了server行为（直接加载到内存或者等待队列）
	 int isDuplication;  //是不是副本数据 元数据中只存放 本身的数据 不存放副本数据  =0 代表不是副本数据，=1 代表是副本数据	
}Client2CoordinatorResponse;

typedef struct _Client2NodeRequest         //客户端向服务器节点的请求
{
 int isPlayingTraces;//客户端状态
 long requestID;
 long blockID;  //数据分块标识
 size_t offset;//磁盘偏移
 long memLocation;//内存地址
 int nodeNum;//节点号
 long freq;//该数据分块的访问频次
 int flag;
 int isConstructingStripe;
 int isDuplication;  //是不是副本数据 元数据中只存放 本身的数据 不存放副本数据  =0 代表不是副本数据，=1 代表是副本数据
}Client2NodeRequest;

typedef struct _Node2CoordiantorRequest{   //服务器节点向协调器的请求
	long requestID;
	long blockID; //数据分块
	int nodeNum; //节点号
    int flag;//标识数据分块在节点位置
    long freq;//当前该数据分块的访问频次
    long memLocation;//该数据分块在此节点的内存地址,可能是等待队列索引或者内存队列索引
  //  double accessTime;//数据分块的访问时间
    int accessCount; //节点请求频次==节点负载
    int diskAccessCount;  //访问磁盘的次数
    int memAccessCount;  //访问内存的次数
    int isFullWaitSets;  // isFullWaitSets=0 代表没有满  isFullWaitSets=1 代表满
    int isDuplication;  //是不是副本数据 元数据中只存放 本身的数据 不存放副本数据  =0 代表不是副本数据，=1 代表是副本数据
  //  double responseTime; //响应时间

}Node2CoordiantorRequest;


typedef struct _Node2CoordinatorConstructingStripesRequestFirst
{
	long blockID;
	int isParity;
	
}Node2CoordinatorConstructingStripesRequestFirst;



typedef struct  _Node2CoordinatorConstructingStripesRequest
{
	long blockID;  //块id
	int hot_level; //HL-0=0 冷数据，HL-1=1温数据，HL-2=2热数据  
	long RG_ID; //数据分块所在冗余组ID
	int flag;  //flag=0 在冗余组中，flag=1在队列中，flag=2在磁盘中
	int position; //数据分块在冗余组中的位置编号
	long memLocation;//内存地址，可能是等待队列或者内存集合索引
	int nodeNum; //节点号
	//char *blockBuf; //实际数据分块的内容
	char blockBuf[Block_Size]; //实际数据分块的内容
	int isParity;  //isParity==0 是校验分块  isParity==1 不是校验分块
}Node2CoordinatorConstructingStripesRequest;

typedef struct _Coordinator2NodeLayoutUpdateRequest{    //布局更新的请求
	int code;        //布局更新时候 不同布局的标志
	int targetNodeNum;      //目标操作节点（需要接收的）
	long targetMemLocation;  //目标节点内存位置
	int sourceNodeNum; //源节点(code==2时，告知副本节点的源节点是哪个)
	long sourceMemlocation;//源节点内存位置
	long blockID;
	//int code2memlocation;  
	//long waitLocation;//等待队列位置
	//char blockBuf[Block_Size];//校验数据分块
}Coordinator2NodeLayoutUpdateRequest;


typedef struct _Node2CoordiantorLayoutUpdateResponse{
	int code2memLocation;  //code=2的时候，发送给coodinator位置
}Node2CoordiantorLayoutUpdateResponse;


/////////////////////////////////////////////////////////////////////////////
Node * read_config(const char *fname);     //读取配置文件信息
void get_ipaddr(char *ip,int len);         //获取IP地址
int current_node (node *nodes);            //获取当前节点编号
char *get_dev_name(node *nodes,int nid);   //返回当前节点的设备

int connect_try(char *ip,ushort port);     //client尝试连接server
int server_accept(ushort port,int backlog);  //server blind listen accept 建立连接

ssize_t read_bytes(int fd,char *buf,size_t nbytes);    //一次读文件
ssize_t write_bytes(int fd,char *buf,size_t nbytes);   //一次写文件
ssize_t send_bytes(int sockfd,char *buf,size_t nbytes);//一次性发送
ssize_t recv_bytes(int sockfd,char *buf,size_t nbytes);//一次性接收
void calc_xor(char *d1,char *d2,char *out,size_t len); //XOR模拟纠删码
int set_server_limit(void);                            //设置服务器限制
#endif
 