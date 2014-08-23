#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <string.h>
#include <errno.h>


#define MAX_BUF 4*1024*1024

//ip
static char szIp[20] = {0};

//port
static int	nPort = 0;

//send times
static int nTimes = 1;

//interval between every send.(micro second)
static int nInertval = 500000;

//whether broadcast to udp when sending.
static int nBroadcast = 0;

//whether output the content when sending or receiving data.
static int nVerbose = 0;

enum
{
	SEND_STRING = 1,
	SEND_BINARY,
};

//send file's format
static int nSendFormat = SEND_STRING;

enum
{
	FUN_SEND = 1,
	FUN_RECV = 2,
};
//whether sending/recving/testfilter data.
//1 -send
//2 -recv
//3 -testfilter
static int nFunction = FUN_SEND;

enum
{
	PROTOCOL_UDP = 1,
	PROTOCOL_TCP = 2,
};
//the protocol used.
//1 -udp
//2 -tcp
static int nProtocol = PROTOCOL_UDP;

//file to read from the content for sending.
static char szFile_Send[100] = {"./cmd"};
static int nSendHostPort = 203901;

//whether receiveing data.
//file to save the recving data.
static char szFile_Recv[100] = {0};
//whether save the receiving data.1-save;others-not save.
static int	nFileSave = 0;
static FILE* file_save = NULL;
	
static char szTime[20] = {"2011-04-18 10:21:22"};

static void testsleep();
static void mysleep(int n);
static char* mytime();
void createthread(void* fun, void* argv);

static void usage();
static int doParams(int argc,void** argv);
static int JoinGroup(int sock);
static int setPortReuse(int sock);
static int setTTL(int sock, int TTL);
static int socket4send();
static int socket4recv();
static int readCmd(char* pBuf);
static void sending();
static void recving();

/*
void char* mytime()
{
	int time = time(NULL);
	struct tm =
}
*/

void testsleep()
{
    printf("[testsleep].begin\n");
    usleep(10000000);
    printf("[testsleep].end\n");
}

void mysleep(int n)
{
    if(n <1000000)
    {
        usleep(n);
    }
    else
    {
        sleep(n/1000000);
    }
}

void createthread(void* fun, void* argv)
{
    pthread_attr_t attr;
    pthread_t thrd;
    struct sched_param sched_param_config;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    pthread_attr_setschedpolicy(&attr,SCHED_FIFO);
    sched_getparam(0,&sched_param_config);
    sched_param_config.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&attr,&sched_param_config);

    pthread_create(&thrd,&attr,fun, argv);
}

