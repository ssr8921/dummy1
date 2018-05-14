#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <bits/stdc++.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <openssl/md5.h>

using namespace std;

#define BUFSIZE 1024

sem_t empty,full;
pthread_mutex_t mutex;
struct sockaddr_in clientaddr; 
int clientlen;
int serverlen;
int N=8;

typedef struct data{
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


void error(char *msg)
{
  perror(msg);
  exit(1);
}


int  UDP_receive(datagram* d,int sockfd,struct sockaddr * &clientaddr,int clientlen)
{
	int n;
	n=recvfrom(sockfd, d, sizeof(datagram), 0,(struct sockaddr *) &clientaddr, (socklen_t*)&clientlen);
	if (n ==-1)	
		error("ERROR in recvfrom");
	return n;
}

int  UDP_send(datagram* d,int n,int sockfd,struct sockaddr * &clientaddr,int clientlen)
{
	int n1 ;
	n1=sendto(sockfd, d, sizeof(*d), 0,(struct sockaddr *) &clientaddr, clientlen);
	if (n1 ==-1) 
		 error("ERROR in sendto"); 
   //return n1;
}

void create_packet(datagram *d,char* buf,int i,int j,int k)
{
    bzero(d->buf, BUFSIZE);
    if(k==-1) 
    	d->len=strlen(buf);
    else 
    	d->len=k;
    d->seq=i;
    d->type=j;
    memcpy(d->buf,buf,d->len);
}

void * mbuf_func(void * ptr)
{
    buffer *k=(buffer *)ptr;
    datagram temp;
    int fs_block_sz;
    char buf[1024];
   
    do
    {
	    sem_wait(&full);
        pthread_mutex_lock(&mutex);
        int write_sz ;
        write_sz = fwrite(k->array[0].buf,sizeof(char),k->array[0].len, k->fd);
        
        k->array.erase(k->array.begin());
		k->nop=k->nop-1;
		if(write_sz < k->array[0].len)
			error("File write failed on server.\n");
		
		pthread_mutex_unlock(&mutex);
        sem_post(&empty);
		if(k->nop==0)
        {
        	fclose(k->fd);
        	break;
        }
    }while(1);
    

}

void server_details(int* sockfd1,char* filename,int* nochunks1,int* size1,char** argv)
{

	int portno;
	int sockfd;
	int size;
	int nochunks; 
	struct sockaddr_in serveraddr;
	char buf[1024];
	int optval;
	optval=1;
	int n; 
	portno = atoi(argv[1]);
	(sockfd) = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)  
		error("ERROR opening socket");
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int));
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);
	if (bind(sockfd, (struct sockaddr *) &serveraddr,sizeof(serveraddr)) < 0)	
                error("ERROR on binding");
	datagram d;
	bzero(d.buf, 1024);
	n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	printf("server received %d/%d bytes: %s\n", strlen(d.buf), n, d.buf);
	
	strcpy(filename,d.buf);
	int i;
	for(i=0;filename[i]!='\n';i++);
	filename[i]='\0';
	
	n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	
	bzero(d.buf, 1024);
	n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	printf("Server received %d/%d bytes: %s\n", strlen(d.buf), n, d.buf);
	char sizename[1024];
	strcpy(sizename,d.buf);
	(size) = atoi(sizename);
	
	n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	bzero(d.buf, 1024);
	n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	printf("Server received %d/%d bytes: %s\n", strlen(d.buf), n, d.buf);

	char chunks[1024];
	strcpy(chunks,d.buf);
	(nochunks)= atoi(chunks);
	n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);

	(*sockfd1)=sockfd;
	
	(*nochunks1)=nochunks;
	(*size1)=size;


}

void app_recv(char* filename,int nochunks,float dp,int sockfd,struct sockaddr * &clientaddr,int clientlen)
{
    datagram d;
    char buf[BUFSIZE];
    int n;
    FILE *fr = fopen(filename, "wb");
    if(fr == NULL)  
         printf("File %s cannot be opened file on server.\n", filename);
    buffer recv_buffer;
    pthread_t mbuf;
    sem_init(&full,0,0);
    sem_init(&empty,0,N);
    recv_buffer.fd=fr;
    recv_buffer.nop=nochunks;
    pthread_create(&mbuf,NULL,mbuf_func,(void *)&recv_buffer);
    int count=0;
    int rwnd=0;
    int left=recv_buffer.nop;
    while(count < nochunks || count>nochunks)
	{
		bzero(d.buf, BUFSIZE);
		n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
		float r ;
		r = ((double) rand() / (RAND_MAX));
		if(d.seq == count)
		{
		        sem_wait(&empty);
	            pthread_mutex_lock(&mutex);
        	    recv_buffer.array.push_back(d);
	    	    rwnd=N;
	    	    rwnd=rwnd-recv_buffer.array.size();
	    	    left=recv_buffer.nop;
    		    pthread_mutex_unlock(&mutex);
	  		    sem_post(&full);
	  		    count=count+1;				
		}
		
		if(r<=dp)
		{	
			bzero(d.buf, 1024);
			int ind;
			ind=count-1;
		    create_packet(&d,buf,ind,0,rwnd);
			n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);
		}
		
	}
	while(1)
	{	
		bzero(d.buf, BUFSIZE);
		n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
		if(d.seq==-1) 
			return;
		bzero(d.buf, BUFSIZE);
	    int jil;
	    jil = count-1;
	    create_packet(&d,buf,jil,0,0);
		n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	}
	pthread_join(mbuf,NULL);
}

void server_file_check(char* filename,int sockfd,struct sockaddr * &clientaddr,int clientlen)
{
	datagram d;
	int n;
	bzero(d.buf, BUFSIZE);
	int i;
	FILE *inFile = fopen (filename, "rb");
	MD5_CTX mdContext;
	if (inFile == NULL)
	{
	    printf ("%s can't be opened.\n", filename);
	    return;
	}
	int bytes;
	char data[1024];
	unsigned char c[MD5_DIGEST_LENGTH];
	MD5_Init (&mdContext);
	while ((bytes = fread (data, 1, BUFSIZE, inFile)) != 0)
	{
	    MD5_Update (&mdContext, data, bytes);
	}
	MD5_Final (c,&mdContext);
	printf("MD5 check sum is : ");
	for( i=0 ; i < MD5_DIGEST_LENGTH; i++) 
		printf("%02x", c[i]);
	printf (" %s\n", filename);
	fclose (inFile);
	n = sendto(sockfd, c, MD5_DIGEST_LENGTH, 0,(struct sockaddr *) &clientaddr,(socklen_t) clientlen);
	if (n < 0)  
		error("ERROR in sendto");
	return;
}

int main(int argc, char **argv)
{
  

	int nochunks;
	int size;
	int sockfd;
	char filename[1024];
	if (argc > 3 || argc <3 )
	{
		fprintf(stderr, "usage: %s <port_for_server> <drop-probability>\n", argv[0]);
		exit(1);
	}
	float dp;
	dp=atof(argv[2]);
	clientlen = sizeof(clientaddr);
	server_details(&sockfd,filename,&nochunks,&size,argv);
	app_recv(filename,nochunks,dp,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	server_file_check(filename,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	return 0;
}
