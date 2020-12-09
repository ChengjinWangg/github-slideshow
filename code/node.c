#include "common.h" 
typedef struct _NodeAsClient2NodeConnect  //当前节点连接其他节点的套接字  当前节点作为客户端
{
   int targetNode;  //目标服务器节点
   int sockfd;   //当前节点作为客户端套接字
}NodeAsClient2NodeConnect;

typedef struct _NodeAsServer2NodeAccept
{
   int sourceNode;   //客户端节点
   int sockfd;  //服务器端套接字
}NodeAsServer2NodeAccept;

/*typedef struct _waitSet{
  long blockID;
  //char *blockBuf;  // 实际内容
  char blockBuf[Block_Size];  //实际内容
  int  waitSet_index;
}waitSet;*/

NodeAsClient2NodeConnect *nodeAsClient2NodeConnect;   //当前节点作为客户端连接的服务器端的节点信息
NodeAsServer2NodeAccept *nodeAsServer2NodeAccept;     //当前节点作为服务器连接的客户端的节点信息

int node2coordinator_sockfd;     //数据节点到协调节点通信套接字  accept
int node2client_sockfd;          //数据节点到客户端通信套接字    accept
int node2coordinatorLayout_sockfd;  //布局更新的通信

node *nodes;  //配置文件
int nodeNum;  //当前节点编号

Block *waitSets;  //等待集合
//WaitSet *waitSets;
Block *memSets;   //内存集合
//int waitSet_index=0; //等待集合索引
int memSet_index=0; //内存集合索引
int isFullWaitSets=0;  //0代表没有满  1代表已经满了 从等待集合中取 并不判断是否有问题

int accessCount=0;  //节点访问次数
int diskAccessCount=0;  //硬盘的访问次数
int memAccessCount=0;  //内存的访问次数
int i;    ///计数器
int system_status=0;

int layIndex=0;
int Stripe_Num_Index=0;


//
int node_constructing_stripes();
int node_init();
void node_destroy();
void client2node_recv(Client2NodeRequest *request);
void node2coordinator_send(Node2CoordiantorRequest *request);
void node2coordinatorStripeRequest_send(Node2CoordinatorConstructingStripesRequest *request);
void coordinator2nodeLayoutUpdateRequest_recv(Coordinator2NodeLayoutUpdateRequest *request);
void node2node_send(Block *block,int copyNode);
void node2node_recv(Block *block,int sourceNode);
void node2coordinatorBlock_send(Block *block);
int processIORequest();
void layout_update();
//


/**
 * [server_init description]  datanode 初始化
 * @return [description]   协调器套接字   备用节点套接字   客户端接收
 */
int node_init(){
   printf("******************************datanode_init begin ................\n");
    if (set_server_limit() <0) {
      	printf("node_init()====set_server_limit error\n");
		return ERR;
    } 
    

   //34 / 11 + (34 % 11 != 0 ? 1 : 0)
    Stripe_Num_Index=(int)((double)Stripe_Num*(double)(K+R+0.1*K))/(NodeNumTotal-2)+(((int)((double)Stripe_Num*(double)(K+R+0.1*K))%(NodeNumTotal-2))!=0 ? 1:0);
    printf("Stripe_Num_Index=%d RGroupCount=%d\n",Stripe_Num_Index,(int)((double)Stripe_Num*(double)(K+R+0.1*K)));

    memSets=(Block *)malloc(sizeof(Block)*Stripe_Num_Index*2);
  //  memSets=(Block *)malloc(sizeof(Block)*Stripe_Num);
    waitSets=(Block *)malloc(sizeof(Block)*Max_Wait_Length);
     
 

    nodes = read_config(CONFIG);
    nodeNum = current_node(nodes);
    printf("node_init()====nodeNum is  %d \n",nodeNum);

    if(nodeNum==-1) {
    	printf("node_init()====current_node is  error\n");
    	return ERR;  
    }else {
        //连接协调器
        node2coordinator_sockfd = connect_try(nodes[13].ip,Coordinator2NodePort+nodeNum);
        node2coordinatorLayout_sockfd =connect_try(nodes[13].ip,Coordinator2NodeLayoutPort+nodeNum);
        if(node2coordinator_sockfd ==-1 ){
         printf("node_init()====node[%d] connect coordinator is error\n", nodeNum);
         return ERR;
        } else{
          if(node2coordinatorLayout_sockfd==-1){
            printf("node_init()====node[%d] connect coordinator LayoutPort is error\n", nodeNum);
            return ERR;
           }
         printf("node_init()====node[%d] connect coordinator successed and  node2coordinator_sockfd=%d \n",nodeNum,node2coordinator_sockfd);
         printf("node_init()====node[%d] connect coordinator successed and node2coordinatorLayout_sockfd=%d\n",nodeNum,node2coordinatorLayout_sockfd);
        

        //接收客户端的连接
        if ((node2client_sockfd=server_accept(Clinet2NodePort,Max_Length_backlog))!=-1){
          printf("node_init()====node accept client  successed\n");              
        }else{
          printf("node_init()====node accept client error\n");
          return ERR;
        }
      }
    }

    nodeAsClient2NodeConnect = (NodeAsClient2NodeConnect *) malloc(sizeof(NodeAsClient2NodeConnect)*(NodeNumTotal-3));
    nodeAsServer2NodeAccept=(NodeAsServer2NodeAccept *)malloc(sizeof(NodeAsServer2NodeAccept)*(NodeNumTotal-3));
    int connectIndex=0;//定义索引
    int acceptIndex=0;//定义索引
      
      //服务器的节点号是0-9
      int count;
     for ( count = 0; count < NodeNumTotal-2; count++)
     {  
        if (count==nodeNum)  //  当前节点作为客户端 服务器的计数器等于当前节点编号的时候 
        {
            int j; //客户端需要连接的服务器的计数器
            for ( j = 0; j < NodeNumTotal-2; j++)
            {
               if (j!=nodeNum)    //节点号不等于当前的节点号，则作为客户端
               {
                   //sleep(2);  //给服务器足够的时间监听,睡眠1秒
                   int nodeAsclientfd;
                   nodeAsclientfd=connect_try(nodes[j].ip,Node2NodePort+count);
                  // printf("node_init()=========count=%d\n", count);
                   if(nodeAsclientfd==-1){
                    printf("node_init()====Node[%d]AsConnectSockfd connect nodes[%d] is error\n",nodeNum,j);
                    return ERR;
                   } else{  //连接成功
                    printf("node_init()====Node[%d]AsConnectSockfd connect nodes[%d] successed\n",nodeNum,j);

                    nodeAsClient2NodeConnect[connectIndex].targetNode=nodes[j].num;
                    nodeAsClient2NodeConnect[connectIndex].sockfd=nodeAsclientfd;
                    connectIndex++;
                   }
               } 
            }    
        } else{ //当前节点作为服务端     服务器的计数器不等于当前节点编号的时候
              int nodeAsAcceptSockfd;
              nodeAsAcceptSockfd=server_accept(Node2NodePort+count,Max_Length_backlog);
              if(nodeAsAcceptSockfd!=-1){
                printf("node_init()====count=%d\n",count);
                printf("node_init()====node[%d]AsAcceptSockfd  accept nodes[%d] successed \n",nodeNum,count);
                nodeAsServer2NodeAccept[acceptIndex].sourceNode=count;
                nodeAsServer2NodeAccept[acceptIndex].sockfd=nodeAsAcceptSockfd;
                acceptIndex++;
              }else{
                printf("node_init()====Node[%d]AsAcceptSockfd  accept nodes[%d] is error\n",nodeNum,count);
                return ERR;
              }
        }
     }

     

      if(nodeNum<NodeNumTotal-2){
       printf("node_init()==== 节点memSets初始化\n");
        int wm;
        for(wm=0;wm<Stripe_Num_Index*2;wm++){  //初始化比较多
           memSets[wm].blockID=-1;//BlockID
           memSets[wm].isParity=-1; //副本没有校验块
        }
      }
     


    int w;
    for(w=0;w<Max_Wait_Length;w++){   //判断队列是否为满，通过遍历整个集合中是否存在为-1的blockID

      waitSets[w].blockID=-1;
     // waitSets[w].waitLocation=w;
    }

    
    if(nodeNum<NodeNumTotal-2){  //只需要校验节点和数据节点构建条带
      node_constructing_stripes();//构建条带
     }
   /*  if(nodeNum<K||nodeNum==8||nodeNum==9){  //只需要校验节点和数据节点构建条带
      node_constructing_stripes();//构建条带
     }
     */
 

      //对于副本节点 对memSets进行初始化
     /* if(nodeNum>3&&nodeNum<8){
       printf("node_init()==== 副本节点memSets初始化\n");
        int wm;
        for(wm=0;wm<Stripe_Num;wm++){
           memSets[wm].blockID=-1;//BlockID
        }
      }*/

  printf("node_init()====结束返回\n");
  return 0;
}

 void node_destroy(){
         int nd;
        for(nd=0;nd<NodeNumTotal-2-1;nd++){
          if(nodeAsClient2NodeConnect[nd].sockfd!=-1){
            close(nodeAsClient2NodeConnect[nd].sockfd);
          }
          if(nodeAsServer2NodeAccept[nd].sockfd!=-1){
            close(nodeAsServer2NodeAccept[nd].sockfd);
          }
        }
        if(node2coordinator_sockfd!=-1){
          close(node2coordinator_sockfd);
        }
        if(node2coordinatorLayout_sockfd!=-1){
          close(node2coordinatorLayout_sockfd);
        }
        if(node2client_sockfd!=-1){
          close(node2coordinator_sockfd);
        }
        if(nodeAsClient2NodeConnect!=NULL){
          free(nodeAsClient2NodeConnect);
        }
        if(nodeAsServer2NodeAccept!=NULL){
          free(nodeAsServer2NodeAccept);
        }
        if(nodes!=NULL){
          free(nodes);
        }
       if(waitSets!=NULL){
            free(waitSets);
        }
        if(memSets!=NULL){
            free(memSets);
        }
      
    }

