//client c++
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <iomanip>
#include <iostream>

#define IP "127.0.0.1"
#define PORT 9337

using namespace std;

int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0;
    unsigned char recvBuff[1024];
    struct sockaddr_in serv_addr; 




    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT); 
    char my_ip[] = IP;
    if(inet_pton(AF_INET, my_ip, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    } 

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return 1;
    } 
    char test_message[1024];
    unsigned char my_smg[1024];
    while (1)
    {
        cin.getline (test_message, 1023);

        if(strcmp(test_message, "stop") == 0)return 0;
        FILE * fp;
	cout<<argv[1]<<endl;
        fp = fopen(argv[1], "rb");

        int my_len = fread(my_smg, sizeof(char), 1023, fp);

        cout<<"The len is: "<<my_len<<endl;

        cout<<my_smg<<endl;

        int test_int = send(sockfd, my_smg, my_len, 0);
        if(test_int < 0){
            printf("error while sending message.\n");
            return 1;
        }

        fclose(fp);

        n = recv(sockfd, recvBuff, 2, MSG_WAITALL);
        int next_msg_len = 0;
        next_msg_len = (int)recvBuff[0];
        next_msg_len = (next_msg_len << 8) | (int) recvBuff[1];
        n = recv(sockfd, recvBuff, next_msg_len, MSG_WAITALL);

        printf("next msg len: %d\n", next_msg_len);

        if(n < 0 ){
            printf("error while reading data.\n");
            return 1;
        }
            recvBuff[n] = 0;
        printf("message len: %d\n", n);
        printf("received Message: %s\n",recvBuff);
/*
        
	for(int i=0; i<2; i++){
            sleep(60);
        }
*/	
    } 

    if(n < 0)
    {
        printf("\n Read error \n");
    } 

    return 0;
}
