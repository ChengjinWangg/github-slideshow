 #include "common.h" 
//#include <gf_rand.h>  //需要添加到当前工作目录
#include "jerasure.h"
#include "reed_sol.h"
#include "cauchy.h"
#include "liberation.h"
#include "timing.h"

typedef struct _NodeMeta  //节点sockfd信息管理
{
  int nodeNum;  //节点编号
  int sockfd;  
  int accessCount;  //节点时间窗口的访问频次
  int diskAccessCount;  //节点访问本地硬盘的次数
  int memAccessCount; //内存的访问次数

  //int tempNodeAccessCount;
  int preNodeAccessCount;
  int TWaccessCount;
  //int tempDiskAccessCount;
  int preDiskAccessCount;
  int TWdiskAccessCount;
 // int tempMemAccessCount;
  int preMemAccessCount;
  int TWmemAccessCount;
  int useTime;
//  int memSet_indexByMemLocation;
}NodeMeta;



typedef struct _NodeMetaLayout  // 布局更新的通信节点
{
  int nodeNum;  //节点编号
  int sockfd;  
  //int accessCount;  //节点时间窗口的访问频次
}NodeMetaLayout;


typedef struct _SortingBlockMetas{ //待排序的数据块相关信息  内存冗余组的节点排序  以及 内存等待队列中的节点进行排序
  long blockID;  //块ID
  long freq;//一个时间窗口内该数据块的访问频次
  int hot_level;//新的热度等级
  int flag;
  int nodeNum;
  int position;
  int memLocation;
  long RG_ID;
 // int tempMetasFreq; //从开始时刻到当前  可以不用这个 但是还没有删掉，删掉的话需要改代码，具体见nodeNum
 // int preMetasFreq; //从开始时刻到当前的前一段
}SortingBlockMetas;

node *nodes;   //全局节点信息
NodeMeta *nodeMetas; //全局节点的sockfd信息 
NodeMetaLayout *nodeMetasLayouts;  //布局更新时候sockfd信息
BlockMeta *blockMetas;   //全局数据分块的信息
//BlockMeta *waitsets;  //逻辑所有等待队列
//SortingBlockMetas *sortingBlockWaits;
//BlockMeta *HL0_Sets;  //HL=0 的队列集合
SortingBlockMetas *HL0_Sets;//全局的
//ParityMeta *parityMetas;  //全局校验节点信息
RGroup *rGroups;   //冗余组
int waitIndex; //等待队列索引
int HL0Index=0; //HL=0的集合的索引
int isConstructingStripe=1; //条带初始化是否完成 1 代表初始化完成 0 代表初始化还没有完成
int coordinator2client_sockfd;
//struct timeval stime;   //用户相应开始计时
int layout_count=0;  //数据布局计数器
int i;
int system_status=0;
//double responseTime=0.0;
//FILE *responseTime_file=NULL;
FILE *layout_time_file=NULL;
int Stripe_Num_Index=0;
int  addCodeIndex=0;
double layoutTime=0.0;

//
int coodinator_init();
void quick_sort(SortingBlockMetas* bm,int i,int j);
//void quick_sort_metas(BlockMeta* bm,int i,int j);
void quick_sort_metas_reduce(SortingBlockMetas* bm,int i,int j);
void client2coordinator_recv(Client2CoordinatorRequest *request);
void coordinator2client_send(Client2CoordinatorResponse *response);
void node2coordinator_recv(Node2CoordiantorRequest *request,int nodeNum);
void node2CoordiantorStripeRequest_recv(Node2CoordinatorConstructingStripesRequest *request,int sockfd);
void coordinator2nodeLayoutUpdateRequest_send(Coordinator2NodeLayoutUpdateRequest *request);
void node2coordinatorBlock_recv(Block *block,int nodeNum);
void coordinator_destroy();
int coodiantor_constructing_stripes();
int metadata_manage();
void layout_update();
void transformation(RGroup *rGroup,int RG_ID,SortingBlockMetas *sortingBlockMetasInRG,int memBlockNum);
int isNumber(double d);
//


/*
   节点初始化
*/
int coodinator_init(){
    printf("coodinator  init  begin..............\n");
    if(set_server_limit() <0) {
      	printf("set_server_limit error*****************\n");
		return ERR;
    } 
   nodes = read_config(CONFIG);

   blockMetas=(BlockMeta *)malloc(sizeof(BlockMeta)*Max_Block_Num); //数据分块元数据  Max_Block_Num = 5000
  // parityMetas=(ParityMeta *)malloc(sizeof(ParityMeta)*Max_Parity_Num);  //校验分块
   rGroups=(RGroup *)malloc(sizeof(RGroup)*Stripe_Num*2);  //Stripe_Num = 139
   long h;
   for(h=0;h<Max_Block_Num;h++){
    blockMetas[h].blockID=h;//BlockID
    blockMetas[h].freq=0;//现在访问频度
    blockMetas[h].preFreq=0;//上一段时间的访问频度
    blockMetas[h].hot_level=0;//初始为冷数据
    blockMetas[h].RG_ID=-1;//-1表示不在任何冗余组
  //  blockMetas[h].isParity=0;//默认是数据分块
    blockMetas[h].flag=2;//默认在磁盘中
    blockMetas[h].position=-1;//不在冗余组中
    blockMetas[h].memLocation=-1;//不在内存中
    blockMetas[h].nodeNum=-1;
    //blockMetas[h].accessTime=0; //访问时间
   // printf("blockMetas[%ld].blockID=%ld,freq=%ld,hot_level=%d,RG_ID=%ld,isParity=%d,flag=%d,position=%d,memLocation=%ld,nodeNum=%d\n",h,blockMetas[h].blockID,blockMetas[h].freq,blockMetas[h].hot_level, blockMetas[h].RG_ID,blockMetas[h].isParity,blockMetas[h].flag, blockMetas[h].position,blockMetas[h].memLocation,blockMetas[h].nodeNum);
   }

/*   for(h=0;h<Max_Parity_Num;h++){
    parityMetas[h].blockID=h;
    parityMetas[h].RG_ID=-1;
    parityMetas[h].position=-1;
    parityMetas[h].memLocation=-1;
    parityMetas[h].nodeNum=-1;
    parityMetas[h].createTime=0;
   }*/

    //初始化
    int r1;
    int r2;
    for(r1=1;r1<=Stripe_Num;r1++){
      for(r2=0;r2<K;r2++){
        rGroups[r1].duplications[r2][0].blockID=-1;
        rGroups[r1].duplications[r2][0].RG_ID=-1;
        rGroups[r1].duplications[r2][0].nodeNum=-1;
        rGroups[r1].duplications[r2][0].memLocation=-1;
        rGroups[r1].duplications[r2][0].position=-1;
        rGroups[r1].duplications[r2][0].hot_level=-1;
      }
    }

   //初始化
   int p1;
   int p2;
   for(p1=1;p1<=Stripe_Num;p1++){
    for(p2=0;p2<R;p2++){
      rGroups[p1].parityMetas[p2].blockID=-1;
      rGroups[p1].parityMetas[p2].RG_ID=-1;
      rGroups[p1].parityMetas[p2].nodeNum=-1;
      rGroups[p1].parityMetas[p2].memLocation=-1;
      rGroups[p1].parityMetas[p2].position=-1;
    }
   }



    nodeMetas = (NodeMeta *)malloc(sizeof(NodeMeta)*(NodeNumTotal-2));
    nodeMetasLayouts=(NodeMetaLayout*)malloc(sizeof(NodeMetaLayout)*(NodeNumTotal-2));

    int i;//计数器
    for ( i = 0; i < NodeNumTotal-2; i++)  //依次注册data节点
      {
        int coodinator2node_sockfd;
        int coodinator2nodeLayout_sockfd;
        if((coodinator2node_sockfd = server_accept(Coordinator2NodePort+i,Max_Length_backlog))!=-1){//元服务器与数据服务器建立连接
          printf("coodinator accept node[%d] successed\n",i);
          nodeMetas[i].nodeNum=i;
          nodeMetas[i].sockfd=coodinator2node_sockfd;

            if((coodinator2nodeLayout_sockfd=server_accept(Coordinator2NodeLayoutPort+i,Max_Length_backlog))!=-1){//更新数据布局时协调器和服务器端的通信端口
            printf("Layout_________coodinator accept node[%d] successed\n",i);
               nodeMetasLayouts[i].nodeNum=i;
               nodeMetasLayouts[i].sockfd=coodinator2nodeLayout_sockfd;
           }else{
            printf("Layout_________coordinator accept node[%d] is error\n",i);
           }
       
        }else{
          printf("coodinator accept node[%d] is error\n",i);
          return ERR;
        }
      }
     
     //接收客户端的连接
    if((coordinator2client_sockfd = server_accept(Client2CoordinatorPort,Max_Length_backlog))!=-1){  
      printf("coordinator accept client successed\n"); 
     } else{
      printf("coordinator accept client is error\n");
      return ERR;  
     } 

     //初始化节点的负载
     int nodeIndex;
     for(nodeIndex=0;nodeIndex<(NodeNumTotal-2);nodeIndex++){
        nodeMetas[nodeIndex].accessCount=0;
        nodeMetas[nodeIndex].preNodeAccessCount=0;
        nodeMetas[nodeIndex].TWaccessCount=0;
        nodeMetas[nodeIndex].diskAccessCount=0;
        nodeMetas[nodeIndex].preDiskAccessCount=0;
        nodeMetas[nodeIndex].TWdiskAccessCount=0;
        nodeMetas[nodeIndex].memAccessCount=0;
        nodeMetas[nodeIndex].preMemAccessCount=0;
        nodeMetas[nodeIndex].TWmemAccessCount=0;
        nodeMetas[nodeIndex].useTime=0;
      //  nodeMetas[nodeIndex].memSet_indexByMemLocation=-1; //节点的内存负载位置
     }
    

     coodiantor_constructing_stripes(); //构造条带
     return 0;   
}

//对数据分块进行快速排序,一共num个数据分块  快速排序 升序排序
void quick_sort(SortingBlockMetas* bm,int i,int j){
//printf("quick_sort()=======i=%d j=%d\n",i,j );
int start;
int end;
long freq;//key
SortingBlockMetas temp;
if(i<j){
  start=i;
  end=j;
  freq=bm[start].freq;
  temp=bm[start];
  //printf("freq===\n",freq);

while(start<end){
  while(start<end&&bm[end].freq>freq)end--;
  if(start<end) bm[start++]=bm[end];
  while(start<end&&bm[start].freq<freq)start++;
  if(start<end)bm[end--]=bm[start];
}
//bm[start].freq=freq;
  bm[start]=temp;
quick_sort(bm,i,start-1);
quick_sort(bm,start+1,j);
}
}
void quick_sort_metas_reduce(SortingBlockMetas* bm,int i,int j){  //元数据排序
int start;
int end;
long freq;//key
 SortingBlockMetas temp;
if(i<j){
  start=i;
  end=j;
  freq=bm[start].freq;
  temp=bm[i];
while(start<end){
  while(start<end&&bm[end].freq<=freq)end--;
  if(start<end) bm[start++]=bm[end];
  while(start<end&&bm[start].freq>=freq)start++;
  if(start<end)bm[end--]=bm[start];
}
 //bm[start].freq=freq;
 bm[start]=temp;
quick_sort_metas_reduce(bm,i,start-1);
quick_sort_metas_reduce(bm,start+1,j);
}
}



void client2coordinator_recv(Client2CoordinatorRequest *request){  //接收来自客户端的消息
  //printf("client2coordinator_recv=====================\n" );
  size_t needRecv=sizeof(Client2CoordinatorRequest);
  char *recvBuf=(char *)malloc(needRecv);   
  recv_bytes(coordinator2client_sockfd,recvBuf,needRecv); //接收来自client的数据信息
  memcpy(request,recvBuf,needRecv);  //反序列化
}