void client2node_recv(Client2NodeRequest *request){  //node 接收来自client的请求
  size_t needRecv =sizeof(Client2NodeRequest);
  char *recvBuf=(char *)malloc(needRecv);
  recv_bytes(node2client_sockfd,recvBuf,needRecv);
  memcpy(request,recvBuf,needRecv);  //反序列化
}

void node2coordinator_send(Node2CoordiantorRequest *request){ //node 向coordinate发送消息
    size_t  needSend =sizeof(Node2CoordiantorRequest);
    char *sendBuf=(char *)malloc(needSend);
    memcpy(sendBuf,request,needSend);   //序列化
    send_bytes(node2coordinator_sockfd,sendBuf,needSend);
}

void node2coordinatorStripeRequest_send(Node2CoordinatorConstructingStripesRequest *request){  //node向coordinator发送构造条带的信息
   size_t needSend=sizeof(Node2CoordinatorConstructingStripesRequest);
   char *sendBuf=(char *)malloc(needSend);
   memcpy(sendBuf,request,needSend);
   send_bytes(node2coordinator_sockfd,sendBuf,needSend);
}

void coordinator2nodeLayoutUpdateRequest_recv(Coordinator2NodeLayoutUpdateRequest *request){  //布局更新的请求
  size_t needRecv =sizeof(Coordinator2NodeLayoutUpdateRequest);
  char *recvBuf=(char *)malloc(needRecv);
  recv_bytes(node2coordinatorLayout_sockfd,recvBuf,needRecv);
  memcpy(request,recvBuf,needRecv);
}

void node2coordinatorLayoutUpdateRequest_send(Node2CoordiantorLayoutUpdateResponse *request){ //code=2发送memelocation
  size_t needSend =sizeof(Node2CoordiantorLayoutUpdateResponse);
  char *sendBuf=(char *)malloc(needSend);
  memcpy(sendBuf,request,needSend);
  send_bytes(node2coordinatorLayout_sockfd,sendBuf,needSend);
}

void node2CoordiantorStripeRequestFirst_Recv(Node2CoordinatorConstructingStripesRequestFirst *request){  //接收对应的数据快号 以及是否是校验
   size_t needRecv=sizeof(Node2CoordinatorConstructingStripesRequestFirst);
   char *recvBuf=(char *)malloc(needRecv);  
   recv_bytes(node2coordinator_sockfd,recvBuf,needRecv);  
   memcpy(request,recvBuf,needRecv);
}


void node2node_send(Block *block,int copyNode){  //源节点向复制节点发送消息
  size_t needSend =sizeof(Block);
  char *sendBuf=(char *)malloc(needSend);
  memcpy(sendBuf,block,needSend);
  int m;
  int fd;
  for(m=0;m<NodeNumTotal-2-1;m++){
    if(nodeAsClient2NodeConnect[m].targetNode==copyNode){
      fd=nodeAsClient2NodeConnect[m].sockfd;
    }
  }    
  send_bytes(fd,sendBuf,needSend);
}

