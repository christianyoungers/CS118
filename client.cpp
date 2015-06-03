#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h> // socket()
#include <netinet/in.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <errno.h>
#include <string>
#include <unistd.h> // close()
#include <netdb.h> // gethostbyname()
#include <sys/time.h> // timeval
#include <fcntl.h> // fcntl()


class Socket
{
public:
 Socket(int port, const char* server)
 {
	// creating socket
	s = socket(PF_INET, SOCK_DGRAM,0);
	if(s<0)
	{
		perror("Socket not created");
	}
	//filling in server information
	cAddr.sin_family = AF_INET;
	cAddr.sin_addr.s_addr = INADDR_ANY;
	cAddr.sin_port = htons(0); //since it is the client the OS can choose the port
	//binding the socket to a port
	if(bind(s, (struct sockaddr *)&cAddr, sizeof(cAddr))<0)
    	{
    	  perror("Did not Bind");
    	}
	//server data
	memset((char*) &sAddr, 0, sizeof(sockaddr)); //allocating memory
	sAddr.sin_family = AF_INET;
	sAddr.sin_port = htons(port);
	//setting up host info
	h = gethostbyname(server);
 }
int sendMessage(const char *message)
 {
        int numByteSent = 0;
        //unrealiable sending of information
       numByteSent = sendto(s,message,strlen(message), 0, (struct sockaddr *)&sAddr, sizeof(sAddr));
        if(numByteSent < 0)
        {
                perror("Message Not Sent");
        }
        printf("%d", numByteSent);
 }

private:
	int s;
	sockaddr_in cAddr;
	sockaddr_in sAddr;
	hostent* h;
};
//takes in server name, port number, message
int main(int argc, char* argv[])
{
	//making sure there are 2 arguements
	if (argc < 4)
	{
		perror("Fucking A");
//		usage(argv[0]);//common for when incorrect amount of arguments occur
	}
	Socket socket(atoi(argv[2]), argv[1]);// port number ,server name
	socket.sendMessage(argv[3]);
	return 0;
}