void coordinator2client_send(Client2CoordinatorResponse *response){  //协调器向客户端发送数据分块信息
  size_t needSend = sizeof(Client2CoordinatorResponse);
  char *sendBuf=(char *)malloc(needSend);
  memcpy(sendBuf,response,needSend);
  send_bytes(coordinator2client_sockfd,sendBuf,needSend);
}

void node2coordinator_recv(Node2CoordiantorRequest *request,int nodeNum){  //协调器接收服务器发送的消息
  size_t needRecv =sizeof(Node2CoordiantorRequest);
  char *recvBuf=(char *)malloc(needRecv);
  recv_bytes(nodeMetas[nodeNum].sockfd,recvBuf,needRecv);
  memcpy(request,recvBuf,needRecv);    //反序列化
}

void node2CoordiantorStripeRequest_recv(Node2CoordinatorConstructingStripesRequest *request,int sockfd){ //构建条带
  size_t needRecv=sizeof(Node2CoordinatorConstructingStripesRequest);
  char *recvBuf=(char *)malloc(needRecv);
  recv_bytes(sockfd,recvBuf,needRecv);
  memcpy(request,recvBuf,needRecv);
}
void node2CoordiantorStripeRequestFirst_send(Node2CoordinatorConstructingStripesRequestFirst *request,int nodeNum){  //协调器向服务器发送消息
  size_t needSend = sizeof(Node2CoordinatorConstructingStripesRequestFirst);
  char *sendBuf=(char *)malloc(needSend);
  memcpy(sendBuf,request,needSend);
  send_bytes(nodeMetas[nodeNum].sockfd,sendBuf,needSend);
}

void coordinator2nodeLayoutUpdateRequest_send(Coordinator2NodeLayoutUpdateRequest *request){  //布局更新
  size_t needSend=sizeof(Coordinator2NodeLayoutUpdateRequest);
  char *sendBuf=(char *)malloc(needSend);
  memcpy(sendBuf,request,needSend);  //序列化
  int m;// 计数
  for(m=0;m<NodeNumTotal-2;m++){
    send_bytes(nodeMetasLayouts[m].sockfd,sendBuf,needSend);   //向所有的服务器发送请求？？
  }
}


void node2coordinatorLayoutUpdateRequest_recv(Node2CoordiantorLayoutUpdateResponse *request,int nodeNum){
   size_t needRecv=sizeof(Node2CoordiantorLayoutUpdateResponse);
   char *recvBuf=(char *)malloc(needRecv);
   recv_bytes(nodeMetasLayouts[nodeNum].sockfd,recvBuf,needRecv);
   memcpy(request,recvBuf,needRecv);
}


void node2coordinatorBlock_recv(Block *block,int nodeNum){  //接收数据信息，然后更改相关的元数据信息
  size_t needRecv=sizeof(Block);
  char *recvBuf=(char *)malloc(needRecv);
  recv_bytes(nodeMetasLayouts[nodeNum].sockfd,recvBuf,needRecv);
  memcpy(block,recvBuf,needRecv); //反序列化
}

void coordinator_destroy(){//coordinator清理工作
  if(nodes!=NULL){
    free(nodes);
  }
  if(nodeMetas!=NULL){
    for(i=0;i<NodeNumTotal-2;i++){
      if(nodeMetas[i].sockfd!=-1){
        close(nodeMetas[i].sockfd);
      }
    }
    free(nodeMetas);
  }
  if(nodeMetasLayouts!=NULL){
    for(i=0;i<NodeNumTotal-2;i++){
      if(nodeMetasLayouts[i].sockfd!=-1){
        close(nodeMetasLayouts[i].sockfd);
      }
    }
    free(nodeMetasLayouts);
  }
  if(coordinator2client_sockfd!=-1){
    close(coordinator2client_sockfd);
  }
 if(blockMetas!=NULL){
    free(blockMetas);
  }
 /* if(parityMetas!=NULL){
    free(parityMetas);
  }*/
  if(rGroups!=NULL){
    free(rGroups);
  }
/* if(sortingBlockMetas!=NULL){
  free(sortingBlockWaits);
 }*/



}