void node2node_recv(Block *block,int sourceNode){  //复制节点向源节点反馈消息
   size_t needRecv=sizeof(Block);
   char *recvBuf=(char *)malloc(needRecv);
   int m;
   int fd;
   for(m=0;m<NodeNumTotal-2-1;m++){
    if(nodeAsServer2NodeAccept[m].sourceNode==sourceNode){
      fd=nodeAsServer2NodeAccept[m].sockfd;
    }
   }
   recv_bytes(fd,recvBuf,needRecv);  
   memcpy(block,recvBuf,needRecv); ///反序列化
}

void node2coordinatorBlock_send(Block *block){  //节点向协调器发送数据更新消息  更新元数据
  size_t needSend=sizeof(Block);
  char *sendBuf=(char *)malloc(needSend);
  memcpy(sendBuf,block,needSend);
  send_bytes(node2coordinatorLayout_sockfd,sendBuf,needSend);
}

 int node_constructing_stripes(){
   

  printf("*********************node_constructing_stripes starting************************\n");

  for(i=0;i<Stripe_Num_Index;i++){  //保证所有的处理完

  Node2CoordinatorConstructingStripesRequestFirst *request_first=(Node2CoordinatorConstructingStripesRequestFirst *)malloc(sizeof(Node2CoordinatorConstructingStripesRequestFirst));
  node2CoordiantorStripeRequestFirst_Recv(request_first);
  printf("first this is NodeNum=%d  and blockID=%d  and parity=%d\n",nodeNum,request_first->blockID,request_first->isParity);
  Node2CoordinatorConstructingStripesRequest *n2ccs_request=(Node2CoordinatorConstructingStripesRequest *)malloc(sizeof(Node2CoordinatorConstructingStripesRequest));

  if(request_first->isParity==0){  //数据分块
     memSets[memSet_index].isParity=0;
     memSets[memSet_index].blockID=request_first->blockID;
     n2ccs_request->isParity=0; //不是校验块
     n2ccs_request->position=request_first->blockID%K;  //数据分块在冗余组中的位置编号0,1,2,3  快号除以K的余数
     n2ccs_request->RG_ID=request_first->blockID/K+1;  //条带编号   快号除以K的商 +1 


        char blockBuf[Block_Size];
        int t;
        for(t=0;t<Block_Size;t++){
         blockBuf[t]='a';
        }

    printf("=================blockBuf[0]=%c\n",blockBuf[0]);
    memcpy(memSets[memSet_index].blockBuf,blockBuf,sizeof(char)*Block_Size);  //对于数据节点的处理
    memcpy(n2ccs_request->blockBuf,blockBuf,sizeof(char)*Block_Size);  //对于数据节点的处理
    printf("node_constructing_stripes()===========nodeNum=%d data blockID=%ld memSet_index =%d\n",nodeNum,request_first->blockID,memSet_index); 

   }


   //校验块的处理
   if(request_first->isParity==1){  //校验分块
      memSets[memSet_index].isParity=1;
      memSets[memSet_index].blockID=request_first->blockID;
      n2ccs_request->isParity=1;
      n2ccs_request->position=request_first->blockID%R+K;
      n2ccs_request->RG_ID=request_first->blockID/R+1;
   }
     n2ccs_request->hot_level=1;
     n2ccs_request->blockID=request_first->blockID; //块编号
     n2ccs_request->flag=0; //在内存中
     n2ccs_request->memLocation=memSet_index; //内存数据分块的地址
     n2ccs_request->nodeNum=nodeNum;
     node2coordinatorStripeRequest_send(n2ccs_request); //向协调器发送请求                   
     printf("node_constructing_stripes()==========backupNode send block to coordinator\n");
     memSet_index++;

    //所有节点收到开启下一组初始化的信息
    char *feedback=(char *)malloc(sizeof(char));
    recv_bytes(node2coordinator_sockfd,feedback,1);
    printf("node_constructing_stripes()===========node has accepted info from coordinator thar can start next stripe\n");   

     
  }

   printf("========================node_constructing_stripes()=======================\n");
   long h;
   for(h=0;h<Stripe_Num_Index;h++){  //
      printf("memSets[%ld].blockID=%ld isParity=%d \n",h,memSets[h].blockID,memSets[h].isParity);
   }
   printf("===========================================================================\n");




   //开始处理多余的块
/*   int snp;
   for(snp=0;snp<Stripe_Num_Index;snp++){
       if((memSets[snp].blockID>=Stripe_Num*K&& memSets[snp].isParity==0)||(memSets[snp].blockID>=Stripe_Num*R&&memSets[snp].isParity==1)){
         memSets[snp].blockID=-1;//BlockID
         memSets[snp].isParity=-1;
        // memSets[snp].blockBuf='\0';
       }
    }*/

   
   int snp;
   for(snp=0;snp<Stripe_Num_Index;snp++){
      if(memSets[snp].isParity==0){  //处理数据块
           if(memSets[snp].blockID>=Stripe_Num*K||memSets[snp].blockID>Max_Block_Num){
           memSets[snp].blockID=-1;//BlockID
           memSets[snp].isParity=-1;
           }
      }
        if(memSets[snp].isParity==1){  //处理数据块
           if(memSets[snp].blockID>=Stripe_Num*R){
           memSets[snp].blockID=-1;//BlockID
           memSets[snp].isParity=-1;
           }
      }
   }



   printf("========================node_constructing_stripes()=======================\n");
   int ho;
   for(ho=0;ho<Stripe_Num_Index;ho++){  //
      printf("memSets[%ld].blockID=%ld isParity=%d\n",ho,memSets[ho].blockID,memSets[ho].isParity);
   }
   printf("===========================================================================\n");



/*

  for(i=0;i<Stripe_Num_Index;i++){
    long blockID;
    Node2CoordinatorConstructingStripesRequest *n2ccs_request=(Node2CoordinatorConstructingStripesRequest *)malloc(sizeof(Node2CoordinatorConstructingStripesRequest));
   
    blockID=(NodeNumTotal-2)*i+nodeNum;
    memSets[memSet_index].blockID=blockID;  //内存memLocation 

    //内容的赋值
    if(blockID=((K+R)*i+4)||blockID=((K+R)*i+5)){ //校验分块的处理
        memSets[memSet_index].isParity=1;
        n2ccs_request->isParity=1;  //是校验分块
     
    }else{  //数据块的处理
        memSets[memSet_index].isParity=0;
        n2ccs_request->isParity=0; //不是校验块

        char blockBuf[Block_Size];
        int t;
        for(t=0;t<Block_Size;t++){
         blockBuf[t]='a';
        }

      printf("=================blockBuf[0]=%c\n",blockBuf[0]);
      memcpy(memSets[memSet_index].blockBuf,blockBuf,sizeof(char)*Block_Size);  //对于数据节点的处理
      memcpy(n2ccs_request->blockBuf,blockBuf,sizeof(char)*Block_Size);  //对于数据节点的处理
      printf("node_constructing_stripes()===========nodeNum=%d data blockID=%ld memSet_index =%d\n",nodeNum,blockID,memSet_index ); 
    }
     
     n2ccs_request->position=blockID%(K+R); //数据分块在冗余组中的位置编号0,1,2,3  快号除以K的余数
     n2ccs_request->blockID=blockID; //块编号
     n2ccs_request->RG_ID=blockID/(k+R)+1;  //条带编号   快号除以K的商 +1 
     n2ccs_request->flag=0; //在内存中
     n2ccs_request->memLocation=memSet_index; //内存数据分块的地址
     n2ccs_request->nodeNum=nodeNum;
     node2coordinatorStripeRequest_send(n2ccs_request); //向协调器发送请求                   
     printf("node_constructing_stripes()==========backupNode send block to coordinator\n");
  
   
     //校验节点的处理，接收校验分块。
     if(blockID=((K+R)*i+4)||blockID=((K+R)*i+5)){  //blockID的节点处理
      char recvParity[Block_Size];
      recv_bytes(node2coordinator_sockfd,recvParity,Block_Size);  //接收校验分块
      printf("node_constructing_stripes() ============node recv parity block from coordinator\n");
      memcpy(memSets[memSet_index-1].blockBuf,recvParity,sizeof(char)*Block_Size);
     
      char c='y';
      send_bytes(node2coordinator_sockfd,&c,1);  //发送成功接收到校验分块的反馈  
      printf("node_constructing_stripes()============node connect coordinator that has accepted parity block\n"); 
     }

      memSet_index++;


    //所有节点收到开启下一个条带冗余组的信息
    char *feedback=(char *)malloc(sizeof(char));
    recv_bytes(node2coordinator_sockfd,feedback,1);
    printf("node_constructing_stripes()===========node has accepted info from coordinator thar can start next stripe\n");

  }*/
/*   
   printf("========================node_constructing_stripes()=======================\n");
   long h;
   for(h=1;h<Stripe_Num_Index;h++){  //
      printf("memSets[%ld].blockID=%ld\n",h,memSets[h].blockID);
   }
   printf("===========================================================================\n");*/
   return 0;
}