void usage()
{
    printf("usage: \e[1mxsock\e[0m \e[4mip\e[0m \e[4mport\e[0m [\e[4m-action\e[0m=ACTION] [\e[4m-protocol\e[0m=PROTOCOL]  [\e[4m-times\e[0m=SEND_TIMES] [\e[4m-inertval\e[0m=SEND_INTERVAL] [\e[4m-format\e[0m=SEND_FORMAT] [\e[4m-file\e[0m=FILE] [\e[4moptions\e[0m]\n");
    printf("\t\e[34m\e[4mip\e[0m\e[34m:\e[0m \t\tthe ip address to send to or receive from.\n");
    printf("\t\t\twhen sending,it means the destination host.\n");
    printf("\t\t\twhen sending multi-cast with -b option,it's the multi cast address(224.0.0.0 to 239.255.255.255).\n");
    printf("\t\t\twhen normal receiving(none multi cast),it's meanless and ignored(but you should still specify it with any valid value,such as 127.0.0.1).\n");
    printf("\t\t\twhen receiving multi cast with -b option,it specify the destination address(224.0.0.0 to 239.255.255.255).\n");
    printf("\t\e[34m\e[4mport\e[0m\e[34m:\e[0m \t\tthe port to send to or receive from.\n");
    printf("\t\t\twhen sending,it specify the destination port.\n");
    printf("\t\t\twhen sending multi cast with -b option,it's the broadcast port.\n");
    printf("\t\t\twhen normal receiving,it's the port of localthost to receive the data.\n");
    printf("\t\t\twhen receiving multi cast with -b option,it specify the destination port.\n");
    printf("\t\e[34m[\e[4m-action=ACTION\e[0m\e[34m]:\e[0m\n");
    printf("\t\t\t\e[1msend\e[0m:send data[DEFAULT]\n");
    printf("\t\t\t\e[1mreceive\e[0m:receive data\n");
    printf("\t\e[34m[\e[4m-protocol=PROTOCOL\e[0m\e[34m]:\e[0m\n");
    printf("\t\t\t\e[1mudp\e[0m:use udp protocol[DEFAULT]\n");
    printf("\t\t\t\e[1mtcp\e[0m:use tcp protocol\n");
    printf("\t\e[34m[\e[4m-times=SEND_TIMES\e[0m\e[34m]:\e[0m\n");
    printf("\t\t\ttimes of sending when use -send option.[DEFAULT:1]\n");
    printf("\t\e[34m[\e[4m-inertval=TIME_INTERVAL\e[0m\e[34m]:\e[0m\n");
    printf("\t\t\tinterval time(u seconds) of sending when use -send option.[DEFAULT:500000]\n");
    printf("\t\e[34m[\e[4m-format=SEND_FORMAT\e[0m\e[34m]:\e[0m\n");
    printf("\t\t\tsend file's format.\n");
    printf("\t\t\t\e[1mstring\e[0m:treate the file as string.[DEFAULT]\n");
    printf("\t\t\t\e[1mbinary\e[0m:treate the file as binary.\n");
    printf("\t\e[34m[\e[4m-file=FILE\e[0m\e[34m]:\e[0m\n");
    printf("\t\t\tthe file where to read from the content when sending,or the file to save the received data when receiving.\n");
    printf("\t\t\t[DEFAULT:'./cmd' for send;\n");
    printf("\t\t\tNULL for receive(the data won't be saved as a file,except you specify a file with this option).]\n");
    printf("\t\e[34m[\e[4moptions\e[0m\e[34m]:\e[0m\t-v  output verbose data when sending or receiving.\n");
    printf("\t\t\t-b  mutilbrocast when use udp protocol to send or receive.\n");
}

int doParams(int argc,void** argv)
{
    int nRet = 0;

    int nTemp = 0;
    char szTemp[100] = {0};

    int i = 0;
    for(i=0; i<argc; i++)
    {
        if(i == 1)
        {
                strcpy(szIp, argv[i]);
        }
        else if(i == 2)
        {
                nPort = atoi(argv[i]);
        }
        else if(strcmp(argv[i], "-v") == 0)
        {
                nVerbose = 1;
        }
        else if(strcmp(argv[i], "-b") == 0)
        {
                nBroadcast = 1;
	}
        else if( sscanf(argv[i], "-action=%s", szTemp)>0)
        {
            if(strcasecmp(szTemp, "send") == 0)
            {
                nFunction = FUN_SEND;
            }
            else if(strcasecmp(szTemp, "receive") == 0)
            {
		nFunction = FUN_RECV;
            }
        }
        else if( sscanf(argv[i], "-protocol=%s", szTemp)>0)
        {
            if(strcasecmp(szTemp, "tcp") == 0)
            {
                nProtocol = PROTOCOL_TCP;
            }
            else if(strcasecmp(szTemp, "udp") == 0)
            {
                nProtocol = PROTOCOL_UDP;
            }
        }
        else if( sscanf(argv[i], "-file=%s", szTemp)>0)
        {
                strcpy(szFile_Send, szTemp);
                strcpy(szFile_Recv, szTemp);
		    nFileSave = 1;
        }
        else if( sscanf(argv[i], "-times=%d", &nTemp)>0)
        {
                nTimes =nTemp;
        }
        else if( sscanf(argv[i], "-interval=%dus", &nTemp)>0)
        {
                nInertval =nTemp;
        }
        else if( sscanf(argv[i], "-format=%s", szTemp)>0)
        {
            if(strcasecmp(szTemp, "binary") == 0)
            {
                nSendFormat = SEND_BINARY;
            }
            else if(strcasecmp(szTemp, "string") == 0)
            {
                nSendFormat = SEND_STRING;
            }
        }
    }

    return nRet;
}