/*
   初始化条带  接收数据分块，然后进行计算，得到校验分块,然后发送校验分块到校验节点
*/
int coodiantor_constructing_stripes(){
  printf("coodiantor_constructing_stripes  start**************************\n");
 Stripe_Num_Index=(int)((double)Stripe_Num*(double)(K+R+0.1*K))/(NodeNumTotal-2)+(((int)((double)Stripe_Num*(double)(K+R+0.1*K))%(NodeNumTotal-2))!=0 ? 1:0);
 printf("Stripe_Num_Index=%d RGroupCount=%d\n",Stripe_Num_Index,(int)((double)Stripe_Num*(double)(K+R+0.1*K)));
// Stripe_Num_Index = 102
  
  int blockID;
  int RIndex=0;
  int KIndex=0;
  int flag=0;
  int a;  //条带的索引
  int b; //节点的索引
  for(a=0;a<Stripe_Num_Index;a++){
      for(b=0;b<NodeNumTotal-2;b++){
        //发送 
        Node2CoordinatorConstructingStripesRequestFirst *request_first=(Node2CoordinatorConstructingStripesRequestFirst *)malloc(sizeof(Node2CoordinatorConstructingStripesRequestFirst));
          if(KIndex%K==0&&KIndex!=0&&flag==0){ //轮到校验分块了
              //printf("a\n");
              //printf("RIndex=%d\n",);
              blockID=RIndex;
            //  printf("blockID=%d\n",blockID);
              request_first->isParity=1;
            // printf("b\n");
             if(RIndex%R==0){ //KIndex 保持不变 
              //printf("c\n");
                RIndex++;
              //  printf("d\n");
              }else{
               // printf("e\n");
                RIndex++;
                flag=1;
              }
          }else{
            flag=0;
            blockID=KIndex;
            request_first->isParity=0;
            KIndex++;
          }
          request_first->blockID=blockID;
          printf("blockID=%d,isParity=%d\n",request_first->blockID,request_first->isParity);
          node2CoordiantorStripeRequestFirst_send(request_first,b);
          printf("hahahahhahahhahhahhhhhhhh\n");
         //接收
         Node2CoordinatorConstructingStripesRequest *request_temp=(Node2CoordinatorConstructingStripesRequest *)malloc(sizeof(Node2CoordinatorConstructingStripesRequest));
         node2CoordiantorStripeRequest_recv(request_temp,nodeMetas[b].sockfd);  //节点号0-3
         printf("coodinator has recved data block from nodeNum=%d  and RG_ID =%ld \n",request_temp->nodeNum,request_temp->RG_ID);
         //更新记录节点内存的使用信息
       //  nodeMetas[request_temp->nodeNum].memSet_indexByMemLocation=request_temp->memLocation;
         //更新元数据的信息 以及 冗余组信息
         if(request_temp->isParity==0&&request_temp->blockID<Stripe_Num*K&&request_temp->blockID<Max_Block_Num){
            blockMetas[request_temp->blockID].RG_ID=request_temp->RG_ID;
            blockMetas[request_temp->blockID].blockID=request_temp->blockID;
            blockMetas[request_temp->blockID].RG_ID=request_temp->RG_ID;
            blockMetas[request_temp->blockID].isParity=request_temp->isParity;
            blockMetas[request_temp->blockID].flag=request_temp->flag;
            blockMetas[request_temp->blockID].position=request_temp->position;
            blockMetas[request_temp->blockID].memLocation=request_temp->memLocation;
            blockMetas[request_temp->blockID].nodeNum=request_temp->nodeNum;
            blockMetas[request_temp->blockID].hot_level=request_temp->hot_level;
             //冗余组信息
            int d;
           for(d=0;d<Max_Duplications;d++){  //直接修改了Max_Duplications
            if(d==0){
               rGroups[request_temp->RG_ID].duplications[request_temp->position][d].blockID=blockMetas[request_temp->blockID].blockID;
               rGroups[request_temp->RG_ID].duplications[request_temp->position][d].RG_ID=blockMetas[request_temp->blockID].RG_ID;
               rGroups[request_temp->RG_ID].duplications[request_temp->position][d].nodeNum=blockMetas[request_temp->blockID].nodeNum;
               rGroups[request_temp->RG_ID].duplications[request_temp->position][d].memLocation=blockMetas[request_temp->blockID].memLocation;
               rGroups[request_temp->RG_ID].duplications[request_temp->position][d].position=blockMetas[request_temp->blockID].position;
               rGroups[request_temp->RG_ID].duplications[request_temp->position][d].hot_level=blockMetas[request_temp->blockID].hot_level;
               printf("rGroups[%d].duplications[%d][%d].blockID=%d\n",request_temp->RG_ID,request_temp->position,d, rGroups[request_temp->RG_ID].duplications[request_temp->position][d].blockID);
             }else{//刚开始还没有副本
              rGroups[request_temp->RG_ID].duplications[request_temp->position][d].blockID=-1;
              rGroups[request_temp->RG_ID].duplications[request_temp->position][d].RG_ID=-1;
              rGroups[request_temp->RG_ID].duplications[request_temp->position][d].nodeNum=-1;
              rGroups[request_temp->RG_ID].duplications[request_temp->position][d].memLocation=-1;
              rGroups[request_temp->RG_ID].duplications[request_temp->position][d].position=-1;
              rGroups[request_temp->RG_ID].duplications[request_temp->position][d].hot_level=-1;

            }
         }
          }

        //更新校验分块的信息 以及 冗余组信息
         if(request_temp->isParity==1&&request_temp->blockID<Stripe_Num*R){  //更新冗余组
          rGroups[request_temp->RG_ID].parityMetas[(request_first->blockID)%R].blockID=request_temp->blockID;
          rGroups[request_temp->RG_ID].parityMetas[(request_first->blockID)%R].RG_ID=request_temp->RG_ID;
          rGroups[request_temp->RG_ID].parityMetas[(request_first->blockID)%R].nodeNum=request_temp->nodeNum;
          rGroups[request_temp->RG_ID].parityMetas[(request_first->blockID)%R].memLocation=request_temp->memLocation;
          rGroups[request_temp->RG_ID].parityMetas[(request_first->blockID)%R].position=request_temp->position;
          printf("nodeNum=%d \n", rGroups[request_temp->RG_ID].parityMetas[(request_first->blockID)%R].nodeNum);
         }
     }
  
  
     //发送构造条带的消息
     int m; //索引
     char c='y';
     for(m=0;m<NodeNumTotal-2;m++){  
        send_bytes(nodeMetas[m].sockfd,&c,1);
        printf("coodiantor_constructing_stripes =====coordinator send feedback to data node that can start next stripe\n");
      }
   }
 

     //测试输出
    int mn;
    int n;
    int d;
    for(mn=1;mn<=Stripe_Num;mn++){
      for(n=0;n<K;n++){
        for(d=0;d<Max_Duplications;d++){
        printf("rGroups[%d].duplications[%d][%d].blockID=%d and nodeNum=%d\n",mn,n,d,rGroups[mn].duplications[n][d].blockID,rGroups[mn].duplications[n][d].nodeNum); 
        }
      }
    }



     int csw;
     int dsw;
     int fsw;
      for(dsw=1;dsw<=Stripe_Num;dsw++){
         for(csw=0;csw<R;csw++){
   
         printf("rGroups[%d].parityMetas[%d].nodeNum=%d,rGroups[%d].parityMetas[%d].blockID=%d\n",dsw,csw,rGroups[dsw].parityMetas[csw].nodeNum,dsw, csw,rGroups[dsw].parityMetas[csw].blockID);
     
       }
      }

     /*char c='y'; 
     send_bytes(coordinator2client_sockfd,&c,1);  //向客户端播放通知，条带初始化结束，开始播放trace
     printf("node_constructing_stripes=========coordinator send request to client that stripes has finished ,and client can paly traces\n");
     printf("=====================初始化测试输出===============================\n");
   */
  
  return 0;
}
int metadata_manage(){
  printf("*************************metadata_manage() begin****************\n");
  long blockID;
  int nodeNum;
  long requestID;
  long replaceBlockIDfromWaitSet;
  int isPlayingTraces;
  int isDuplication=0; //代表不是副本数据
//  printf("metadata_manage()======1\n");
  Client2CoordinatorRequest *c2c_request=(Client2CoordinatorRequest *)malloc(sizeof(Client2CoordinatorRequest));
 // printf("metadata_manage()======2\n" );
  client2coordinator_recv(c2c_request);  //接收到客户端的信息
//  printf("c2c_request->requestID=%ld\n",c2c_request->requestID);
//  printf("metadata_manage()======coordinator recv request from client \n");
  isPlayingTraces=c2c_request->isPlayingTraces;
  if(isPlayingTraces==0){
    printf("metadata_manage()======client closed and coodinator starting destory\n");
    return ERR;
  }

   blockID=c2c_request->blockID;
   nodeNum=c2c_request->nodeNum;
   requestID=c2c_request->requestID;

   BlockMeta * blockMeta=(BlockMeta *)malloc(sizeof(BlockMeta));
   blockMeta=&blockMetas[blockID];  //blockID对应的元数据信息
//   printf("metadata_manage()======blockMeta.blockID=%ld,freq=%ld,hot_level=%d,RG_ID=%ld,isParity=%d,flag=%d,position=%d,memLocation=%ld,nodeNum=%d\n",blockMeta->blockID,blockMeta->freq,blockMeta->hot_level,blockMeta->RG_ID,blockMeta->isParity,blockMeta->flag,blockMeta->position,blockMeta->memLocation,blockMeta->nodeNum );
   Client2CoordinatorResponse *c2c_response=(Client2CoordinatorResponse *)malloc(sizeof(Client2CoordinatorResponse));
   c2c_response->blockID=blockID;
   c2c_response->requestID=requestID;

   if(blockMeta->flag==2){//block 在磁盘中              、、、、、、、、、、在磁盘中为什么要更新nodeNum
 //   printf("metadata_manage()======the block is in disk\n");
    blockMeta->nodeNum=nodeNum; //更新元数据的nodeNum  
    c2c_response->memLocation=-1; 
    c2c_response->nodeNum=blockMeta->nodeNum;
   }else if(blockMeta->flag==1){  //在内存队列中
  //  printf("metadata_manage()======the block is in mem waitsets\n");
    c2c_response->memLocation=blockMeta->memLocation;  
    c2c_response->nodeNum=blockMeta->nodeNum;
   }else{  //在内存冗余组中
      if(blockMeta->hot_level==2){  //说明有副本 选择节点负载最小的nodeNum
    //    printf("metadata_manage()======the block has duplications\n");
        int m;
        int minIndex=0;
        long minLoad=nodeMetas[rGroups[blockMeta->RG_ID].duplications[blockMeta->position][0].nodeNum].accessCount; //源数据分块
        for(m=1;m<Max_Duplications;m++){
          long tempLoad=nodeMetas[rGroups[blockMeta->RG_ID].duplications[blockMeta->position][m].nodeNum].accessCount;
          if(tempLoad<minLoad&&rGroups[blockMeta->RG_ID].duplications[blockMeta->position][m].blockID!=-1){
             minLoad=tempLoad;
             minIndex=m;
          }
        }
       //判断是不是副本数据
       if(minIndex!=0){  //是副本数据
         isDuplication=1; //是副本数据
       }
      c2c_response->nodeNum=rGroups[blockMeta->RG_ID].duplications[blockMeta->position][minIndex].nodeNum;  //选择节点负载最小的节点
      c2c_response->memLocation=rGroups[blockMeta->RG_ID].duplications[blockMeta->position][minIndex].memLocation;//副本所在位置  blockMeta->position
    //  printf("c2c_response->nodeNum=%d c2c_response->isDuplication=%d \n",c2c_response->nodeNum,c2c_response->isDuplication);
      } else{  //在冗余组中不存在副本
     //  printf("metadata_manage()======the block has not duplications\n");
       c2c_response->nodeNum=blockMeta->nodeNum;
       c2c_response->memLocation=blockMeta->memLocation;
      } 
   }
   c2c_response->isDuplication=isDuplication;
   c2c_response->flag=blockMeta->flag;
   c2c_response->freq=blockMeta->freq;
   c2c_response->isConstructingStripe=isConstructingStripe;/// 初始化条带是否完成
   coordinator2client_send(c2c_response);   //返回客户端信息
   printf("metadata_manage()======coordinator send response to client\n");

   Node2CoordiantorRequest *n2c_request =(Node2CoordiantorRequest *)malloc(sizeof(Node2CoordiantorRequest));
   node2coordinator_recv(n2c_request,c2c_response->nodeNum);//接收来自数据服务器的响应，响应后更新数据分块的访问频次
   printf("metadata_manage()======coodinator recv response from node\n");
   printf("n2c_request->blockID=%ld,n2c_request->nodeNum=%d,n2c_request->flag=%d,n2c_request->freq=%ld,n2c_request->memLocation=%ld,n2c_request->isFullWaitSets=%d  \n",n2c_request->blockID,n2c_request->nodeNum,n2c_request->flag,n2c_request->freq,n2c_request->memLocation,n2c_request->isFullWaitSets);
    
/*   //如果访问的是副本的数据 那么元数据根本不需要更新。 更新的只是节点的访问频次  相关的数据信息根本不用更新
  if(n2c_request->isDuplication==1){ //如果是副本的话，只需要更新相关如下 ，其实diskAccessCount也不用更新
   blockMetas[n2c_request->blockID].freq=n2c_request->freq;  //还需要更新该BlockID的访问频次
   nodeMetas[n2c_request->nodeNum].accessCount=n2c_request->accessCount;  //节点的访问频次
   nodeMetas[n2c_request->nodeNum].diskAccessCount=n2c_request->diskAccessCount;  //磁盘访问频次 没有命中
   nodeMetas[n2c_request->nodeNum].memAccessCount=n2c_request->memAccessCount;  //节点某个内存的访问次数
  }else{  //其实内存中的也不用更新，主要更新的就是访问到硬盘的
   blockMetas[n2c_request->blockID].blockID=n2c_request->blockID;
   blockMetas[n2c_request->blockID].freq=n2c_request->freq;
   blockMetas[n2c_request->blockID].flag=n2c_request->flag;  //默认在磁盘中
   blockMetas[n2c_request->blockID].memLocation=n2c_request->memLocation;  
   blockMetas[n2c_request->blockID].nodeNum=n2c_request->nodeNum;
   blockMetas[n2c_request->blockID].accessTime=n2c_request->accessTime;
   nodeMetas[n2c_request->nodeNum].accessCount=n2c_request->accessCount;  //节点的访问频次
   nodeMetas[n2c_request->nodeNum].diskAccessCount=n2c_request->diskAccessCount;  //磁盘访问频次 没有命中
   nodeMetas[n2c_request->nodeNum].memAccessCount=n2c_request->memAccessCount;  //节点某个内存的访问次数
  }

   //处理等待队列满的情况
   if(n2c_request->isFullWaitSets==1){
      recv_bytes(nodeMetas[c2c_response->nodeNum].sockfd,(char *)&replaceBlockIDfromWaitSet,sizeof(long));
      printf("metadata_manager()=======coordinator recv replaceBlockIDfromWaitSet  from  server[%d]\n",c2c_response->nodeNum );
      //初始化
      blockMetas[replaceBlockIDfromWaitSet].blockID=replaceBlockIDfromWaitSet;//BlockID
     // blockMetas[replaceBlockIDfromWaitSet].blockID=-1;//BlockID
      blockMetas[replaceBlockIDfromWaitSet].freq=0;
      blockMetas[replaceBlockIDfromWaitSet].preFreq=0;
      blockMetas[replaceBlockIDfromWaitSet].hot_level=0;//初始为冷数据
      blockMetas[replaceBlockIDfromWaitSet].RG_ID=-1;//-1表示不在任何冗余组
      blockMetas[replaceBlockIDfromWaitSet].flag=2;//默认在磁盘中
      blockMetas[replaceBlockIDfromWaitSet].position=-1;//不在冗余组中
      blockMetas[replaceBlockIDfromWaitSet].memLocation=-1;//不在内存中
      blockMetas[replaceBlockIDfromWaitSet].nodeNum=-1;
      blockMetas[replaceBlockIDfromWaitSet].accessTime=0;
   //  printf("hahhahahahhahahahhahahah\n");
    }*/

   if(n2c_request->isFullWaitSets==1){  //不用进行元数据的更新，只需要更新相应的节点即可。
       nodeMetas[n2c_request->nodeNum].accessCount=n2c_request->accessCount;  //节点的访问频次
       nodeMetas[n2c_request->nodeNum].diskAccessCount=n2c_request->diskAccessCount;  //磁盘访问频次 没有命中
       nodeMetas[n2c_request->nodeNum].memAccessCount=n2c_request->memAccessCount;  //节点某个内存的访问次数
  /* 
      blockMetas[n2c_request->blockID].blockID=n2c_request->blockID;//BlockID
     // blockMetas[replaceBlockIDfromWaitSet].blockID=-1;//BlockID
      blockMetas[n2c_request->blockID].freq=0;
      blockMetas[n2c_request->blockID].preFreq=0;
      blockMetas[n2c_request->blockID].hot_level=0;//初始为冷数据
      blockMetas[n2c_request->blockID].RG_ID=-1;//-1表示不在任何冗余组
      blockMetas[n2c_request->blockID].flag=2;//默认在磁盘中
      blockMetas[n2c_request->blockID].position=-1;//不在冗余组中
      blockMetas[rn2c_request->blockID].memLocation=-1;//不在内存中
      blockMetas[n2c_request->blockID].nodeNum=-1;
      blockMetas[n2c_request->blockID].accessTime=0;*/
   //  printf("hahhahahahhahahahhahahah\n");
    }else{

       if(n2c_request->isDuplication==1){ //如果是副本的话，只需要更新相关如下 ，其实diskAccessCount也不用更新
          blockMetas[n2c_request->blockID].freq=n2c_request->freq;  //还需要更新该BlockID的访问频次
          nodeMetas[n2c_request->nodeNum].accessCount=n2c_request->accessCount;  //节点的访问频次
          nodeMetas[n2c_request->nodeNum].diskAccessCount=n2c_request->diskAccessCount;  //磁盘访问频次 没有命中
          nodeMetas[n2c_request->nodeNum].memAccessCount=n2c_request->memAccessCount;  //节点某个内存的访问次数
         }else{  //其实内存中的也不用更新，主要更新的就是访问到硬盘的
          blockMetas[n2c_request->blockID].blockID=n2c_request->blockID;
          blockMetas[n2c_request->blockID].freq=n2c_request->freq;
          blockMetas[n2c_request->blockID].flag=n2c_request->flag;  //默认在磁盘中
          blockMetas[n2c_request->blockID].memLocation=n2c_request->memLocation;  
          blockMetas[n2c_request->blockID].nodeNum=n2c_request->nodeNum;
       //   blockMetas[n2c_request->blockID].accessTime=n2c_request->accessTime;
          nodeMetas[n2c_request->nodeNum].accessCount=n2c_request->accessCount;  //节点的访问频次
          nodeMetas[n2c_request->nodeNum].diskAccessCount=n2c_request->diskAccessCount;  //磁盘访问频次 没有命中
          nodeMetas[n2c_request->nodeNum].memAccessCount=n2c_request->memAccessCount;  //节点某个内存的访问次数
         }
   }  
    //responseTime=n2c_request->responseTime;
   // printf("responseTime=%f\n",responseTime);
   // fprintf(responseTime_file, "%lf\r\n",responseTime );
   
   long sendRequestID = n2c_request->requestID;
   send_bytes(coordinator2client_sockfd,(char *)&sendRequestID,sizeof(long));
   printf("metadata_manage()======coodinator send requestID=%ld to client\n",sendRequestID);
/*
  printf("========================metadata_manage()========================\n");
  long h;
  for(h=0;h<Max_Block_Num;h++){
  printf("blockMetas[%ld].blockID=%ld,freq=%ld,hot_level=%d,RG_ID=%ld,isParity=%d,flag=%d,position=%d,memLocation=%ld,nodeNum=%d\n",h,blockMetas[h].blockID,blockMetas[h].freq,blockMetas[h].hot_level, blockMetas[h].RG_ID,blockMetas[h].isParity,blockMetas[h].flag, blockMetas[h].position,blockMetas[h].memLocation,blockMetas[h].nodeNum);
  }
  printf("==================================================================================\n");
  */
   return 0; 
}