/*节点构造条带初始化*/
/*int node_constructing_stripes(){

  printf("*********************node_constructing_stripes starting************************\n");

  for(i=0;i<Stripe_Num;i++){
    long blockID;
    Node2CoordinatorConstructingStripesRequest *n2ccs_request=(Node2CoordinatorConstructingStripesRequest *)malloc(sizeof(Node2CoordinatorConstructingStripesRequest));
   
    if(nodeNum<K){  //节点号0,1,2,3 的节点是数据节点
       

      blockID=i*K+nodeNum;  //加载到内存的块号是0,1,2,3，  总共 Stripe_Num*K 个块
      memSets[memSet_index].blockID=blockID;
     
     //填充数据块
     // char *blockBuf=(char *)malloc(sizeof(char)*Block_Size);
     // memset(blockBuf,'a',sizeof(char)*Block_Size);  
      char blockBuf[Block_Size];
      int t;
      for(t=0;t<Block_Size;t++){
      blockBuf[t]='a';
      }
      printf("=================blockBuf[0]=%c\n",blockBuf[0]);


     // memSets[memSet_index].blockBuf=(char *)malloc(sizeof(char)*Block_Size);
     // memSets[memSet_index].blockBuf=blockBuf;
      memcpy(memSets[memSet_index].blockBuf,blockBuf,sizeof(char)*Block_Size);
      printf("node_constructing_stripes()===========nodeNum=%d data blockID=%ld memSet_index =%d\n",nodeNum,blockID,memSet_index ); 

      n2ccs_request->hot_level=1;
     // n2ccs_request->blockBuf=(char *)malloc(sizeof(char)*Block_Size);
     // n2ccs_request->blockBuf=blockBuf;
      memcpy(n2ccs_request->blockBuf,blockBuf,sizeof(char)*Block_Size);

      n2ccs_request->position=nodeNum; //数据分块在冗余组中的位置编号0,1,2,3
      memSet_index++;
    }else if(nodeNum>=2*K&&nodeNum<NodeNumTotal-2) {  //8 9 号是校验节点
      blockID=-2*i-nodeNum+7;  //校验分块的blockID -1，-2，-3，-4
      printf("node_constructing_stripes()============nodeNum =%d parity blockID =%ld memSet_index=%d \n",nodeNum,blockID,memSet_index );
      memSets[memSet_index].blockID=blockID;
      //memSets[memSet_index].blockBuf=(char *)malloc(sizeof(char)*Block_Size);
      //n2ccs_request->blockBuf=NULL;
      n2ccs_request->hot_level=-1;
      n2ccs_request->position=nodeNum-K; //校验分块在冗余组中的位置编号4,5号 节点号是8号 9号
      memSet_index++;             //只定义blockID 校验分块的内容等待coordinator进行校验计算得到
    }

     n2ccs_request->blockID=blockID; //块编号
     n2ccs_request->RG_ID=i;  //条带编号
     n2ccs_request->flag=0; //在内存中
     n2ccs_request->memLocation=memSet_index-1; //内存数据分块的地址
     n2ccs_request->nodeNum=nodeNum;
     node2coordinatorStripeRequest_send(n2ccs_request); //发送请求
     printf("node_constructing_stripes()==========node send block to coordinator\n");

    if(nodeNum>=2*K&&nodeNum<NodeNumTotal-2){  //如果是校验节点，则接受校验块
      //char *recvParity=(char *)malloc(sizeof(char)*Block_Size); 
      char recvParity[Block_Size];
      recv_bytes(node2coordinator_sockfd,recvParity,Block_Size);  //接收校验分块
      printf("node_constructing_stripes() ============node recv parity block from coordinator\n");

      //memSets[memSet_index-1].blockBuf=(char *)malloc(sizeof(char)*Block_Size);
      //memSets[memSet_index-1].blockBuf=recvParity;
      memcpy(memSets[memSet_index-1].blockBuf,recvParity,sizeof(char)*Block_Size);
     
      char c='y';
      send_bytes(node2coordinator_sockfd,&c,1);  //发送成功接收到校验分块的反馈  
      printf("node_constructing_stripes()============node connect coordinator that has accepted parity block\n");     
    }

     
    //所有节点收到开启下一个条带冗余组的信息
    char *feedback=(char *)malloc(sizeof(char));
    recv_bytes(node2coordinator_sockfd,feedback,1);
    printf("node_constructing_stripes()===========node has accepted info from coordinator thar can start next stripe\n");

  }
   
   printf("========================node_constructing_stripes()=======================\n");
   long h;
   for(h=0;h<Stripe_Num;h++){  //
      printf("memSets[%ld].blockID=%ld\n",h,memSets[h].blockID);
   }
   printf("===========================================================================\n");
    return 0;
}*/


