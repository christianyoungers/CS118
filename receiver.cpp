//
//  receiver.cpp
//  cs118project2
//
//  Created by Ian Cordero/Gagik Movsisyan.
//  Copyright (c) 2014 Ian Cordero, Gagik Movsisyan. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h> // socket()
#include <netinet/in.h>
#include <cstring>
#include <unistd.h> // close()
#include <netdb.h> // gethostbyname()
#include <sys/time.h> // timeval
#include "utilities.h"
#include <fcntl.h> // fcntl()

// Socket ////////////////////////////////////////////////////////////////////

//
// Socket
// Opens a UDP/datagram socket on this host, pointing to 'serverName', port
// 'portNumber'
//
class Socket
{
public:
    Socket(char* serverName, int portNumber)
    {
        socketAddressDataLength = sizeof(sockaddr);
        srand(time());
        
        // Create socket
        if ((socketFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            error("Error: unable to open socket");
        }
        
        // Fill in client's (this program's) data
        clientAddress.sin_family = AF_INET;
        clientAddress.sin_addr.s_addr = INADDR_ANY;
        clientAddress.sin_port = htons(0); // let OS choose socket
        
        // Bind socket
        if (bind(socketFileDescriptor, (sockaddr*) &clientAddress, socketAddressDataLength) < 0)
        {
            error("Error: bind failed");
        }
        
        // Fill in server's data
        memset((char*) &serverAddress, 0, sizeof(sockaddr));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(portNumber);
        
        senderInfo = gethostbyname(serverName);
        if (senderInfo == NULL)
        {
            error("Error: could not obtain server address");
        }
        
        memcpy((void*) &serverAddress.sin_addr, senderInfo->h_addr_list[0], senderInfo->h_length);
        
        // Configure settings
        int flags = fcntl(socketFileDescriptor, F_GETFL, 0);
        if (flags < 0)
        {
            error("Error: could not configure socket");
        }
        fcntl(socketFileDescriptor, F_SETFL, flags | O_NONBLOCK);
    }
    
    //
    // send()
    // A simple and best-effort send that does NOT guarantee delivery of
    // message. This is acceptable because the burden of implementing
    // a reliable connection is on the corresponding sender/server
    //
    int send(const char* message, int messageLength, int messageType)
    {
        Packet sendPacket(messageType, 0, message, messageLength);
        
        int bytesSent = unreliableSend((char*) &sendPacket, sizeof(Packet));
        
        if (bytesSent < 0)
        {
            error("Error: could not send packet");
        }
        
        return bytesSent;
    }
    
    //
    // receive()
    // A reliable retrieval of data that will block until data is received.
    // To be called when the application-level program is expecting data. This
    // function's return indicates that data was received, or an error occurred
    //
    int receive(char** buffer)
    {
        int bytesReceived = 0, packetsReceived = 0, bytesWritten = 0, packetsStored = 0, current = 0;
        int bufferSize = BUFSIZE;
        
        *buffer = (char*) checked_malloc(BUFSIZE);
        
        // DEBUG
        //std::cerr << "Buffer is size " << bufferSize << std::endl;
        
        Packet temporaryStore[SEQUENCE];
        bool receivals[SEQUENCE];
        for (int i = 0; i < SEQUENCE; i++)
        {
            receivals[i] = false;
        }
        
        for (;;)
        {
            Packet packetBuffer;
            
            bytesReceived = unreliableReceive((char*) &packetBuffer);
            
            if (bytesReceived < 0)
            {
                continue;
            }
            else
            {
                // Record receipt
                packetsReceived++;
                
                // Expand buffer if necessary
                while (packetsStored * PACKETSIZE + PACKETSIZE >= bufferSize)
                {
                    bufferSize *= 2;
                    *buffer = (char*) checked_realloc(*buffer, bufferSize);
                    
                    // DEBUG
                    //std::cerr << "Buffer is size " << bufferSize << std::endl;
                }
                
                if (packetBuffer.type != DAT_P)
                {
                    std::cout << PREF_RDT << "Received " << packetBuffer.length << "-byte, " << description(packetBuffer.type) << " type packet, sequence no. " << packetBuffer.sequenceNumber << std::endl;
                    
                    if (packetBuffer.type == TRL_P)
                    {
                        packetBuffer = Packet(TRL_P, 0, EMPTYMSG, 0);
                        
                        int bytesSent = unreliableSend((char*) &packetBuffer, sizeof(Packet));
                        
                        if (bytesSent < 0)
                        {
                            error("Error sending packet");
                        }
                    }
                    else
                    {
                        memcpy(*buffer, packetBuffer.data, packetBuffer.length);
                        bytesWritten += packetBuffer.length;
                    }
                    
                    break;
                }
                else
                {
                    // Store data
                    
                    // DEBUG
                    //std::cerr << "Storing " << packetBuffer.length << " bytes of data at *buffer, offset " << packetsStored * PACKETSIZE << std::endl;
                    
                    // DEBUG
                    /*std::cerr << "Packet contains:" << std::endl;
                    for (int i = 0; i < packetBuffer.length; i++)
                    {
                        std::cerr << packetBuffer.data[i];
                    }
                    std::cerr << std::endl;*/
                    
                    char* ptr = (*buffer) + packetsStored * PACKETSIZE;
                    
                    /*std::cerr << "*buffer contains:" << std::endl;
                    for (char* c = *buffer; c != ptr; c++)
                    {
                        std::cerr << *c;
                    }
                    std::cerr << std::endl;*/
                    
                    memcpy(ptr, packetBuffer.data, packetBuffer.length);
                    bytesWritten += packetBuffer.length;
                    packetsStored++;
                    current = current + 1 % SEQUENCE;
                    
                    // Send ACK
                    std::cout << PREF_RDT << "Received " << packetBuffer.length << "-byte, " << description(packetBuffer.type) << " type packet, sequence no. " << packetBuffer.sequenceNumber << std::endl;
                    
                    Packet sendPacket(ACK_P, packetBuffer.sequenceNumber, EMPTYMSG, 0);
                    
                    std::cout << PREF_RDT << "Sent " << sendPacket.length << "-byte, " << description(sendPacket.type) << " type packet, sequence no. " << sendPacket.sequenceNumber << std::endl;
                    
                    int bytesSent = unreliableSend((char*) &sendPacket, sizeof(Packet));
                    
                    if (bytesSent < 0)
                    {
                        error("Error sending packet");
                    }
                }
            }
        }
        
        // DEBUG
        /*std::cerr << "It's a wrap. *buffer must contain:" << std::endl;
        for (int i = 0; i < bytesWritten; i++)
        {
            std::cerr << (*buffer)[i];
        }
        std::cerr << std::endl;*/
        
        // DEBUG
        std::cerr << "packetsStored: " << packetsStored << std::endl;
        
        return bytesWritten;
    }
    
    //
    // unreliableSend()
    // Sends message over UDP protocol
    // Does NOT guarantee delivery of message
    //
    int unreliableSend(const char* message, int messageLength)
    {
        // Simulate loss/corruption
        /*if (randomInt(100) < toInt(P_L * 100.0) || randomInt(100) < toInt(P_C * 100.0))
        {
            return messageLength;
        }*/
        
        // Send filename request
        int bytesSent = toInt(sendto(socketFileDescriptor, message, messageLength, 0, (sockaddr*) &serverAddress, socketAddressDataLength));
        
        if (bytesSent < 0)
        {
            error("Error: sendto() failed");
        }
        else
        {
            std::cout << PREF_UDT << "Sent " << bytesSent << "-byte UDP datagram" << std::endl;
        }
        
        return bytesSent;
    }
    
    //
    // unreliableReceive()
    // Receives message, if any, over UDP protocol, which does NOT guarantee
    // delivery of messages
    //
    int unreliableReceive(char* buffer)
    {
        int bytesReceived = toInt(recvfrom(socketFileDescriptor, buffer, BUFSIZE, 0, (sockaddr*) &serverAddress, &socketAddressDataLength));
    
        if (bytesReceived < 0)
        {
        }
        else
        {
            std::cout << PREF_UDT << "Received " << bytesReceived << "-byte UDP datagram" << std::endl;
        }
        
        return bytesReceived;
    }

    ~Socket()
    {
        close(socketFileDescriptor);
    }
    
private:
    int socketFileDescriptor;
    sockaddr_in clientAddress, serverAddress;
    hostent* senderInfo;
    socklen_t socketAddressDataLength;
};

// Main //////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    // Reject if not enough arguments
    if (argc < 4)
    {
        usage(argv[0]);
    }
    