void layout_update(){  //时间窗口到来，启动布局更新
printf("***********************layout_update()***************************\n");
int RGroupCount=0; //冗余组中内存的个数
layout_count=1;  //数据布局计数器
pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_mutex_init(&mutex,NULL);
pthread_cond_init(&cond,NULL);
//struct timespec *ts; //绝对时间
struct timespec ts;
struct timeval stime,stime1,etime; //开始数据布局
gettimeofday(&stime,NULL);


 //int tempAccessCount=0; //所有的访问频次
 //int tempDiskAccessCount=0;  //所有的访问频次
 //int tempMemAccessCount=0;
 //int allWaitBlockNumberLogic=0; //逻辑上所有的等待的个数
 double hitRatio; //命中率
 double repalceRatio; //替换比率
 double arriveDiskRatio; //硬盘到达率
  

 //int preRGMetasFreq=0; 
 //int tempRGMetasFreq=0;
 int allRGMetasFreq=0;//所有的冗余组的访问频率初始化为0
 //int preWaitsFreq=0;
// int tempWaitsFreq=0;
 int allWaitsFreq=0; //所有的等待队列的访问频度初始化为0
 int sortCount;  //排序的索引
 printf("hahaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
 FILE *Hot_Ratio_file=NULL;
 FILE *Repalce_Ratio_file=NULL;
 FILE *RGroupCount_file=NULL;
 FILE *hot_count_file=NULL;
 FILE *cold_count_file=NULL;
 FILE *pdrSetIndex_file=NULL;







while(1){
   if(system_status==1){
    printf("layout_update()====exit\n");
    // pthread_exit(NULL);
    return ERR;
   }
  //ts->tv_sec=stime.tv_sec+TimeWindow*layout_count;
  ts.tv_sec=stime.tv_sec;
  ts.tv_nsec=stime.tv_usec*1000;
  ts.tv_sec +=TimeWindow*layout_count;//每过一个时间窗口就开始布局更新
  printf("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n");
  pthread_cond_timedwait(&cond,&mutex,&ts);  //每隔TW时间间隔启动一次
  gettimeofday(&stime1,NULL);
 // usleep(5000000);
  printf("cccccccccccccccccccccccccccccccccccccc\n");
  //开始布局更新的时候，就要统计内存里边（包括冗余组和队列的所有数据分块的访问频次  这样比较合理
   //一个SortingBlockMetas 代表是冗余组的   一个SortingBlockWaits 代表是等待队列中的
   
 int allWaitBlockNumberLogic=0;
 int nodeAccessCount;        //一个节点时间间隔节点的访问频次
 int nodeDiskAccessCount;    //一个节点时间间隔硬盘的访问频次
 int nodeMemAccessCount;     //一个节点时间间隔里边内存的访问频次
 int allAccessCount=0;       //所有节点时间间隔里边 
 int allDiskAccessCount=0;   //所有节点时间间隔里边
 int allMemAccessCount=0;     //所有节点时间间隔里边

   SortingBlockMetas *sortingBlockWaits=(SortingBlockMetas *)malloc(sizeof(SortingBlockMetas)*Max_Wait_Length*(NodeNumTotal-2));//12个节点中的等待队列中的数据块
  int mwl;
  //初始化
  for(mwl=0;mwl<Max_Wait_Length*(NodeNumTotal-2);mwl++){
    sortingBlockWaits[mwl].blockID=-1;
    sortingBlockWaits[mwl].flag=-1;  //初始化
    sortingBlockWaits[mwl].freq=-1;
    sortingBlockWaits[mwl].nodeNum=-1;
    sortingBlockWaits[mwl].memLocation=-1;
    sortingBlockWaits[mwl].position=-1;
    sortingBlockWaits[mwl].RG_ID=-1;
    sortingBlockWaits[mwl].hot_level=-1;
  }
  //开始布局更新操作 ① 取得元数据中的冗余组中的所有数据块（不包括复制的数据块） 此处空间多分了一倍
  SortingBlockMetas *sortingBlockMetasInRG=(SortingBlockMetas *)malloc(sizeof(SortingBlockMetas)*2*K*Stripe_Num);
  //初始化 
  int bmi;
  for(bmi=0;bmi<2*K*Stripe_Num;bmi++){
    sortingBlockMetasInRG[bmi].blockID=-1;
    sortingBlockMetasInRG[bmi].flag=-1;
    sortingBlockMetasInRG[bmi].freq=-1;
    sortingBlockMetasInRG[bmi].nodeNum=-1;
    sortingBlockMetasInRG[bmi].memLocation=-1;
    sortingBlockMetasInRG[bmi].position=-1;
    sortingBlockMetasInRG[bmi].RG_ID=-1;
    sortingBlockMetasInRG[bmi].hot_level=-1;
  }


 // int i;
  int j=0;
  int m=0;
   for(i=0;i<Max_Block_Num;i++){
    if(blockMetas[i].flag==0){  //在冗余组中
       // printf("i====%d\n",i);
        sortingBlockMetasInRG[j].blockID=blockMetas[i].blockID;
        allRGMetasFreq=blockMetas[i].freq-blockMetas[i].preFreq;
        blockMetas[i].preFreq=blockMetas[i].freq;
        sortingBlockMetasInRG[j].freq=allRGMetasFreq;
        sortingBlockMetasInRG[j].flag=0;
        sortingBlockMetasInRG[j].nodeNum=blockMetas[i].nodeNum;
        sortingBlockMetasInRG[j].memLocation=blockMetas[i].memLocation;
        sortingBlockMetasInRG[j].position=blockMetas[i].position;
        sortingBlockMetasInRG[j].RG_ID=blockMetas[i].RG_ID;
        sortingBlockMetasInRG[j].hot_level=blockMetas[i].hot_level;  //初始值默认为1；
        j++;
    }
    if(blockMetas[i].flag==1){  //在队列中
       sortingBlockWaits[m].blockID=blockMetas[i].blockID;
       allWaitsFreq=blockMetas[i].freq-blockMetas[i].preFreq;
       blockMetas[i].preFreq=blockMetas[i].freq;
       sortingBlockWaits[m].freq=allWaitsFreq;
       sortingBlockWaits[m].flag=1;
       sortingBlockWaits[m].nodeNum=blockMetas[i].nodeNum;
       sortingBlockWaits[m].memLocation=blockMetas[i].memLocation;
       sortingBlockWaits[m].position=blockMetas[i].position;
       sortingBlockWaits[m].RG_ID=blockMetas[i].RG_ID;
       sortingBlockWaits[m].hot_level=1;  //初始值默认为1；
       m++;
       allWaitBlockNumberLogic++;
    }
  }

    printf("layout_update()====allWaitBlockNumberLogic==%d\n", allWaitBlockNumberLogic );
    //打印冗余组中的相关信息
    int af;
    for(af=0;af<j;af++){
     printf("sortingBlockMetasInRG[%d]  blockID=%ld freq=%d hot_level=%d flag=%d\n",af,sortingBlockMetasInRG[af].blockID, sortingBlockMetasInRG[af].freq,sortingBlockMetasInRG[af].hot_level,sortingBlockMetasInRG[af].flag);
    }
   
    int cw;
    for(cw=0;cw<allWaitBlockNumberLogic;cw++){
      printf("sortingBlockWaits[%d] blockID =%ld freq=%d hot_level=%ld flag=%d\n",cw,sortingBlockWaits[cw].blockID,sortingBlockWaits[cw].freq,sortingBlockWaits[cw].hot_level,sortingBlockWaits[cw].flag );
    }

   // printf("layout_update()====j=%d\n",j );
    sortCount=j-1;//冗余组中排序的块个数
    printf("layout_update()====sortCount=%d\n",sortCount);
    printf("ddddddddddddddddddddddddddddddddddddddddddddddddd\n");
    //②对元数据中的冗余组中数据块进行排序
    quick_sort(sortingBlockMetasInRG,0,sortCount);
   // quick_sort(sortingBlockMetasInRG,0,j-1);  //按照频率升序排序
   printf("fffffffffffffffffffffffffffffffffffffffffffffffff\n");
   //根据j求出内存中冗余组的数据块的个数 （主要是针对flag=0 说明内存中有副本   RGroupCount
     int sbm;
     int copyCount=0;;
     for(sbm=0;sbm<j;sbm++){
        if(sortingBlockMetasInRG[sbm].hot_level==2){
          copyCount++;
        }
     }
     
     RGroupCount=(K+R)*Stripe_Num+copyCount; //冗余组的个数 暂且不包括副本个数吧  差别不是很大

    // RGroupCount=j+copyCount;  //冗余组的个数
     printf("layout_update()====j=%d\n",j );
     printf("RGroupCount=%d\n",RGroupCount);
     RGroupCount_file=fopen("A_RGroupCount.txt","a+");
     fprintf(RGroupCount_file, "%d\r\n",RGroupCount);
     fclose(RGroupCount_file);

     //求出TW阶段，所有节点的访问频次，即该时间段内节点的访问频次。 （节点0-9的）
       int ad;
      for(ad=0;ad<NodeNumTotal-2;ad++){
         printf("layout_update()====nodeMetas[%d].preNodeAccessCount=%d\n",ad,nodeMetas[ad].preNodeAccessCount);
         nodeAccessCount=nodeMetas[ad].accessCount-nodeMetas[ad].preNodeAccessCount;
         nodeMetas[ad].TWaccessCount=nodeAccessCount;
         nodeMetas[ad].preNodeAccessCount=nodeMetas[ad].accessCount;
         printf("layout_update()====nodeMetas[%d].tempNodeAccessCount=%d \n",ad,nodeMetas[ad].accessCount);
         allAccessCount+=nodeAccessCount;

         printf("layout_update()====nodeMetas[%d].preDiskAccessCount=%d\n",ad,nodeMetas[ad].preDiskAccessCount);
         nodeDiskAccessCount=nodeMetas[ad].diskAccessCount-nodeMetas[ad].preDiskAccessCount;
         nodeMetas[ad].TWdiskAccessCount=nodeDiskAccessCount;
         nodeMetas[ad].preDiskAccessCount=nodeMetas[ad].diskAccessCount;
         printf("layout_update()====nodeMetas[%d].tempDiskAccessCount=%d \n",ad,nodeMetas[ad].diskAccessCount);
         allDiskAccessCount+=nodeDiskAccessCount;

         printf("layout_update()====nodeMetas[%d].preMemAccessCount=%d\n",ad,nodeMetas[ad].preMemAccessCount);
         nodeMemAccessCount=nodeMetas[ad].memAccessCount-nodeMetas[ad].preMemAccessCount;
         nodeMetas[ad].TWmemAccessCount=nodeMemAccessCount;
         nodeMetas[ad].preMemAccessCount=nodeMetas[ad].memAccessCount;
         printf("layout_update()====nodeMetas[%d].tempMemAccessCount=%d \n",ad,nodeMetas[ad].memAccessCount);
         allMemAccessCount+=nodeMemAccessCount;
      }

      printf("allAccessCount=%d allDiskAccessCount=%d allMemAccessCount=%d \n",allAccessCount,allDiskAccessCount, allMemAccessCount);
      
      if(j==1000){//为什么j = 1000 ，命中率就为1？
          hitRatio=1;
       }else{
        hitRatio=(double)allMemAccessCount/(double)allAccessCount; //命中率
       } 
      
      // hitRatio=(double)allMemAccessCount/(double)allAccessCount; //命中率
       Hot_Ratio_file=fopen("A_Hit_Ratio.txt","a+");
       fprintf(Hot_Ratio_file, "%lf\r\n",hitRatio );
       fclose(Hot_Ratio_file);
       printf("layout_update()======hitRatio=%f\r\n",hitRatio );

    //硬盘的到达率 arriveDiskRatio  也不能使到达率 算是到达的个数吧
      arriveDiskRatio = (1-hitRatio)*ArriveRatio; //ArriveRatio一秒钟请求到达的个数
      printf("layout_update()======arriveDiskRatio=%f\r\n",arriveDiskRatio );
      double d1,d2;
      int flagRatio=0;
      const double d = 0.0001;
      d1=0.0;
      d2=arriveDiskRatio;
     if(d1 - d2 > -d && d1 - d2 < d){//arriveDiskRatio<0.0001，则说明效果好
       flagRatio=1;
       printf("相等\n");
      }else{
        flagRatio=0;
        printf("不相等\n");
      }


     //判断等待队长
     if(allDiskAccessCount>Max_Wait_Length*(NodeNumTotal-2)){
        allDiskAccessCount=Max_Wait_Length*(NodeNumTotal-2);   //等待队长的最大值
     }

     if(allDiskAccessCount==0||flagRatio==1){//不需要访问磁盘
       repalceRatio=0.0;
     }else{//计算替换率      ？？？？？？？？？？？？？？？？？？？？？？
     repalceRatio=((double)(allDiskAccessCount+1)*(double)Bandwith*(double)arriveDiskRatio*(double)(TimeWindow*1000))/((double)(2*(double)(RGroupCount))*((double)allDiskAccessCount*(double)Bandwith-arriveDiskRatio*(double)allDiskAccessCount*(double)Sblk-arriveDiskRatio*(double)Sblk));
     }
     
   //  repalceRatio=((double)(allDiskAccessCount+1)*(double)Bandwith*(double)arriveDiskRatio*(double)(TimeWindow*1000))/((double)(2*(double)(RGroupCount))*((double)allDiskAccessCount*(double)Bandwith-arriveDiskRatio*(double)allDiskAccessCount*(double)Sblk-arriveDiskRatio*(double)Sblk));
     Repalce_Ratio_file=fopen("A_Repalce_Ratio.txt","a+");
     int a= fprintf(Repalce_Ratio_file, "%lf\r\n",repalceRatio );
     fclose(Repalce_Ratio_file);
     
     printf("layout_update()======repalceRatio=%f  and a=%d\n",repalceRatio,a);
    
   //  int isnumber=isNumber(repalceRatio);
     //printf("isNumber=%d\n",isNumber);
 /*    if(isNumber){
       if(repalceRatio>=1){
       continue;  //终止此次循环
      }
     }else{
      continue;
     }
*/

  /*    if(repalceRatio>=1){
       continue;  //终止此次循环
     }
*/
/*
     if(repalceRatio<1&&repalceRatio>0){
         
     }else{
      continue;
     }
    */
    //③ 前hot_ration的比例的新的hot_level=2
    //④ cold_ration 比例的hot_level=0
    //int hot_count=(int)((K+R)*Stripe_Num*Hot_Ratio);   //sortingBlockMetas中前hot_count个数据块为热数据
   int hot_count=(int)(K*Stripe_Num*Hot_Ratio);   //sortingBlockMetas中前hot_count个数据块为热数据
  //  int hot_count=(int)(RGroupCount*Hot_Ratio);   //sortingBlockMetas中前hot_count个数据块为热数据
    printf("layout_update()====hot_count=%d\n",hot_count);

     hot_count_file=fopen("A_hot_count.txt","a+");
     fprintf(hot_count_file, "%d\r\n",hot_count);
     fclose(hot_count_file);


    int h;
    for(h=0;h<hot_count;h++){
       sortingBlockMetasInRG[j-1-h].hot_level=2;//后hot_count个数据块为热数据块
    }
      
    int cold_count=(int)(RGroupCount*repalceRatio);//冷块的数量，根据替换率来计算
    printf("layout_update()====cold_count=%d\n",cold_count);

     cold_count_file=fopen("A_cold_count.txt","a+");
     fprintf(cold_count_file, "%d\r\n",cold_count);
     fclose(cold_count_file);



    int c;
    for(c=0;c<cold_count;c++){
      sortingBlockMetasInRG[c].hot_level=0;
    }
     
    printf("layout_update()====中间数据=%d \n",j- hot_count-cold_count);
    int h1;
    for(h1=cold_count;h1<(j- hot_count);h1++){   //温数据块
      sortingBlockMetasInRG[h1].hot_level=1;
    }
     
    //开始进行数据布局的变换    对于替换统一进行替换
    HL0_Sets=(SortingBlockMetas *)malloc(sizeof(SortingBlockMetas)*cold_count);

    //int HL0Index=0;
    //打印调试
    int sbmi;
    for(sbmi=0;sbmi<j;sbmi++){
      printf("sortingBlockMetasInRG[%d].hot_level=%d  sortingBlockMetasInRG[%d].blockID=%ld\n",sbmi,sortingBlockMetasInRG[sbmi].hot_level,sbmi,sortingBlockMetasInRG[sbmi].blockID);
    }


    //测试输出
    int mn;
    int n;
    for(mn=1;mn<=Stripe_Num;mn++){
    	for(n=0;n<K;n++){
    		printf("rGroups[%d].duplications[%d][0].blockID=%d\n",mn,n,rGroups[mn].duplications[n][0].blockID);
    	}
    }


    int s;
    for(s=1;s<=Stripe_Num;s++){
    // printf("s==============%d\n",s);
   /* int aa;
    if(addCodeIndex%3==0){
      for(aa=0;aa<NodeNumTotal-2;aa++){
           nodeMetas[aa].useTime=0;
      }
    }*/

     transformation(&rGroups[s],s,sortingBlockMetasInRG,j);//更新每条冗余组
    }

    
、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、、
   

    //开始统计HL-0集合
    int replaceNum=(int)(RGroupCount*repalceRatio);  //j需要改变
    printf("layout_update()====replaceNum=%d\n",replaceNum);

    //归并的数组
    SortingBlockMetas *mergeSets=(SortingBlockMetas*)malloc(sizeof(SortingBlockMetas)*(HL0Index+allWaitBlockNumberLogic));
    int mergeSetIndex=0;
    int t;
    for(t=0;t<HL0Index;t++){
      mergeSets[mergeSetIndex]=HL0_Sets[t];
      mergeSetIndex++;
    }
    printf("layout_update()==== mergeSetIndex1=%d\n",mergeSetIndex);
    int w;
    for(w=0;w<allWaitBlockNumberLogic;w++){  // sortingBlockWaits
      //mergeSets[mergeSetIndex]=waitsets[w];
      mergeSets[mergeSetIndex]=sortingBlockWaits[w];
      mergeSetIndex++;
    }
    printf("layout_update()====mergeSetIndex=%d\n",mergeSetIndex);

      //输出测试
    int msi;
    for(msi=0;msi<mergeSetIndex;msi++){
      printf("mergeSets[%d].hot_level=%d  mergeSets[%d].blockID=%ld mergeSets[%d].freq=%d mergeSets[%d].flag=%d\n",msi,mergeSets[msi].hot_level,msi,mergeSets[msi].blockID,msi,mergeSets[msi].freq,msi,mergeSets[msi].flag);
    }

    quick_sort_metas_reduce(mergeSets,0,mergeSetIndex-1); //降序排序
    
    printf("======================降序排序之后==========================\n");
    //输出测试
    for(msi=0;msi<mergeSetIndex;msi++){
      printf("mergeSets[%d].hot_level=%d  mergeSets[%d].blockID=%ld mergeSets[%d].freq=%d mergeSets[%d].flag=%d\n",msi,mergeSets[msi].hot_level,msi,mergeSets[msi].blockID,msi,mergeSets[msi].freq,msi,mergeSets[msi].flag);
    }

    //对于归并的数组找出pdr
    SortingBlockMetas *pdrSets=(SortingBlockMetas *)malloc(sizeof(SortingBlockMetas)*replaceNum);
    int u;
    int pdrSetIndex=0;
    for(u=0;u<replaceNum;u++){
      if(mergeSets[u].flag==1){
        pdrSets[pdrSetIndex]=mergeSets[u];
        pdrSetIndex++;
      }
    }
    printf("layout_update()====pdrSetIndex=%d\n",pdrSetIndex);
    pdrSetIndex_file=fopen("A_pdrSetIndex_count.txt","a+");
    fprintf(pdrSetIndex_file, "%d\r\n",pdrSetIndex);
    fclose(pdrSetIndex_file);

    //输出测试
    for(msi=0;msi<pdrSetIndex;msi++){
      printf("pdrSets[%d].blockID=%d,pdrSets[%d].nodeNum=%d\n",msi,pdrSets[msi].blockID,msi,pdrSets[msi].nodeNum);
    }

    //对于归并的数组找出tgt
    SortingBlockMetas *tgtSets=(SortingBlockMetas *)malloc(sizeof(SortingBlockMetas)*pdrSetIndex);

    int v;
    int tgtSetIndex=0;
    for(v=replaceNum;v<mergeSetIndex;v++){
      if(mergeSets[v].flag==0){
        tgtSets[tgtSetIndex]=mergeSets[v];
        tgtSetIndex++;
      }
    }
    printf("layout_update()====tgtSetIndex=%d\n",tgtSetIndex);
    //输出测试
    for(msi=0;msi<tgtSetIndex;msi++){
      printf("tgtSets[%d].blockID=%d,tgtSets[%d].nodeNum=%d\n",msi,tgtSets[msi].blockID,msi,tgtSets[msi].nodeNum);
    }
    //进行相关的替换  这个步骤主要是本地局部性
    int x;
    int y;
    int commonIndex=0;
    for(x=0;x<pdrSetIndex;x++){
       for(y=0;y<tgtSetIndex;y++){  //给falg一个标志信息，当flag=0 冗余组。flag=1，队列中。 flag=2，磁盘中。 flag=3 完成替换工作
          if(pdrSets[x].nodeNum==tgtSets[y].nodeNum&&pdrSets[x].flag!=3&&tgtSets[y].flag!=3){
            //处理本地局部性。      
            Coordinator2NodeLayoutUpdateRequest *c2nLayoutUpdate_Request=(Coordinator2NodeLayoutUpdateRequest *)malloc(sizeof(Coordinator2NodeLayoutUpdateRequest));
            c2nLayoutUpdate_Request->code=3;
            c2nLayoutUpdate_Request->targetNodeNum=tgtSets[y].nodeNum;  //被替换的节点
            c2nLayoutUpdate_Request->targetMemLocation=tgtSets[y].memLocation; //被替换节点的内存位置
            c2nLayoutUpdate_Request->sourceNodeNum=pdrSets[x].nodeNum;
            c2nLayoutUpdate_Request->blockID=pdrSets[x].blockID;
            c2nLayoutUpdate_Request->sourceMemlocation=pdrSets[x].memLocation;
            coordinator2nodeLayoutUpdateRequest_send(c2nLayoutUpdate_Request);  //发送请求
            printf("coodinator has send request to node and code=3  &&&local \n");
            //接收差值的部分
            char *deltaBuf=(char *)malloc(sizeof(char)*Block_Size);
            recv_bytes(nodeMetasLayouts[c2nLayoutUpdate_Request->targetNodeNum].sockfd,deltaBuf,Block_Size*sizeof(char));
            printf("coordinator has recved deltaBuf from node &&&local  \n");
            //更新旧的元数据
            //blockMetas[tgtSets[y].blockID].freq=0;
            blockMetas[tgtSets[y].blockID].flag=2;
            blockMetas[tgtSets[y].blockID].hot_level=0;
            int RG_ID_temp=blockMetas[tgtSets[y].blockID].RG_ID;
            blockMetas[tgtSets[y].blockID].RG_ID=-1;
            blockMetas[tgtSets[y].blockID].isParity=0;
            blockMetas[tgtSets[y].blockID].freq=0;
            blockMetas[tgtSets[y].blockID].preFreq=0;
            int position_temp=blockMetas[tgtSets[y].blockID].position;
            blockMetas[tgtSets[y].blockID].position=-1;
            int memLocation_temp=blockMetas[tgtSets[y].blockID].memLocation;
            blockMetas[tgtSets[y].blockID].memLocation=-1;
            int nodeNum_temp=blockMetas[tgtSets[y].blockID].nodeNum;
            blockMetas[tgtSets[y].blockID].nodeNum=-1;
       //      blockMetas[tgtSets[y].blockID].accessTime=0;
            
            //更新新的元数据
            blockMetas[pdrSets[x].blockID].hot_level=1;  //新数据块的热度等级为1
            blockMetas[pdrSets[x].blockID].RG_ID=RG_ID_temp;
            blockMetas[pdrSets[x].blockID].isParity=0;
            blockMetas[pdrSets[x].blockID].flag=0;
            blockMetas[pdrSets[x].blockID].position=position_temp;
            blockMetas[pdrSets[x].blockID].memLocation=memLocation_temp;
            blockMetas[pdrSets[x].blockID].nodeNum=nodeNum_temp;
           
            //更新冗余组
            rGroups[RG_ID_temp].duplications[position_temp][0]=blockMetas[pdrSets[x].blockID]; 
            printf("=====================本地局部性测试输出===============================\n");
            //测试输出
            int mn;
            int n;
            for(mn=1;mn<=Stripe_Num;mn++){
            for(n=0;n<K;n++){
            printf("rGroups[%d].duplications[%d][0].blockID=%d\n",mn,n,rGroups[mn].duplications[n][0].blockID);
             }
            } 
           
            //校验更新 把deltaBuf发送到校验节点对应的位置
            int rp;
            for(rp=0;rp<R;rp++){
            Coordinator2NodeLayoutUpdateRequest *c2nLayoutUpdate_Request=(Coordinator2NodeLayoutUpdateRequest *)malloc(sizeof(Coordinator2NodeLayoutUpdateRequest));
            c2nLayoutUpdate_Request->code =4;
            c2nLayoutUpdate_Request->targetNodeNum=rGroups[RG_ID_temp].parityMetas[rp].nodeNum;
            c2nLayoutUpdate_Request->targetMemLocation=rGroups[RG_ID_temp].parityMetas[rp].memLocation;
            c2nLayoutUpdate_Request->sourceNodeNum=-1;
            c2nLayoutUpdate_Request->sourceMemlocation=-1;
            c2nLayoutUpdate_Request->blockID=-1;

            coordinator2nodeLayoutUpdateRequest_send(c2nLayoutUpdate_Request);
            printf("layout_count======coodinator send request to node  and code=4  &&&local \n");
            send_bytes(nodeMetasLayouts[c2nLayoutUpdate_Request->targetNodeNum].sockfd,deltaBuf,Block_Size*sizeof(char));
            printf("layout_update()====coordinator send deltaBuf to node &&&local \n");
            //收到校验信息完成反馈
            char *feedback=(char *)malloc(sizeof(char));
            recv_bytes(nodeMetasLayouts[c2nLayoutUpdate_Request->targetNodeNum].sockfd,feedback,1);
            printf("layout_update()====coordinator has recved feedback from node &&&local\n");
            }

            pdrSets[x].flag=3;
            tgtSets[y].flag=3;
            commonIndex++;
            break;  //跳出当前循环        
          }
       }  
    }

   //对剩余的数据块进行操作，主要是体现负载均衡
  int  commonIndexRest=pdrSetIndex-commonIndex;
  printf("layout_update()====commonIndex=%d,commonIndexRest=%d\n",commonIndex,commonIndexRest);
  SortingBlockMetas *pdrSetsRest=(SortingBlockMetas *)malloc(sizeof(SortingBlockMetas)*commonIndexRest);
  SortingBlockMetas *tgtSetsRest=(SortingBlockMetas *)malloc(sizeof(SortingBlockMetas)*commonIndexRest);
  int pdrSetRestIndex=0;
  int tgtSetRestIndex=0;
   int d5;
  int e;
  for(d5=0;d5<pdrSetIndex;d5++){
    if(pdrSets[d5].flag!=3){
      pdrSetsRest[pdrSetRestIndex]=pdrSets[d5];
      pdrSetRestIndex++;
    }
  }
  printf("layout_update()====pdrSetRestIndex=%d\n",pdrSetRestIndex);
  //输出测试
   for(msi=0;msi<pdrSetRestIndex;msi++){
      printf("pdrSetsRest[%d].blockID=%d,pdrSetsRest[%d].nodeNum=%d\n",msi,pdrSetsRest[msi].blockID,msi,pdrSetsRest[msi].nodeNum);
    }
  for(e=0;e<tgtSetIndex;e++){
    if(tgtSets[e].flag!=3){
      tgtSetsRest[tgtSetRestIndex]=tgtSets[e];
      tgtSetRestIndex++;
    }
  }
  printf("layout_update()====tgtSetRestIndex=%d\n",tgtSetRestIndex);
  //输出测试
   for(msi=0;msi<tgtSetRestIndex;msi++){
      printf("tgtSetsRest[%d].blockID=%d,tgtSetsRest[%d].nodeNum=%d\n",msi,tgtSetsRest[msi].blockID,msi,tgtSetsRest[msi].nodeNum);
    }
   //针对剩余的 直接把替换   把负载最大的节点尽可能发送到负载最小的节点 （可以进一步完善，提高整体的性能） 可以找一下候选节点
    quick_sort_metas_reduce(pdrSetsRest,0,pdrSetRestIndex-1); //降序
    quick_sort(tgtSetsRest,0,tgtSetRestIndex-1);//升序 

    int f;
    for(f=0;f<commonIndexRest;f++){
       //发送请求
        Coordinator2NodeLayoutUpdateRequest *c2nLayoutUpdate_Request=(Coordinator2NodeLayoutUpdateRequest *)malloc(sizeof(Coordinator2NodeLayoutUpdateRequest));  
        c2nLayoutUpdate_Request->code=3;
        c2nLayoutUpdate_Request->targetNodeNum=tgtSetsRest[f].nodeNum;  //被替换的节点
        c2nLayoutUpdate_Request->targetMemLocation=tgtSetsRest[f].memLocation; //被替换节点的内存位置
        printf("c2nLayoutUpdate_Request->targetMemLocation=%ld \n", c2nLayoutUpdate_Request->targetMemLocation);
        c2nLayoutUpdate_Request->sourceNodeNum=pdrSetsRest[f].nodeNum;
        c2nLayoutUpdate_Request->blockID=pdrSetsRest[f].blockID;  //供给节点
        printf("c2nLayoutUpdate_Request->blockID=%d\n",c2nLayoutUpdate_Request->blockID );
        c2nLayoutUpdate_Request->sourceMemlocation=pdrSetsRest[f].memLocation;
        coordinator2nodeLayoutUpdateRequest_send(c2nLayoutUpdate_Request);  //发送请求
        printf("coodinator has send request to node and code=3 \n");

 
            //接收差值
            char *deltaBuf=(char *)malloc(sizeof(char)*Block_Size);
            recv_bytes(nodeMetasLayouts[c2nLayoutUpdate_Request->targetNodeNum].sockfd,deltaBuf,Block_Size*sizeof(char));
            printf("coordinator has recved deltaBuf from node   \n");
            //更新旧的元数据
            //blockMetas[tgtSets[y].blockID].freq=0;
            blockMetas[tgtSetsRest[f].blockID].flag=2;
            blockMetas[tgtSetsRest[f].blockID].freq=0;
            blockMetas[tgtSetsRest[f].blockID].preFreq=0;
            blockMetas[tgtSetsRest[f].blockID].hot_level=0;
            int RG_ID_temp=blockMetas[tgtSetsRest[f].blockID].RG_ID;
            blockMetas[tgtSetsRest[f].blockID].RG_ID=-1;
            blockMetas[tgtSetsRest[f].blockID].isParity=0;
            int position_temp=blockMetas[tgtSetsRest[f].blockID].position;
            blockMetas[tgtSetsRest[f].blockID].position=-1;
            int memLocation_temp=blockMetas[tgtSetsRest[f].blockID].memLocation;
            blockMetas[tgtSetsRest[f].blockID].memLocation=-1;
            int nodeNum_temp=blockMetas[tgtSetsRest[f].blockID].nodeNum;
            blockMetas[tgtSetsRest[f].blockID].nodeNum=-1;
      //      blockMetas[tgtSetsRest[f].blockID].accessTime=0;
            
            //更新新的元数据
            blockMetas[pdrSetsRest[f].blockID].hot_level=1;  //新数据块的热度等级为1
            blockMetas[pdrSetsRest[f].blockID].RG_ID=RG_ID_temp;
            blockMetas[pdrSetsRest[f].blockID].isParity=0;
            blockMetas[pdrSetsRest[f].blockID].flag=0;
            blockMetas[pdrSetsRest[f].blockID].position=position_temp;
            blockMetas[pdrSetsRest[f].blockID].memLocation=memLocation_temp;
            blockMetas[pdrSetsRest[f].blockID].nodeNum=nodeNum_temp;

/*
            //接收数据块
           Block *old_block=(Block *)malloc(sizeof(Block));
           Block *new_block=(Block *)malloc(sizeof(Block));
          
           
           node2coordinatorBlock_recv(new_block,pdrSetsRest[f].nodeNum);   //接收新的数据分块
           printf("coordinator has recved new block from node and new_block->blockID=%d\n",new_block->blockID);

           node2coordinatorBlock_recv(old_block,tgtSetsRest[f].nodeNum);   //接收旧的数据分块
           printf("coordinator has recved old block from node and old_block->blockID=%d\n",old_block->blockID);

         //  node2coordinatorBlock_recv(new_block,tgtSetsRest[f].nodeNum);   //接收新的数据分块
         //  printf("coordinator has recved new block from node and new_block->blockID=%d\n",new_block->blockID);
      

           //更新旧的元数据
           // blockMetas[old_block.blockID].freq=0;
            blockMetas[old_block->blockID].flag=2;
            blockMetas[old_block->blockID].freq=0;
            blockMetas[old_block->blockID].preFreq=0;
            blockMetas[old_block->blockID].hot_level=0;
            int RG_ID_temp=blockMetas[old_block->blockID].RG_ID;
            blockMetas[old_block->blockID].RG_ID=-1;
            blockMetas[old_block->blockID].isParity=0;
            int position_temp=blockMetas[old_block->blockID].position;
            blockMetas[old_block->blockID].position=-1;
            int memLocation_temp=blockMetas[old_block->blockID].memLocation;
            blockMetas[old_block->blockID].memLocation=-1;
            int nodeNum_temp=blockMetas[old_block->blockID].nodeNum;
            blockMetas[old_block->blockID].nodeNum=-1;
            blockMetas[old_block->blockID].accessTime=0;
            
            //更新新的元数据
            blockMetas[new_block->blockID].hot_level=1;  //新数据块的热度等级为1
            blockMetas[new_block->blockID].RG_ID=RG_ID_temp;
            blockMetas[new_block->blockID].isParity=0;
            blockMetas[new_block->blockID].flag=0;
            blockMetas[new_block->blockID].position=position_temp;
            blockMetas[new_block->blockID].memLocation=memLocation_temp;
            blockMetas[new_block->blockID].nodeNum=nodeNum_temp;*/
           
           //更新冗余组  pdrSetsRest[f].blockID
           // rGroups[RG_ID_temp].duplications[position_temp][0]=blockMetas[new_block->blockID]; 
            rGroups[RG_ID_temp].duplications[position_temp][0]=blockMetas[pdrSetsRest[f].blockID]; 
             printf("=====================负载均衡测试输出===============================\n");
            //测试输出
            int mn;
            int n;
            for(mn=1;mn<=Stripe_Num;mn++){
            for(n=0;n<K;n++){
            printf("rGroups[%d].duplications[%d][0].blockID=%d\n",mn,n,rGroups[mn].duplications[n][0].blockID);
             }
            } 
           
            //校验更新 把deltaBuf发送到校验节点对应的位置
          /*  char *deltaBuf=(char *)malloc(sizeof(char)*Block_Size);
            calc_xor(old_block->blockBuf,new_block->blockBuf,deltaBuf,Block_Size*sizeof(char));*/
            int rp;
            for(rp=0;rp<R;rp++){
            Coordinator2NodeLayoutUpdateRequest *c2nLayoutUpdate_Request=(Coordinator2NodeLayoutUpdateRequest *)malloc(sizeof(Coordinator2NodeLayoutUpdateRequest));
            c2nLayoutUpdate_Request->code =4;
            c2nLayoutUpdate_Request->targetNodeNum=rGroups[RG_ID_temp].parityMetas[rp].nodeNum;
            c2nLayoutUpdate_Request->targetMemLocation=rGroups[RG_ID_temp].parityMetas[rp].memLocation;
            c2nLayoutUpdate_Request->sourceNodeNum=-1;
            c2nLayoutUpdate_Request->blockID=-1;
            c2nLayoutUpdate_Request->sourceMemlocation=-1;
            coordinator2nodeLayoutUpdateRequest_send(c2nLayoutUpdate_Request);
            printf("layout_count======coodinator send request to node  and code=4  \n");
            send_bytes(nodeMetasLayouts[c2nLayoutUpdate_Request->targetNodeNum].sockfd,deltaBuf,Block_Size*sizeof(char));
            printf("layout_update()====coordinator send deltaBuf to node \n");
            //收到校验信息完成反馈
            char *feedback=(char *)malloc(sizeof(char));
            recv_bytes(nodeMetasLayouts[c2nLayoutUpdate_Request->targetNodeNum].sockfd,feedback,1);
            printf("layout_update()====coordinator has recved feedback from node\n");
            }

    }
        
  layout_count++;
  HL0Index=0;
  gettimeofday(&etime,NULL);
   printf(" response time is %f (s) \n",((etime.tv_sec-stime1.tv_sec)+((double)(etime.tv_usec-stime1.tv_usec)/1000000)));
   layoutTime=((etime.tv_sec-stime1.tv_sec)+((double)(etime.tv_usec-stime1.tv_usec)/1000000));
   layout_time_file=fopen("A_Layout_Time.txt","a+");
   fprintf(layout_time_file, "%lf\r\n",layoutTime );
   fclose(layout_time_file);
  printf("layout_update()======end==========================and layout_count=%d====================\n",(layout_count-1));
}



}


/**
 * [transformation description]   核心算法
 * @param  rGroups           [description]  冗余组信息
 * @param  RG_ID             [description]  冗余组ID  
 * @param  sortingBlockMetas [description]  已经升序排序的冗余组的元数据分块（不包含copy的块）
 * @param  memBlockNum       [description]  参数sortingBlockMetasInRG的总的个数  不包含数据分块的个数
 * @return                   [description]  
 */

  void  transformation(RGroup *rGroup,int RG_ID,SortingBlockMetas *sortingBlockMetasInRG,int memBlockNum){  //更新每条冗余组
    printf("****************transformation()***********************\n");
    int i;
   // int HL0Index=0;
    for(i=0;i<K;i++){
        BlockMeta *blockMeta=(BlockMeta *)malloc(sizeof(BlockMeta)); 
      //  printf("i==============%d\n",i);
        blockMeta=&(rGroup->duplications[i][0]); //block[i]的元数据
        //blockMeta=&(blockMetas[rGroup->duplications[i][0].blockID]);
        printf("rGroup->duplications[i][0].blockID=%d hot_level=%d\n",rGroup->duplications[i][0].blockID, rGroup->duplications[i][0].hot_level);
        printf("blockMetas[rGroup->duplications[i][0].blockID=%d hot_level=%d\n",blockMetas[rGroup->duplications[i][0].blockID].blockID,blockMetas[rGroup->duplications[i][0].blockID].hot_level );
        int old_level=blockMeta->hot_level;
        int new_level;
        int j;
        for(j=0;j<memBlockNum;j++){
          if(sortingBlockMetasInRG[j].blockID==blockMeta->blockID){
             new_level=sortingBlockMetasInRG[j].hot_level;
             break;
          }
        }
       printf("transformation()=====blockMeta.blockID=%ld\n",blockMeta->blockID);
       printf("transformation()=====old_level=%d new_level=%d\n",old_level,new_level);
      
        if(old_level==2&&new_level==1){  //处理热度由HL-2 --> HL-1 ,主要操作是删除数据分块的副本 (删除副本中负载最大的)
          //long maxLoad=nodeMetas[blockMeta.nodeNum].accessCount;
          printf("11111111111111111111111111111111111111111111111111111111\n");
          long maxLoad=-1;
          int maxLoadIndex=1;
          int m;
          for(m=1;m<Max_Duplications;m++){
             if(maxLoad<nodeMetas[rGroup->duplications[i][m].nodeNum].TWaccessCount&&rGroup->duplications[i][m].blockID!=-1){
              maxLoad=nodeMetas[rGroup->duplications[i][m].nodeNum].TWaccessCount;
              maxLoadIndex=m;
             }
          }

          //保存目标节点和目标位置
          int targetNodeNum_temp=rGroup->duplications[i][maxLoadIndex].nodeNum;
          int targetMemLocation_temp=rGroup->duplications[i][maxLoadIndex].memLocation;
          
          // 更新元数据的信息 (包括需要更新副本的热度)
          blockMetas[blockMeta->blockID].hot_level=new_level;  //新的等级
          rGroups[blockMeta->RG_ID].duplications[blockMeta->position][0].hot_level=new_level;

          //更新副本数据信息
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].blockID=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].nodeNum=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].RG_ID=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].memLocation=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].position=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].hot_level=-1;
          printf(" blockMetas[blockMeta->blockID].hot_level=%d\n", blockMetas[blockMeta->blockID].hot_level );
          printf("rGroups[blockMeta->RG_ID]->duplications[blockMeta->position][0].hot_level=%d\n",rGroups[blockMeta->RG_ID].duplications[blockMeta->position][0].hot_level );
          
          //向服务器发送消息 删除该副本
          Coordinator2NodeLayoutUpdateRequest *c2nLayoutUpdate_Request=(Coordinator2NodeLayoutUpdateRequest *)malloc(sizeof(Coordinator2NodeLayoutUpdateRequest));
          c2nLayoutUpdate_Request->code=1;
          c2nLayoutUpdate_Request->targetNodeNum=targetNodeNum_temp;
          c2nLayoutUpdate_Request->targetMemLocation=targetMemLocation_temp;
          c2nLayoutUpdate_Request->sourceNodeNum=-1;
          c2nLayoutUpdate_Request->sourceMemlocation=-1;
          c2nLayoutUpdate_Request->blockID=blockMeta->blockID;
          coordinator2nodeLayoutUpdateRequest_send(c2nLayoutUpdate_Request);
          printf("transformation()=== coordinator2nodeLayoutUpdateRequest_send  HL-2 ---> HL-1\n");
          
          //接收反馈
          char *feedback=(char *)malloc(sizeof(char));
          printf("transformation()===targetNodeNum_temp=%d\n",targetNodeNum_temp);
          recv_bytes(nodeMetasLayouts[targetNodeNum_temp].sockfd,feedback,1);
          printf("transformation=========coordinator has recved feedback from node\n");
          printf("transformation=========code=1=====================end\n");
        }


        if(old_level==1&&new_level==2){  //处理热度由HL-1 ---> HL-2 ，主要操作是增加副本
         printf("22222222222222222222222222222222222222222222222222222222222222\n");
        // addCodeIndex++;
         //增加副本需要找到 哪些节点可以存放副本  存放副本的节点需要在非纠删码组中  这样是为了使数据更加合理分配
        //所有节点除去冗余组中的节点  就是可供选择的目标节点 targetNode  然后从targetNode中选择一个负载最小的节点
        //targetNodes的个数最多为N-2-K-R
         // rGroups[request_temp->RG_ID].duplications[index][d]=blockMetas[request_temp->blockID]; 
          
           int m;
           int noTargetNodes[NodeNumTotal];
           int noTargetNodesIndex=0;
           for(m=0;m<K;m++){  //数据节点
              int j;
              for(j=0;j<Max_Duplications;j++){
                if(j==0){  //数据节点
                noTargetNodes[noTargetNodesIndex]=rGroup->duplications[m][j].nodeNum;
                noTargetNodesIndex++;
                printf("transformation=========nodeNum=%d\n", rGroup->duplications[m][j].nodeNum);
                }else{ //可能为副本节点  九个节点的时候 不统计副本节点了
                /*  if(rGroup->duplications[m][j].blockID!=-1){  //肯定是副本节点
                    noTargetNodes[noTargetNodesIndex]=rGroup->duplicati    ons[m][j].nodeNum;
                    noTargetNodesIndex++;
                    printf("transformation=========nodeNum=%d blockID=%d \n",rGroup->duplications[m][j].nodeNum,rGroup->duplications[m][j].blockID);
                  }*/
                }
              }
           }
           printf("transformation========noTargetNodesIndex1=%d\n",noTargetNodesIndex);
           int r;  //校验节点
           for(r=0;r<R;r++){
            printf("transformation=========rGroup->parityMetas[%d].nodeNum=%d \n",r,rGroup->parityMetas[r].nodeNum );
               noTargetNodes[noTargetNodesIndex]=rGroup->parityMetas[r].nodeNum;
               noTargetNodesIndex++;
           }
           printf("transformation========noTargetNodesIndex2=%d\n",noTargetNodesIndex);
           //输出测试
           int nto;
           for(nto=0;nto<noTargetNodesIndex;nto++){
            printf("transformation=========noTargetNodes[%d]=%d\n",nto,noTargetNodes[nto]);
           }

           int targetNodes[NodeNumTotal-2-noTargetNodesIndex];
           int targetNodeIndex=0;
           int p; //所有的节点的索引
           for(p=0;p<(NodeNumTotal-2);p++){
                int q; //非目标节点的索引
                int status=0;
                for(q=0;q<noTargetNodesIndex;q++){
                  if(p==noTargetNodes[q]){
                    status=1;  //p 肯定不是targetNode
                    break;
                }
                }
             if(status==0){
              targetNodes[targetNodeIndex]=p;
              targetNodeIndex++;
             }  
           }
           printf("transformation=========targetNodeIndex=%d\n",targetNodeIndex);
           //输出测试
           int l;
           for(l=0;l<targetNodeIndex;l++){
            printf("transformation=========TargetNodes[%d]=%d\n",l,targetNodes[l]);
           }
           
           //目标节点中寻找负载最小的节点 
         /*  long minLoad=nodeMetas[targetNodes[0]].TWaccessCount;
           int minLoadIndex=0;  //目标节点中负载最小的节点的索引
           int d;
           for(d=1;d<targetNodeIndex;d++){
            long tempLoad=nodeMetas[targetNodes[d]].TWaccessCount;
             if(minLoad>tempLoad){
              minLoad=tempLoad;
              minLoadIndex=d;
             }
           }*/

            long minLoad=nodeMetas[targetNodes[0]].accessCount;;
           int minLoadIndex=0;  //目标节点中负载最小的节点的索引
           int d;
           for(d=1;d<targetNodeIndex;d++){
            long tempLoad=nodeMetas[targetNodes[d]].accessCount;
             if(minLoad>tempLoad){
              minLoad=tempLoad;
              minLoadIndex=d;
             }
           }



         printf("transformation=========minLoadIndex=%d\n",minLoadIndex);

         int copyNode=targetNodes[minLoadIndex];  //非冗余组中节点负载最小的节点
         //统计每个节点增加的使用次数，尽量落在不同的节点上面
         nodeMetas[copyNode].useTime++;
         printf("transformation=========copyNode=%d\n",copyNode);

         //向服务器发送消息 增加该副本
          Coordinator2NodeLayoutUpdateRequest *c2nLayoutUpdate_Request=(Coordinator2NodeLayoutUpdateRequest *)malloc(sizeof(Coordinator2NodeLayoutUpdateRequest));
          c2nLayoutUpdate_Request->code=2;
          c2nLayoutUpdate_Request->targetNodeNum=copyNode;
          c2nLayoutUpdate_Request->targetMemLocation=-1;
          c2nLayoutUpdate_Request->sourceNodeNum=blockMeta->nodeNum;
          c2nLayoutUpdate_Request->sourceMemlocation=blockMeta->memLocation;
         // c2nLayoutUpdate_Request->sourceMemlocation=-1;
          c2nLayoutUpdate_Request->blockID=blockMeta->blockID;
          coordinator2nodeLayoutUpdateRequest_send(c2nLayoutUpdate_Request);
          printf("transformation()=== coordinator2nodeLayoutUpdateRequest_send  HL-1 ---> HL-2\n" );
          Node2CoordiantorLayoutUpdateResponse *response=(Node2CoordiantorLayoutUpdateResponse *)malloc(sizeof(Node2CoordiantorLayoutUpdateResponse));
          node2coordinatorLayoutUpdateRequest_recv(response,copyNode);
          int memLocation;
          memLocation=response->code2memLocation;
          //recv_bytes(nodeMetasLayouts[copyNode].sockfd,(char *)&memLocation,sizeof(memLocation)); //接收副本分块的内存位置
          printf("transformation()=== coodinator has recved memLocation from node and memLocation = %d\n",memLocation);
       
          //更新冗余组信息
          int rGIndex;
          for(rGIndex=1;rGIndex<Max_Duplications;rGIndex++){
            if(rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].blockID==-1){
              //printf("hahhahahhahahhahahhahhahhahh  this is update about rGroup\n");
              rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].blockID=blockMeta->blockID;
              printf("rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].blockID=%d\n",rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].blockID );
              rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].RG_ID=blockMeta->RG_ID;
              printf("rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].RG_ID=%d\n",rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].RG_ID );
              rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].nodeNum=copyNode;
              printf("rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].nodeNum=%d\n",rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].nodeNum);
              rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].memLocation=memLocation;
              printf("rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].memLocation=%d\n",rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].memLocation );
              rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].position=blockMeta->position+(K+R);   
              printf("rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].position=%d\n",rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].position );
              rGroups[blockMeta->RG_ID].duplications[blockMeta->position][rGIndex].hot_level=new_level;
             // break;
            }
          }
          
           //更新热度（元数据的热度，以及副本的热度）
           rGroups[blockMeta->RG_ID].duplications[blockMeta->position][0].hot_level=new_level;
           printf("rGroups->duplications[blockMeta->position][0].hot_level=%d\n", rGroups[blockMeta->RG_ID].duplications[blockMeta->position][0].hot_level);
           blockMetas[blockMeta->blockID].hot_level=new_level;   //更新数据分块元信息
           printf("blockMetas[blockMeta->blockID].hot_level=%d\n", blockMetas[blockMeta->blockID].hot_level);
           printf("transformation=========code=2=====================end\n");
        }

        if(new_level==0){ //处理HL-2 --> HL-0 和 HL-1-->HL-0的情况  包括两种情况，一种是HL-2变为HL-O 一种是HL-1 变为HL-0
          printf("3333333333333333333333333333333333333333333333333333333\n");
       
          if(old_level==2){  //删除副本 同上 
          printf("3333333333333333=== new_level=0  and old_level=2\n");
          long maxLoad=-1;
          int maxLoadIndex=0;
          int m;
          for(m=1;m<Max_Duplications;m++){//删除负载最重的节点上的负载
             if(maxLoad<nodeMetas[rGroup->duplications[i][m].nodeNum].TWaccessCount&&rGroup->duplications[i][m].blockID!=-1){
              maxLoad=nodeMetas[rGroup->duplications[i][m].nodeNum].TWaccessCount;
              maxLoadIndex=m;
             }
          }

          //保存目标节点和目标位置
          int targetNodeNum_temp=rGroup->duplications[i][maxLoadIndex].nodeNum;
          int targetMemLocation_temp=rGroup->duplications[i][maxLoadIndex].memLocation;

          // 更新元数据的信息 (包括需要更新副本的热度)
          blockMetas[blockMeta->blockID].hot_level=new_level;  //新的等级
          rGroups[blockMeta->RG_ID].duplications[blockMeta->position][0].hot_level=new_level;

          //更新副本信息
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].blockID=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].nodeNum=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].RG_ID=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].memLocation=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].position=-1;
          rGroups[blockMeta->RG_ID].duplications[i][maxLoadIndex].hot_level=-1;
          printf("blockMetas[blockMeta->blockID].hot_level=%d\n", blockMetas[blockMeta->blockID].hot_level );
          printf("rGroups[blockMeta->RG_ID]->duplications[blockMeta->position][0].hot_level=%d\n",rGroups[blockMeta->RG_ID].duplications[blockMeta->position][0].hot_level );
          
          Coordinator2NodeLayoutUpdateRequest *c2nLayoutUpdate_Request=(Coordinator2NodeLayoutUpdateRequest *)malloc(sizeof(Coordinator2NodeLayoutUpdateRequest));
          c2nLayoutUpdate_Request->code=1;
          c2nLayoutUpdate_Request->targetNodeNum=targetNodeNum_temp;
          c2nLayoutUpdate_Request->targetMemLocation=targetMemLocation_temp;
          c2nLayoutUpdate_Request->sourceNodeNum=-1;
          c2nLayoutUpdate_Request->sourceMemlocation=-1;
          c2nLayoutUpdate_Request->blockID=blockMeta->blockID;
          coordinator2nodeLayoutUpdateRequest_send(c2nLayoutUpdate_Request);
          printf("transformation()=== coordinator2nodeLayoutUpdateRequest_send  HL-2 ---> HL-1\n");
          
          //接收反馈
          char *feedback=(char *)malloc(sizeof(char));
          printf("targetNodeNum_temp=%d\n",targetNodeNum_temp);
          recv_bytes(nodeMetasLayouts[targetNodeNum_temp].sockfd,feedback,1);
          printf("transformation=========coordinator has recved feedback from node\n");    
        }

          
       //开始统计HL-1 ==>HL-0的操作  一个时间窗口内该数据块的访问频次  集中替换
          printf("3333333333333333=== new_level=0 and old_level !=2\n");
           HL0_Sets[HL0Index].blockID=blockMeta->blockID;
          // HL0_Sets[HL0Index].freq=blockMeta->freq;
           HL0_Sets[HL0Index].flag=blockMeta->flag;
           HL0_Sets[HL0Index].nodeNum=blockMeta->nodeNum;
           HL0_Sets[HL0Index].position=blockMeta->position;
           HL0_Sets[HL0Index].memLocation=blockMeta->memLocation;
           HL0_Sets[HL0Index].RG_ID=blockMeta->RG_ID;

           //频次要是内存中的一段时间的频次  要找出对应的ID
           int hls;
           for(hls=0;hls<memBlockNum;hls++){
           if(sortingBlockMetasInRG[hls].blockID==blockMeta->blockID){//找出对用ID的访问频率，将冷数据块统一放到HL0_Sets中
              HL0_Sets[HL0Index].freq=sortingBlockMetasInRG[hls].freq;
              HL0_Sets[HL0Index].hot_level=sortingBlockMetasInRG[hls].hot_level;
              printf("HL0_Sets[HL0Index].freq=%ld  hot_level=%d\n",HL0_Sets[HL0Index].freq,HL0_Sets[HL0Index].hot_level);
              break;
            }
           }
           HL0Index++;
           printf("3333333333333333===HL0Index=%d\n",HL0Index );
          }
    } 

   //  return HL0Index;     
  }