int JoinGroup(int sock)
{
    if(sock <= 0)

    printf("join group: %s...", szIp);
    {
        return -1;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(szIp);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0)
    {
        printf("failed! %s\n", strerror(errno));
        return -1;
    }
    else
    {
        printf("ok\n");
    }

    return 0;
}

int setPortReuse(int sock)
{
    if(sock <= 0)
    {
        return -1;
    }

    int value = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value) )< 0)
    {
        printf("setPortReuse() failed! %s\n",strerror(errno));
        return -1;
    }

    return 0;
}

int setTTL(int sock, int TTL)
{
    if( (sock <= 0) || (TTL <= 0) || (TTL >= 256))
    {
        return -1;
    }

     //unsigned char TTL = 65;
     if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&TTL, sizeof(TTL)) < 0)
     {
        printf("[setTTL] set  ttl to %d failed, %s\n", 65, strerror(errno));
        return -1;
     }

     return 0;
}

int socket4send()
{
    int sock = 0;
    if(nProtocol == PROTOCOL_UDP)//udp
    {
        sock = socket(AF_INET,  SOCK_DGRAM,  IPPROTO_UDP);
    }
    else if(nProtocol == PROTOCOL_TCP)
    {
        sock = socket(AF_INET,  SOCK_STREAM, IPPROTO_TCP);
    }

    if(sock <= 0)
    {
        printf("[socket] failed! %s\n", strerror(errno));
    }

    setPortReuse(sock);

    if(nProtocol == PROTOCOL_UDP)//udp.on some routers, if the ttl is too small, the multi-broadcast packet will lose.
    {
	setTTL(sock, 65);
    }

    struct sockaddr_in Addr;
    Addr.sin_family = AF_INET;
    Addr.sin_port = htons(203901);
    Addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr*)&Addr, sizeof(Addr)) < 0)
    {
        printf("[bind]error. %s\n", strerror(errno));
        return -1;
    }

    return sock;
}

int socket4recv()
{
    int sock = 0;

    if(nProtocol == PROTOCOL_UDP)//udp
    {
        sock = socket(AF_INET,  SOCK_DGRAM,  IPPROTO_UDP);
    }
    else if(nProtocol == PROTOCOL_TCP)
    {
        sock = socket(AF_INET,  SOCK_STREAM, IPPROTO_TCP);
    }

    if(sock <= 0)
    {
        printf("[socket] failed! %s\n", strerror(errno));
    }
    
    setPortReuse(sock);

    struct sockaddr_in Addr;
    Addr.sin_family = AF_INET;
    Addr.sin_port = htons(nPort);
    Addr.sin_addr.s_addr = INADDR_ANY;
    if(nBroadcast == 1)//if to receive udp multibroadcast,should specify the ip.
    {
        Addr.sin_addr.s_addr = inet_addr(szIp);
    }

    if(bind(sock, (struct sockaddr*)&Addr, sizeof(Addr)) < 0)
    {
        printf("[bind]error. %s\n", strerror(errno));
        
        close(sock);
        return -1;
    }

    if(nProtocol == PROTOCOL_UDP)
    {
        if(nBroadcast == 1)//join the udp multicast address.
        {
            JoinGroup(sock);
        }
    }
    else if(nProtocol == PROTOCOL_TCP)//for tcp connection,we must listen.
    {
        if(listen(sock, 5000) == 0)
        {
            printf("listening on port:%d...\n", nPort);
        }
        else
        {
            printf("listen failed!%s\n", strerror(errno));

            close(sock);
            sock = -1;
        }
    }
    
    return sock;
}

