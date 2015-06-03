#include <iostream>
#include <fstream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

class Socket
{
public:
 Socket(int port)
 {
	// creating socket
	int s = socket(PF_INET, SOCK_DGRAM,0);
	if(s<0)
	{
		perror("Socket not created");
	}
	//filling in server information
	sAddr.sin_family = AF_INET;
	sAddr.sin_addr.s_addr = INADDR_ANY;
	sAddr.sin_port = htons(port);
	//binding socket
	if(bind(s, (struct sockaddr *)&sAddr, sizeof(sAddr))<0)
    	{
    	  perror("Did not Bind");
    	}
 }
 int sendMessage(const char *message)
 {
	int numByteSent = 0;
	//unrealiable sending of information
	numByteSent = static_cast<int> (sendto(s,message,strlen(message), 0, (struct sockaddr *)&sAddr, sizeof(sAddr)));
	if(numByteSent < 0)
	{
		perror("Message Not Sent");
	}
	printf("%d", numByteSent);
 }
int receiveMessage(char *buf)
{
	socklen_t length = sizeof(sockaddr);
        int receivedBytes = static_cast<int> (recvfrom(s, buf, 2048, 0, (sockaddr*) &cAddr, &length));
        if (receivedBytes < 0)
        {}
        else
        {printf("Received %d bytes",receivedBytes);}
	return receivedBytes;
}

private:
        int s;
        sockaddr_in sAddr;
        sockaddr_in cAddr;
};


int main(int argc, char* argv[])
{
	printf("%s","fuck");
	//making sure there are 2 arguements
	if (argc < 2)
	{
		perror("not enough arguments");
	}
	char** charsReceived;
	Socket socket(atoi(argv[1]));
	int receivedBytes = -1;
	printf("%s","fuck");
	//*charsReceived = (char*) malloc(2048);
	while(receivedBytes<0)
	{
		receivedBytes = socket.receiveMessage((char*) &charsReceived);
	}
	printf("%d", receivedBytes);
}