int main(){
    printf("Max_Wait_Length=%d  Stripe_Num=%d  TimeWindow=%d\n", Max_Wait_Length,Stripe_Num,TimeWindow);
    int flag1;
    int flag2;
    flag1= coodinator_init();
    if(flag1==-1){
    	printf("coordinator init  is  error\n");
      return ERR;
    }else {
      for(i=0;i<NodeNumTotal-2;i++){
        printf("nodeMeta[%d] nodeNum=%d,sockfd=%d \n",i,nodeMetas[i].nodeNum,nodeMetas[i].sockfd );
      }
      	printf("coordinator init successed\n");


     char c='y'; 
     send_bytes(coordinator2client_sockfd,&c,1);  //向客户端播放通知，条带初始化结束，开始播放trace
     printf("node_constructing_stripes=========coordinator send request to client that stripes has finished ,and client can paly traces\n");
     printf("=====================初始化测试输出===============================\n");

      
    pthread_t layout_pid;//布局更新线程
    pthread_create(&layout_pid,NULL,layout_update,NULL);

     
    // responseTime_file=fopen("A_responseTime.txt","a+");


     

      while(1){
         int s;
       // for(s=0;s<90;s++){
         flag2=metadata_manage();
      //   }
      //   usleep(10000000);
         //flag2=metadata_manage();
         //usleep(10000000);
     //    layout_update();
         if(flag2==-1){ //用户相应结束
          // fclose(responseTime_file);

           system_status=1;
          pthread_join(layout_pid,NULL);
           usleep(2000000);
           coordinator_destroy();
           

           // pthread_exit(NULL);      
            //break;
         }
      }
      return 0;
    }
}