int readCmd(char* pBuf)
{
    int nRet = -1;
    if(pBuf != NULL)
    {
        FILE* file = fopen(szFile_Send, "r");
        if(file != NULL)
        {
            if(nSendFormat == SEND_STRING)
            {
                nRet = fread(pBuf, 1, MAX_BUF, file);
            }
            else if(nSendFormat == SEND_BINARY)
            {
/*
               char *line = NULL;
               size_t len = 0;
               ssize_t read;
                
               char* p = pBuf;
               while ((read = getline(&line, &len, file)) != -1) 
               {
                    int nTemp = 0;
                    char cTemp = '\0';
                    char sTemp[100] = {0};

                    printf("11111111\n");
                    //printf("%s\n", line);

                    printf("11111111\n");
                    if(sscanf(line, "int:%d\n", nTemp) >= 1)
                    {
                        printf("%d\n", nTemp);
                        *(int*)p = nTemp;

                        p += sizeof(int);
                        continue;
                    }
                    else
                    {
                        printf("int ERROR\n");
                    }

                    if(sscanf(line, "char:%c", cTemp) >= 1)
                    {
                        *(char*)p = cTemp;
                        
                        p += sizeof(char);
                        continue;
                    }
                    else
                    {
                        printf("char ERROR\n");
                    }

                    if(sscanf(line, "string:%s", sTemp) >= 1)
                    {
                        strncpy(p, sTemp, strlen(sTemp));
                        
                        p += strlen(sTemp);
                        continue;
                    }
                    else
                    {
                        printf("string ERROR\n");
                    }
               }

               free(line);
*/
            }

            fclose(file);
            file = NULL;
        }
        else
        {
            printf("[readCmd].open file:%s failed! %s\n", szFile_Send, strerror(errno));
        }
        nRet = strlen(pBuf);
    }

    return nRet;
}

void sending()
{
    //open the socket
    int sender = socket4send();
    if(sender <= 0)
    {
        return;
    }

    struct sockaddr_in to_addr;
    int to_len = sizeof(struct sockaddr_in);
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = htons(nPort);
    to_addr.sin_addr.s_addr = inet_addr(szIp);

    //read the cmd.
    char *szTemp = NULL;
    int nSize = 0;

    if(nSendFormat == SEND_STRING)
    {
        szTemp = (char*)malloc(sizeof(char)*MAX_BUF);
        nSize = readCmd(szTemp);
    }
    else if(nSendFormat == SEND_BINARY)
    {
        typedef struct SEND_STRUCT
        {
            int nAlarmType;
            int nStartTime;
            int nDeviceID;          //ip
            int nInterfaceIndex;
            int nProgramNum;
            int nDetectTime;
        }send_struct;

        send_struct data;
        data.nAlarmType = 2;
        data.nStartTime = time(NULL);
        data.nDeviceID = inet_addr("192.168.110.189");
        data.nInterfaceIndex = 1;
        data.nProgramNum = 1;
        data.nDetectTime = 4;

        nSize = sizeof(data);
	/*
        szTemp = (char*)&data;
	*/
        szTemp = (char*)malloc(nSize + sizeof(uint32_t));
	uint32_t* _p = (uint32_t*)szTemp;
	*_p = nSize;
	memcpy((void*)((char*)(szTemp + sizeof(uint32_t))), (void*)&data, nSize);
	nSize += sizeof(uint32_t);
    }


    if(nVerbose == 1)
    {
        printf("<<<<<<<<<<<<<<<<<<<<  CONTENT >>>>>>>>>>>>>>>\n");
        printf("%s\n", szTemp);
        printf("<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>\n");
    }

    //if(nProtocol == 2)//tcp,need to connet
    {
        if(connect(sender, (struct sockaddr_in*)&to_addr, to_len) == 0)
        {
            int i = 0;
            for(i=0; i< nTimes; i++)
            {
		//usleep(10000);
                printf("[%d]\tsending...", i+1);

                int nsent = 0 ;
                //if(nProtocol == 1)
                {
                    nsent = sendto(sender, (void*)szTemp, nSize, 0, (struct sockaddr_in*)&to_addr, to_len);
                }
                //else if(nProtocol == 2)
                {
                    //nsent = sendto(sender, (void*)szTemp, strlen(szTemp), 0, (struct sockaddr_in*)&to_addr, to_len);
                    //nsent = send(sender, (void*)szTemp, strlen(szTemp), 0);
                }

                if(nsent == nSize)
                {
                    printf("ok! sent = %d\n", nsent);
                }
                else
                {
                    printf("failed!sent = %d. %s\n", nsent, strerror(errno));
                }

                if(i < (nTimes-1))
                {
                    //mysleep(nInertval);
                }
            }
        }
        else
        {
            printf("connect() failed!%s\n", strerror(errno));
        }
    }        

    //free(szTemp);
    //close(sender);
}