    // Make receiving Socket
    Socket socket(argv[1], atoi(argv[2]));
    
    std::cout << PREF_APP << "Created reliable socket" << std::endl;
    
    // Establish buffer
    char* buffer;
    
    // Send file request to server/sender
    socket.send(argv[3], toInt(strlen(argv[3])), REQ_P);
    
    std::cout << PREF_APP << "Sent file request: \"" << argv[3] << "\"" << std::endl;

    // Receive file
    std::cout << PREF_APP << "Waiting for server..." << std::endl;
    
    std::ofstream os;
    
    os.open(argv[3], std::ofstream::out);
    
    if (!os.is_open())
    {
        error("Error: could not save file");
    }
    
    int numberOfBytesToWrite = socket.receive(&buffer);
    
    if (numberOfBytesToWrite < 0)
    {
        error("Error: could not receive data");
    }
    
    if (!os.write(buffer, numberOfBytesToWrite))
    {
        error("Error: writing error occurred");
    }
    
    // DEBUG
    /*std::cerr << "Wrote:" << std::endl;
    for (int i = 0; i < numberOfBytesToWrite; i++)
    {
        std::cerr << buffer[i];
    }
    std::cerr << std::endl;*/
    
    free(buffer);
    os.close();
    
    // Terminate connection
    std::cout << PREF_APP << "File received. Terminating connection..." << std::endl;
}