int processIORequest(){
  //printf("==========================processIORequest()======================================\n");
 // printf("processIORequest()==================================begin\n");
 // double responseTime=0.0;
  int count=0;  //waitset中count的个数
  int waitLocation=0; //waitset中存放的位置
  int memLocation1=0;
  //char *blockBuf =(char *)malloc(sizeof(char)*Block_Size);    //从硬盘读取的数据
 // memset(blockBuf,'a',sizeof(char)*Block_Size); //设置数据分块的内容
  //char blockBufMem[Block_Size];  //从内存中读取的数据
  //struct timeval stime,etime1,etime2,etime3,etime4,etime5,etime6,etime;
  Client2NodeRequest *c2n_request=(Client2NodeRequest *)malloc(sizeof(Client2NodeRequest));
  Node2CoordiantorRequest *n2coor_request=(Node2CoordiantorRequest *)malloc(sizeof(Node2CoordiantorRequest));
  char blockBufMem[Block_Size];  //从内存中读取的数据
  client2node_recv(c2n_request);   //接收客户端的请求
  accessCount++;  //节点访问频次
  printf("processIORequest()======accessCount=%d\n",accessCount);
  printf("processIORequest()======diskAccessCount=%d\n",diskAccessCount);
 printf("processIORequest()======memAccessCount=%d\n",memAccessCount);

 // printf("processIORequest()======node recv request from client\n");
    if(c2n_request->isPlayingTraces==0){
      printf("client closed and node[%d] starting destory\n",nodeNum);
      return ERR;
    }
 //  printf("processIORequest()======node[%d] starting process request [%ld]\n",nodeNum,c2n_request->requestID);
  // printf("processIORequest()======node starting process request from client\n");
  // char *blockBuf =(char *)malloc(sizeof(char)*Block_Size);    //从硬盘读取的数据
 //  char blockBufMem[Block_Size];  //从内存中读取的数据
  // long replaceBlockID;// 返回当等待队满的时候，要替换的id号
 //  int count=0;  //waitset中count的个数
//   int waitLocation=0; //waitset中存放的位置
//   int memLocation1=0;
   //printf("======================================================\n");
 //  printf("processIORequest()======c2n_request->flag=%d\n",c2n_request->flag);
 //  printf("processIORequest()======c2n_request->blockID=%ld\n", c2n_request->blockID);

  // gettimeofday(&stime,NULL);
  // printf("a=======\n");
   if(c2n_request->flag==2){ //从磁盘中读取
    char *blockBuf =(char *)malloc(sizeof(char)*Block_Size); 
 //  gettimeofday(&etime1,NULL);
//   printf("etime1 is %f (s) \n",((etime1.tv_sec-stime.tv_sec)+((double)(etime1.tv_usec-stime.tv_usec)/1000000)));
 //  diskAccessCount++;
   int nodeNumTemp=c2n_request->nodeNum;
   int offsetTemp=c2n_request->offset;
   int devfd;
   char *dev=nodes[nodeNumTemp].dev;  //获取该节点的设备号
 //  gettimeofday(&etime2,NULL);
//   printf("etime2 is %f (s) \n",((etime2.tv_sec-stime.tv_sec)+((double)(etime2.tv_usec-stime.tv_usec)/1000000)));
    devfd = open(dev,O_RDWR); // 打开设备
    if(lseek(devfd,offsetTemp,SEEK_SET)<0){
      perror("lseek error");
      return ERR;
    }
   // gettimeofday(&etime3,NULL);
   // printf("etime3 is %f (s) \n",((etime3.tv_sec-stime.tv_sec)+((double)(etime3.tv_usec-stime.tv_usec)/1000000)));
   // printf("**************************************\n");
   // memset(blockBuf,'a',sizeof(char)*Block_Size); //设置数据分块的内容
    read_bytes(devfd,blockBuf,Block_Size);
  ///  printf("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n");
   // gettimeofday(&etime,NULL);  //请求完成
  
    diskAccessCount++;
    memset(blockBuf,'a',sizeof(char)*Block_Size); //设置数据分块的内容
    memcpy(blockBufMem,blockBuf,sizeof(char)*Block_Size);
   // gettimeofday(&etime4,NULL);
  //  printf("etime4 is %f (s) \n",((etime4.tv_sec-stime.tv_sec)+((double)(etime4.tv_usec-stime.tv_usec)/1000000)));
  //  printf("ccccccccccccccccccccccccccccccccccccccc\n");
   // gettimeofday(&etime,NULL);  //请求完成
    //printf("server read disk block successed\n");
  //  responseTime=(etime.tv_sec-stime.tv_sec)+((double)(etime.tv_usec-stime.tv_usec)/1000000);
   // printf("user response time is %f (s) \n",((etime.tv_sec-stime.tv_sec)+((double)(etime.tv_usec-stime.tv_usec)/1000000)));

 //   memset(blockBuf,'a',sizeof(char)*Block_Size); //设置数据分块的内容
   // printf("processIORequest()======blockBuf strlen is  %ld\n",strlen(blockBuf));

  //  n2coor_request->accessTime=etime.tv_sec+(double)etime.tv_usec/1000000;  //数据分块的访问时间
   // printf("processIORequest()========data block accessTime is %lf(s) \n",n2coor_request->accessTime);

    
      if(c2n_request->isConstructingStripe==0){  //正在初始化， 则加载到内存中 并不会发生（因为初始化提前做好了）
        // memSets[memSet_index].blockBuf=blockBuf;
         memcpy(memSets[memSet_index].blockBuf,blockBuf,sizeof(char)*Block_Size);
         memSets[memSet_index].blockID=c2n_request->blockID;
         n2coor_request->memLocation=memSet_index;
         n2coor_request->flag=0;
         memSet_index++;
      }else{   //需要执行这一步，加载到等待集合中  首先要判断等待集合是否已经满了
         //①判断是否为满 ，还要结合替换操作 替换可能中间空着了  所以需要整体blockID=-1的个数  如果等于0已经满
         //当队列已经满的时候，就不再进行添加。 当队列没有满的话，则添加进去
       
         //判断blockID=-1 的个数
         int s;  
         for(s=0;s<Max_Wait_Length;s++){
          //if(waitSets[s].blockID!=-1){
          if(waitSets[s].blockID>-1){
            count++;
          }
         }
        // printf("processIORequest()======count=%d\n",count);

        if(count<Max_Wait_Length){  //等待队列未满。添加到等待队列
         // printf("processIORequest()======the waitSet is not full\n");
          isFullWaitSets=0;
           
         //寻找第一个ID!=-1的位置 waitLocation
            int ws;
            for(ws=0;ws<Max_Wait_Length;ws++){
              //if(waitSets[ws].blockID==-1){
                if(waitSets[ws].blockID<0){
                waitLocation=ws;
                break;
              }
            }

         memcpy(waitSets[waitLocation].blockBuf,blockBuf,Block_Size);
         waitSets[waitLocation].blockID=c2n_request->blockID;
         n2coor_request->memLocation=(long)waitLocation;
         n2coor_request->flag=1;
       //  printf("processIORequest()======waitLocation=%d\n",waitLocation);
        }else{  //队列已经满了 ，只进行响应，元数据不变。 元数据只记录存在内存中的数据（包括内存布局中的，还有等待队列中的）
           isFullWaitSets=1;  //等待队列已经满了 并不需要修改相关元数据的信息 但是还是需要发送该数据结构（代码有待改善）
           n2coor_request->memLocation=-1;
           n2coor_request->flag=2;
        }
      }
        n2coor_request->requestID=c2n_request->requestID;
        n2coor_request->blockID=c2n_request->blockID;
        n2coor_request->nodeNum=nodeNum;
        n2coor_request->freq=c2n_request->freq; //访问硬盘时候访问频次不改变
      //  printf("processIORequest()======node process disk IO request successed\n");
   }else{ //从内存中读取，包括从内存集合或者等待集合中读取
     
         //gettimeofday(&etime5,NULL);
       //  printf("etime5 is %f (s) \n",((etime5.tv_sec-stime.tv_sec)+((double)(etime5.tv_usec-stime.tv_usec)/1000000)));
 
 
        if(c2n_request->flag ==1){  //等待集合中读取
          //blockBuf=waitSets[c2n_request->memLocation].blockBuf;
          memLocation1=c2n_request->memLocation;
    //      gettimeofday(&etime,NULL);//请求完成
          memcpy(blockBufMem,waitSets[memLocation1].blockBuf,sizeof(char)*Block_Size);

      //   gettimeofday(&etime6,NULL);
        // printf("wait etime6 is %f (s) \n",((etime6.tv_sec-stime.tv_sec)+((double)(etime6.tv_usec-stime.tv_usec)/1000000)));
         // memcpy(blockBufMem,waitSets[c2n_request->memLocation].blockBuf,sizeof(char)*Block_Size);
         //  gettimeofday(&etime,NULL);//请求完成
     //     printf("processIORequest()======node read waitSet block\n");
        } 



        if(c2n_request->flag ==0){ //从内存布局中的数据分块
         // printf("b===========\n");
          //blockBuf=memSets[c2n_request->memLocation].blockBuf;
          memLocation1=c2n_request->memLocation;
         // printf("memLocation1=%d and c2n_request->memLocation\n",memLocation1,c2n_request->memLocation);
       //   printf("c==============\n");
     //     gettimeofday(&etime,NULL);//请求完成
      //    printf("d=============\n");
          memcpy(blockBufMem,memSets[memLocation1].blockBuf,sizeof(char)*Block_Size);
      //    printf("e=============\n");

       //  gettimeofday(&etime6,NULL);
       //  printf("mem etime6 is %f (s) \n",((etime6.tv_sec-stime.tv_sec)+((double)(etime6.tv_usec-stime.tv_usec)/1000000)));

       //   memcpy(blockBufMem,memSets[c2n_request->memLocation].blockBuf,sizeof(char)*Block_Size);
       //   gettimeofday(&etime,NULL);//请求完成
   //       printf("processIORequest()======node read memSet block\n");
        }
      

        isFullWaitSets=-1;  //从等待集合中取 并不判断是否有问题
        memAccessCount++; 
        n2coor_request->requestID=c2n_request->requestID;
        n2coor_request->blockID=c2n_request->blockID;
        n2coor_request->nodeNum=nodeNum;
        n2coor_request->freq=c2n_request->freq+1;
        n2coor_request->memLocation=c2n_request->memLocation;
        n2coor_request->flag=c2n_request->flag;


     //   printf("processIORequest()======read mem blockID is %ld \n",c2n_request->blockID);
 //       responseTime=(etime.tv_sec-stime.tv_sec)+((double)(etime.tv_usec-stime.tv_usec)/1000000);
    //    printf("processIORequest()======user response time is %f (s) \n",((etime.tv_sec-stime.tv_sec)+((double)(etime.tv_usec-stime.tv_usec)/1000000)));
 //      n2coor_request->accessTime=etime.tv_sec+(double)etime.tv_usec/1000000;  //数据分块的访问时间
     //   printf("processIORequest()======data block accessTime is %f",n2coor_request->accessTime);
        }
        
        n2coor_request->isDuplication=c2n_request->isDuplication; //还是原来的是不是副本数据
        n2coor_request->accessCount=accessCount;    //总的访问频次
        n2coor_request->isFullWaitSets=isFullWaitSets;
        n2coor_request->diskAccessCount=diskAccessCount;  //磁盘访问频次
        n2coor_request->memAccessCount=memAccessCount; //内存中的数据访问频次
  //      n2coor_request->responseTime=responseTime;  //访问时间
        node2coordinator_send(n2coor_request);   //向协调器发送请求
  //      printf("processIORequest()======node send request to coordinator and nodeNum is =%d and accessCount=%d\n",nodeNum,n2coor_request->accessCount);
      /*  send_bytes(node2client_sockfd,blockBuf,Block_Size); //响应客户请求，返回client数据分块 
        printf("processIORequest()======node send block to client\n");*/

        /*
         if(n2coor_request->isFullWaitSets==1 && c2n_request->flag==2 ){ //队列已经满了 并且请求是从客户端发过来，读取硬盘的时候才会发生替换 
         send_bytes(node2coordinator_sockfd,(char *)&replaceBlockID,sizeof(long));
         printf("processIORequest()=========replaceBlockID=%d\n",replaceBlockID);
         printf("processIORequest()=========server has send replaceBlockID to coordinator\n");
        }*/

   
        //和前边分别对应
      /*  if(c2n_request->flag==2){  //硬盘 
           send_bytes(node2client_sockfd,blockBuf,Block_Size); //响应用户读请求，返回client数据分块
           printf("processIORequest()======node send block to client\n");
        }else {  //内存
           send_bytes(node2client_sockfd,blockBufMem,Block_Size); //响应用户读请求，返回client数据分块
           printf("processIORequest()======node send block to client\n");
        }*/
       
        send_bytes(node2client_sockfd,blockBufMem,Block_Size); //响应用户读请求，返回client数据分块
        printf("processIORequest()======node send block to client\n");

        


  
 // printf("==========================processIORequest()======================================\n");
  long h;
   for(h=0;h<Stripe_Num_Index;h++){  //
      printf("processIORequest()========memSets[%ld].blockID=%ld\n",h,memSets[h].blockID);
   }
  for(h=0;h<Max_Wait_Length;h++){  //
      printf("processIORequest()========waitSets[%ld].blockID=%ld\n",h,waitSets[h].blockID);
   }
   return 0;   
}