void recving_savefile(char* p)
{
	if(file_save == NULL)
	{
    		file_save = fopen(szFile_Recv, "w");
	}

	if(fwrite(p, 1, strlen(p), file_save) <= 0)
	{
		printf("%s\n", strerror(errno));
	}
	fflush(file_save);
}

void recving_tcp_thread(void* argv)
{
	printf("recving_tcp_thread() start.\n");

    int sock = *((int*)argv);

    char *pTemp = (char*)malloc(sizeof(char)*MAX_BUF);
    while(1)
    {
        int nRecv = recv(sock, pTemp, MAX_BUF, 0);
        if (nRecv <= 0)
	 {
		if(nRecv == 0)
		{
			printf("connection 0x%x closed.\n\n", sock);
		}
		else
		{
			printf("connection 0x%x recv error.%s\n\n", sock, strerror(errno));
		}

	    	close(sock);
	     	break;		
	}

	printf("0x%x: [recv]. len = %d\n", sock, nRecv);        
        if(nVerbose == 1)
        {
            printf("%s\n", pTemp);
        }
	
	if(nFileSave == 1)
	{
		recving_savefile(pTemp);
	}
	char szTemp[MAX_BUF];
	while(scanf("%s", szTemp) > 0)	
	{
		//char* p = (char*)malloc(strlen(szTemp)*sizeof(char));
		//strcpy(p, szTemp);
   		//int nsent = send(sock, void*), szTemp, strlen(p), 0);
   		int nsent = send(sock, /*(void*)p*/(void*)szTemp, strlen(szTemp), 0);
		printf("nsent:%d\n", nsent);
	}
    }

    free(pTemp);
    close(sock);
}

void recving()
{
    int sock = socket4recv();
    if(sock <= 0)
    {
        return;
    }

    char *pTemp = (char*)malloc(sizeof(char)*MAX_BUF);

    int new_sock;
    struct sockaddr_in from;
    int from_len;

	while(1)
	{
		memset(pTemp, 0 ,MAX_BUF);
		if(nProtocol == PROTOCOL_UDP) //udp
		{
			if(recvfrom(sock, pTemp, MAX_BUF, 0, (struct sockaddr_in*)&from, &from_len) > 0)
			{
				//printf("[recvfrom]:[%s:%d], len =%d\n",  inet_ntoa(from.sin_addr), ntohs(from.sin_port),  strlen(pTemp));

				if(nVerbose == 1)
				{
				    printf("%s\n", pTemp);
				}

				if(nFileSave == 1)
				{
					recving_savefile(pTemp);
				}
			}
		}
		else if(nProtocol == PROTOCOL_TCP)//tcp
		{
		    new_sock = accept(sock, (struct sockaddr_in*)&from, &from_len);
		    printf("new connection(0x%x) to [%s:%d].\n",  new_sock, inet_ntoa(from.sin_addr), ntohs(from.sin_port));

		    createthread(recving_tcp_thread, (void*)&new_sock);
        	}
    }

    free(pTemp);
    close(sock);
}

//#define __DBUG_
int main(int argc,void** argv)
{
#ifdef __DBUG_
    argc = 7;
    int i=0;
    for(;i<7; i++)
    {
        argv[i] = (char*)malloc(20);
    }
    strncpy(argv[0], "xsock", 20);
    strncpy(argv[1], "127.0.0.1", 20);
    strncpy(argv[2], "7932", 20);
    strncpy(argv[3], "-action=send", 20);
    strncpy(argv[4], "-protocol=udp", 20);
    strncpy(argv[5], "-format=binary", 20);
    strncpy(argv[6], "-v", 20);
#endif
    //process the params.
    if(argc <3 || (doParams(argc, argv) < 0))
    {
        usage();
        return;
    }

    if(nFunction == FUN_SEND)
    {
        sending();
    }
    else if(nFunction == FUN_RECV)
    {
        recving();
    }

	while(1)
	{
		sleep(10);
	}
}
