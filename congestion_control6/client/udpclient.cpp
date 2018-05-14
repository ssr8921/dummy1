#include <bits/stdc++.h> 
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <openssl/md5.h>

using namespace std;

#define BUFSIZE 1024

sem_t full,empty;
pthread_mutex_t mutex;
struct sockaddr_in serveraddr;
int N=5;
int serverlen;

typedef struct
{
  int seq;
  int len;
  int type;//0-ack,1-data
  char buf[1024];
}datagram;

typedef struct 
{
    std::vector<datagram> array;
    FILE* fd;
    int nop;
}buffer;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int  UDP_receive(datagram* d,int sockfd,struct sockaddr * & serveraddr,int serverlen)
{
    bzero(d->buf, BUFSIZE);
    int n ;
    n = recvfrom(sockfd, d, sizeof(datagram), 0,(struct sockaddr *) &serveraddr, (socklen_t*)&serverlen);
    return n;
}


int UDP_send(datagram* d,int sockfd,struct sockaddr * & serveraddr,int serverlen)
{
    int n ;
    n = sendto(sockfd, d, sizeof(datagram), 0,(struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) 
    	 error("ERROR in sendto");
    return n;   
}

void create_packet(datagram *d,char* buf,int i,int j,int k)
{
   	if(k==-1) 
   		d->len=strlen(buf);
    else 
    	d->len=k;
    d->seq=i;
    d->type=j;
    bzero(d->buf, BUFSIZE);

    memcpy(d->buf,buf,d->len);
}

void * mbuf_func(void * ptr)
{
    buffer *k=(buffer *)ptr;
    int fs_block_sz;
    char buf[1024];
    datagram temp;
    
    do
    {
        fs_block_sz = fread(buf,sizeof(char),BUFSIZE,k->fd);
        k->nop=k->nop-1;
        if(fs_block_sz < 0)
        {
            printf("error in reading data\n");
            return 0;
        }
        create_packet(&temp,buf,-1,1,fs_block_sz);        
        sem_wait(&empty);
        pthread_mutex_lock(&mutex);
        k->array.push_back(temp);
        pthread_mutex_unlock(&mutex);
        sem_post(&full);
        if(k->nop==0){
         fclose(k->fd);
         break;
        }
    }while(1);
   
}

void client_details(int* sockfd1,char* filename,int* nochunks1,int* size11,char** argv)
{
    int sockfd;
    int portno;
    int n;
    struct hostent *server;
    char *hostname;
    char buf[1024];
    hostname = argv[1];
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
    	error("ERROR opening socket");
    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,(char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    serverlen = sizeof(serveraddr);

    bzero(buf, 1024);
    printf("Please enter the file name: ");
    fgets(buf, BUFSIZE, stdin);
    strcpy(filename,buf);
    int i;
    
    for(i=0;filename[i]!='\n';i++);
    filename[i]='\0';
    datagram* d=(datagram *)malloc(sizeof(datagram));
    create_packet(d,buf,-1,1,-1);
    n=UDP_send(d,sockfd,(struct sockaddr * &)serveraddr,serverlen);  
    n=UDP_receive(d,sockfd,(struct sockaddr * &)serveraddr,serverlen);
    printf("Echo from the server: %s", d->buf);


    bzero(buf, BUFSIZE);
    FILE *fp = fopen(filename,"r");
    if(fp == NULL){
        printf("unable to open file");
        exit(1);
    }
    fseek(fp,0,2);
    int size ;
    size = ftell(fp);
    snprintf (buf, sizeof(buf), "%d",size);
    
        
    create_packet(d,buf,-1,1,-1);
    n=UDP_send(d,sockfd,(struct sockaddr * &)serveraddr,serverlen);
    n=UDP_receive(d,sockfd,(struct sockaddr * &)serveraddr,serverlen);
    printf("Echo from the server: %s", d->buf);
    
   bzero(buf, BUFSIZE);
   int nochunks ;
   nochunks = (int)(size/1024)+( (size%1024==0)? 0 : 1) ;
    snprintf (buf, sizeof(buf), "%d",nochunks);
    
    create_packet(d,buf,-1,1,-1);
    n=UDP_send(d,sockfd,(struct sockaddr * &)serveraddr,serverlen);
    n=UDP_receive(d,sockfd,(struct sockaddr * &)serveraddr,serverlen);
    printf("Echo from server: %s", d->buf);


    (*sockfd1)=sockfd;
    (*size11)=size;
    (*nochunks1)=nochunks;
}

typedef struct
{
    int curr;
    int base;
    int cwnd;
    int rwnd;
    int windsize;
    int ssthresh;
    int state;
    int prev;
    int count;
    int ack;
    int flag;
}window;

void update_window(window* z,int nochunks)
{
  
        if(z->state==0)
        {
           z->windsize=(z->rwnd<z->cwnd)? z->rwnd : z->cwnd;
        }
        else if(z->state==1)
        {
            z->windsize=z->base-z->curr;
            if(z->windsize>=z->ssthresh)
            {
                z->ssthresh=z->ssthresh/2;
                z->cwnd=z->ssthresh;
            }
            else
            {
                z->windsize=1;
            }
        }            
        else if(z->state==2)
        {
            z->windsize=z->base-z->curr;
            if(z->prev==-1)
            {
                z->prev=z->ack;
                z->count=0;
            }
            else if(z->prev!=z->ack)
            {
                z->prev=z->ack;
                z->count=0;
            }
            else
            {
                z->count=z->count+1;
            }
            if(z->count==3)
            {
            
                z->count=0;
                z->ssthresh=z->ssthresh/2;
                if(z->ssthresh==0) z->ssthresh=1;
                	z->cwnd=z->ssthresh;
                 z->flag=1;
             }
        }
        else if(z->state==3)
        {
           
            if(z->windsize<=z->ssthresh) 
            	z->cwnd=z->windsize+(z->ack-z->curr);
            else    
            	z->cwnd=z->windsize+1;             
        }
        z->state=-1;
 
    return;
}


void app_send(char* filename,int nochunks,int sockfd,struct sockaddr * &serveraddr,int serverlen)
{

    FILE *fd =fopen(filename,"rb");
    int fs_block_sz;
    int i=0;
    int seqno=0;
    int n;
    std::vector<datagram> array;
    datagram temp,d;    
    char buf[1024];
    buffer send_buffer;
    pthread_t mbuf;
    sem_init(&full,0,0);
    sem_init(&empty,0,N);
    send_buffer.fd=fd;
    send_buffer.nop=nochunks;
    pthread_create(&mbuf,NULL,mbuf_func,(void *)&send_buffer);
    int w;
    int popsize;
    int p;
        
    window* z = (window *) malloc(sizeof(window));
    
    z->cwnd=z->rwnd=1;
    z->windsize=3;
    z->ssthresh=N;
    z->state=-1;
    z->prev=-1;
    z->count=0;
    z->curr=-1;
    z->base=0;
    

    while(z->curr != (nochunks-1)){
           
           z->state=0;
           if(z->flag==0)
           {
                z->windsize= (z->cwnd-array.size()<z->rwnd)? z->cwnd-array.size(): z->rwnd;
           }
           if(z->windsize<=0) 
           		z->windsize=1;
           update_window(z,nochunks);
           for(w=0;w<z->windsize;w++)
           {
                if(z->flag==0 && z->base<=nochunks && seqno<nochunks)
                {
                    sem_wait(&full);
                    pthread_mutex_lock(&mutex);
                    create_packet(&temp,send_buffer.array[0].buf,seqno,1,send_buffer.array[0].len);
                    send_buffer.array.erase(send_buffer.array.begin());
                    pthread_mutex_unlock(&mutex);
                    sem_post(&empty);
                    seqno=seqno+1;        
                    array.push_back(temp);
                    p=UDP_send(&temp,sockfd,(struct sockaddr * &)serveraddr,serverlen);
                    z->base=z->base+1;
                }
                else if(z->flag==1 && w<array.size())
                {
                    p=UDP_send(&array[w],sockfd,(struct sockaddr * &)serveraddr,serverlen);
                   	z->base=z->base+1;
                }
            }
            struct timeval tv;
            tv.tv_sec =5;
            tv.tv_usec =0;
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
            {   
                perror("Error");
            }
            n=UDP_receive(&d,sockfd,(struct sockaddr * &)serveraddr,serverlen);
            if (n < 0)
            {
                z->state=1;
                z->base=z->curr+1;
                z->flag=1;
                update_window(z,nochunks);
                continue;
            }
            else
            {
                z->ack=d.seq;
                z->rwnd=d.len;                
                if(z->ack <= z->curr)
                {
                    z->flag=-1;
                    z->state=2;                  
                    update_window(z,nochunks);
                }
                else if(z->ack > z->curr)
                {
                    popsize=z->ack-z->curr;
                    while(popsize--)
                    {
                        array.erase(array.begin());
                    }
                    z->state=3;
                    update_window(z,nochunks);
                    z->curr=z->ack;
                    z->flag=0;
                }
            }    
          
        }
        create_packet(&temp,buf,-1,1,0);
        p=UDP_send(&temp,sockfd,(struct sockaddr * &)serveraddr,serverlen); 
  
}

void client_file_check(char* filename,int sockfd,struct sockaddr * &serveraddr,int serverlen)
{
    int i;
    int n;
    datagram d;
    char buf[1024];
    bzero(buf, BUFSIZE);
    unsigned char c[MD5_DIGEST_LENGTH];
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];
    if (inFile == NULL)
    {
        printf ("%s can't be opened.\n", filename);
        return;
    }
    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, BUFSIZE, inFile)) != 0) 
    	MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);
    printf("MD5 check sum is : ");
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) 
    	printf("%02x", c[i]);
    printf (" %s\n", filename);
    fclose (inFile);
    unsigned char crev[MD5_DIGEST_LENGTH];
    n = recvfrom(sockfd, crev, MD5_DIGEST_LENGTH, 0,(struct sockaddr *) &serveraddr,(socklen_t*) &serverlen);
    if (n < 0)  error("ERROR in recvfrom");
    printf("Echo from server: ");
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) 
    	printf("%02x", crev[i]);
    printf (" %s\n", filename);
    if(strstr((const char*)c,(const char*)crev)!=NULL) 
    	printf("MD5  not Matched\n");
    else 
    	printf("MD5   Matched\n");
}

int main(int argc, char **argv)
{
    int nochunks;
    int size;
    char filename[BUFSIZE];
    int sockfd;
    if (argc < 3|| argc>3)
    {
        fprintf(stderr, "usage: %s <Host_for_server> <port>\n", argv[0]);
        exit(1);
    }
    serverlen = sizeof(serveraddr);
    client_details(&sockfd,filename,&nochunks,&size,argv);
    app_send(filename,nochunks,sockfd,(struct sockaddr * &)serveraddr,serverlen);
    client_file_check(filename,sockfd,(struct sockaddr * &)serveraddr,serverlen);


    return 0;
}