void layout_update(){  //处理数据布局更新
  printf("layout_update()====================================\n");
 
  while(1){

    if(system_status==1){
      //pthread_exit(NULL);
      return ERR;
    }
    layIndex++;
    Coordinator2NodeLayoutUpdateRequest *request=(Coordinator2NodeLayoutUpdateRequest *)malloc(sizeof(Coordinator2NodeLayoutUpdateRequest));
    coordinator2nodeLayoutUpdateRequest_recv(request);
    printf("layout_update()===========================and layIndex=%d\n",layIndex);
    printf("node has recved request from coordinator and request->code=%d and request->blockID=%d  request->targetNodeNum=%d and request->sourceNodeNum=%d \n",request->code,request->blockID,request->targetNodeNum,request->sourceNodeNum);
    if(request->code==1){
      if(request->targetNodeNum==nodeNum){  //需要操作的节点，然后需要删除该节点的副本
       printf("this is targetNodeNum and request->code=1\n");
       printf("request->targetMemLocation=%d \n",request->targetMemLocation);
       memSets[request->targetMemLocation].blockID=-1; //标记该副本已经被删除
       printf("memSets[request->targetMemLocation].blockID=%d\n",memSets[request->targetMemLocation].blockID );
       memset(memSets[request->targetMemLocation].blockBuf,'\0',sizeof(memSets[request->targetMemLocation].blockBuf));//副本块置为空
      //...给coordinator反馈
       char c='y';
       send_bytes(node2coordinatorLayout_sockfd,&c,1);
       printf("layout_update()====node send feedback to coordinator  that has deleted duplications block\n");
      }
    }
    if(request->code==2){   //增加副本
      if(request->targetNodeNum==nodeNum){  //需要接收
        Block *block=(Block *)malloc(sizeof(Block));
        printf("this is targetNodeNum ande request->code =2\n");
        node2node_recv(block,request->sourceNodeNum);
        //找到第一个ID!=-1的地方存放
        int memLocation;
        int mlo;
        for(mlo=0;mlo<Stripe_Num_Index*2;mlo++){
          if(memSets[mlo].blockID==-1){
             memLocation=mlo;
             break;
          }
        }
        printf("request->code=2  and memLocation=%d\n",memLocation);
        memSets[memLocation].blockID=request->blockID;
        memcpy(memSets[memLocation].blockBuf,block->blockBuf,sizeof(char)*Block_Size);
         //memSet_index++;
        //向协调器发送内存的位置
        //int memLocation=memSet_Index-1;
        Node2CoordiantorLayoutUpdateResponse *response=(Node2CoordiantorLayoutUpdateResponse *)malloc(sizeof(Node2CoordiantorLayoutUpdateResponse));
        response->code2memLocation=memLocation;
        printf("code2memLocation=%d\n", response->code2memLocation);
        node2coordinatorLayoutUpdateRequest_send(response);
      //  send_bytes(node2coordinatorLayout_sockfd,(char *)&memLocation,sizeof(&memLocation)); //向coordinator发送副本分块的内存位置
        printf("layout_update()====node send memLocation to coordinator \n");
      }

      if(request->sourceNodeNum==nodeNum){  //需要发送
         printf("this is sourceNodeNum and request->code=2\n");
         Block *block=(Block *)malloc(sizeof(Block));
         printf("a\n");
         block->blockID=request->blockID;
         printf("b\n");
         memcpy(block->blockBuf,memSets[request->sourceMemlocation].blockBuf,sizeof(char)*Block_Size);
         printf("c\n");
         node2node_send(block,request->targetNodeNum);
         printf("layout_update()====sourceNodeNum send block to targetNodeNum\n");
      }

    }
    if(request->code==3){
       if(request->targetNodeNum==nodeNum&&request->sourceNodeNum==nodeNum){ //本地局部性的体现
         printf("this is targetNodeNum and sourceNodeNum and targetNodeNum===sourceNodeNum and request->code=3\n");
          //先找一个lock 暂存内存队列的值
          Block *block=(Block *)malloc(sizeof(Block));
          block->blockID=-1;
          memcpy(block->blockBuf,memSets[request->targetMemLocation].blockBuf,sizeof(char)*Block_Size);
         
         //替换更新
         memSets[request->targetMemLocation].blockID=request->blockID;
         memcpy(memSets[request->targetMemLocation].blockBuf,waitSets[request->sourceMemlocation].blockBuf,sizeof(char)*Block_Size);
         waitSets[request->sourceMemlocation].blockID=-1;
         memset(waitSets[request->sourceMemlocation].blockBuf,'\0',sizeof(waitSets[request->sourceMemlocation].blockBuf)); //等待队列

         //计算校验块
         char *deltaBuf=(char *)malloc(Block_Size*sizeof(char));//delta块
         calc_xor(memSets[request->targetMemLocation].blockBuf,block->blockBuf,deltaBuf,Block_Size*sizeof(char)); //由旧数据分块和新数据分块得到Delta数据分块 RMW
         //发送给协调器
         send_bytes(node2coordinatorLayout_sockfd,deltaBuf,Block_Size*sizeof(char));
         printf(" layout_update()====node send deltaBuf to coordinator %s\n");
       } else{ //网络负载均衡的体现
         if(request->targetNodeNum==nodeNum){ //接收
          /* printf("this is targetNodeNum and request->code=3\n");
           Block *block=(Block *)malloc(sizeof(Block));
           node2node_recv(block,request->sourceNodeNum);
           printf("layout_update()====targetNodeNum recved block from sourceNodeNum\n");
           node2coordinatorBlock_send(&(memSets[request->targetMemLocation])); //发送旧的block
           printf("layout_count====targetNodeNum send old block to coodinator\n");
           memSets[request->targetMemLocation].blockID=block->blockID;
           memcpy(memSets[request->targetMemLocation].blockBuf,block->blockBuf,sizeof(char)*Block_Size);*/

           
           //发送差值  不发送块  计算之后只发送差值部分
           printf("this is targetNodeNum and request->code=3\n");
           Block *block=(Block *)malloc(sizeof(Block));
           node2node_recv(block,request->sourceNodeNum);
           printf("layout_update()====targetNodeNum recved block from sourceNodeNum\n");

           char *deltaBuf=(char *)malloc(Block_Size*sizeof(char));//delta块
           calc_xor(memSets[request->targetMemLocation].blockBuf,block->blockBuf,deltaBuf,Block_Size*sizeof(char));
           send_bytes(node2coordinatorLayout_sockfd,deltaBuf,Block_Size*sizeof(char));


         //  node2coordinatorBlock_send(&(memSets[request->targetMemLocation])); //发送旧的block
         //  printf("layout_count====targetNodeNum send old block to coodinator\n");
           memSets[request->targetMemLocation].blockID=block->blockID;
           memcpy(memSets[request->targetMemLocation].blockBuf,block->blockBuf,sizeof(char)*Block_Size);

          // node2coordinatorBlock_send(block); //发送新的block
         //  printf("layout_update()====targetNodeNum send new block to coodinator \n");

           
    /*      //向原节点发送反馈消息
          char *feedback=(char *)malloc(sizeof(char));
          int m;
          int fd;
          for(m=0;m<NodeNumTotal-2-1;m++){
           if(nodeAsClient2NodeConnect[m].targetNode==request->sourceNodeNum){
            fd=nodeAsClient2NodeConnect[m].sockfd;
            }
          }    
         send_bytes(fd,feedback,1);
         printf("layout_update()====targetNodeNum send  feedback to sourceNodeNum\n");*/

        }

         if(request->sourceNodeNum==nodeNum){ //发送
          printf("this is sourceNodeNum ande request->code=3\n");
          Block *block=(Block *)malloc(sizeof(Block));
          block->blockID=request->blockID;
          memcpy(block->blockBuf,waitSets[request->sourceMemlocation].blockBuf,sizeof(char)*Block_Size);
          node2node_send(block,request->targetNodeNum);
          printf("layout_update()====sourceNodeNum send block to targetNodeNum\n");

         

         // node2coordinatorBlock_send(block); //发送新的数据块至协调器
          // printf("layout_update()====sourceNodeNum send block to coodinator\n");
           
     /*      //接收反馈
           char *feedback=(char *)malloc(sizeof(char));
           int m;
           int fd;
           for(m=0;m<NodeNumTotal-2-1;m++){
             if(nodeAsServer2NodeAccept[m].sourceNode==request->targetNodeNum){
               fd=nodeAsServer2NodeAccept[m].sockfd;
             }
           }
           recv_bytes(fd,feedback,1); 
           printf("layout_update()====sourceNodeNum has recved feedback from targetNodeNum\n");*/

          waitSets[request->sourceMemlocation].blockID=-1;
          memset(waitSets[request->sourceMemlocation].blockBuf,'\0',sizeof(waitSets[request->sourceMemlocation].blockBuf)); //等待队列
         // waitSet_index--;   //需要注释掉================================================================
        }

       }
     }
    if(request->code==4){
          if(request->targetNodeNum==nodeNum){//此节点为校验节点
            printf("this is targetNodeNum and request->code ==4\n");
            char *parityBuf=(char *)malloc(Block_Size*sizeof(char));//新的校验分块
            char *deltaBuf=(char *)malloc(Block_Size*sizeof(char));//delta数据分块
            recv_bytes(node2coordinatorLayout_sockfd,deltaBuf,Block_Size*sizeof(char));
            printf("layout_update()====node has recved deltaBuf from coordinator\n");
            calc_xor(deltaBuf,memSets[request->targetMemLocation].blockBuf,parityBuf,Block_Size*sizeof(char));//delta数据分块与旧的校验分块计算得到新的校验分块
            memcpy(memSets[request->targetMemLocation].blockBuf,parityBuf,Block_Size*sizeof(char));

         char feedback='y';
         send_bytes(node2coordinatorLayout_sockfd,&feedback,1);
         printf("layout_update()====node send feedback to coordinator\n");
        }

    }
  }

}







int main() {
	int flag1;
  int flag2;
	flag1= node_init();
	if (flag1==-1) {
		printf("node is error\n");
		return  -1;
	} else{
     for ( i = 0; i < NodeNumTotal-2-1; i++){
      printf("node[%d]AsConnectSockfd  connect nodes[%d] successed and sockfd=%d \n",nodeNum,nodeAsClient2NodeConnect[i].targetNode,nodeAsClient2NodeConnect[i].sockfd);
    }
      for ( i = 0; i < NodeNumTotal-2-1; i++){
      printf("Node[%d]AsAcceptSockfd  accept nodes[%d] successed and sockfd=%d\n",nodeNum,nodeAsServer2NodeAccept[i].sourceNode,nodeAsServer2NodeAccept[i].sockfd);
    }
    printf("node init  successed\n");
    
   pthread_t layout_pid;//布局更新线程
   pthread_create(&layout_pid,NULL,layout_update,NULL);
    while(1){
      flag2=processIORequest();   
     // usleep(10000000);
      //layout_update(); 
      if(flag2==-1){
        system_status=1;
        pthread_join(layout_pid,NULL);
        usleep(2000000);
        node_destroy();
        //pthread_exit(NULL);//////??? 
        break;
      }
    }
   return 0;
	}
}