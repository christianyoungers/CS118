//
//  sender.cpp
//  cs118project2
//
//  Created by Ian Cordero/Gagik Movsisyan.
//  Copyright (c) 2014 Ian Cordero, Gagik Movsisyan. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h> // close()
#include <sys/time.h> // timeval
#include "utilities.h"
#include <sys/stat.h> // stat, stat()
#include <fcntl.h> // fcntl()

// Socket ////////////////////////////////////////////////////////////////////

//
// Socket
// Opens a UDP/datagram socket on this host using the specified portNumber
//
class Socket
{
public:
    Socket(int portNumber)
    {
        socketAddressDataLength = sizeof(sockaddr);
        srand(toInt(time(NULL)));
        
        // Create socket
        if ((socketFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            error("Error: unable to open socket");
        }
        
        // Fill in server's (this program's) data
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        serverAddress.sin_port = htons(portNumber);
        
        // Bind socket
        if (bind(socketFileDescriptor, (sockaddr*) &serverAddress, socketAddressDataLength) < 0)
        {
            error("Error: bind failed");
        }
        
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
    // A reliable send that guarantees delivery of message using reliable data
    // transfer (RDT) principles. This function's return indicates that the
    // recipient has acknowledged receipt of the data, or an error occurred
    //
    int send(const char* message, int messageLength, int messageType)
    {
        if (messageType != DAT_P)
        {
            Packet sendPacket(messageType, 0, message, messageLength);
            
            int bytesSent = unreliableSend((char*) &sendPacket, sizeof(Packet));
            
            if (bytesSent < 0)
            {
                error("Error: could not send packet");
            }
            
            return bytesSent;
        }
        
        // If an incoming ACK is detected, process it. Otherwise, attempt to
        // send data.
        int offsetPacketSize = messageLength % PACKETSIZE;
        int numberOfPackets = (offsetPacketSize == 0) ? messageLength / PACKETSIZE : messageLength / PACKETSIZE + 1;
        if (numberOfPackets == 0)
        {
            numberOfPackets++;
        }
        Packet buffer;
        
        int bytesReceived, bytesSent, packetsSent = 0;
        
        bool acknowledgements[SEQUENCE];
        bool sentList[SEQUENCE];
        for (int i = 0; i < SEQUENCE; i++)
        {
            acknowledgements[i] = false;
            sentList[i] = false;
        }
        
        int current = 0;
        
        Packet packets[SEQUENCE];
        int loader = 0;
        
        int timers[SEQUENCE];
        
        while (packetsSent < numberOfPackets)
        {
            // (current + WINDOWSIZE) % SEQUENCE represents the current
            // window in the array 'acknowledgements'
            // The window only advances if the window's first n elements
            // are acknowledged
            if ((bytesReceived = unreliableReceive((char*) &buffer)) > 0)
            {
                // Received an ACK
                int ackNumber = buffer.sequenceNumber;
                
                if (buffer.type != ACK_P)
                {
                    error("Error: unexpected result from client");
                }
                
                std::cout << PREF_RDT << "Received " << buffer.length << "-byte, " << description(buffer.type) << " type packet, sequence no. " << buffer.sequenceNumber << std::endl;
                
                // Record acknowledgement
                acknowledgements[ackNumber] = true;
                packetsSent++;
            }
            else
            {
                // Attempt to send packet
                for (int i = 0; i < WINDOWSIZE && packetsSent + i < numberOfPackets; i++)
                {
                    int packetNumber = (i + current) % SEQUENCE;
                    
                    if (sentList[packetNumber] == false)
                    {
                        int loadNumber = (i * PACKETSIZE + PACKETSIZE < messageLength) ? PACKETSIZE : messageLength - i;
                        
                        packets[packetNumber] = Packet(messageType, packetNumber, &message[loader], loadNumber);
                        
                        loader += loadNumber;
                        
                        bytesSent = unreliableSend((char*) &packets[packetNumber], sizeof(Packet));
                        
                        if (bytesSent < 0)
                        {
                            error("Error sending packet");
                        }
                        else
                        {
                            std::cout << PREF_RDT << "Sent " << packets[packetNumber].length << "-byte, " << description(messageType) << " type packet, sequence no. " << packetNumber << std::endl;
                        }
                        
                        sentList[packetNumber] = true;
                        timers[packetNumber] = time();
                    }
                }
            }
            
            // Move window
            int moveAmount = 0;
            for (int i = 0; i < WINDOWSIZE && packetsSent + i < numberOfPackets; i++)
            {
                int packetNumber = (current + i) % SEQUENCE;
                
                if (acknowledgements[packetNumber] == true)
                {
                    acknowledgements[packetNumber] = false;
                    sentList[packetNumber] = false;
                    moveAmount++;
                }
                else
                {
                    break;
                }
            }
            current += moveAmount % SEQUENCE;
            
            // Check for timeouts -- resend packet if it has timed out
            for (int i = 0; i < WINDOWSIZE && packetsSent + i < numberOfPackets; i++)
            {
                int packetNumber = (i + current) % SEQUENCE;
                
                if (sentList[packetNumber] == false || acknowledgements[packetNumber] == true)
                {
                    continue;
                }
                
                if (timeout(timers[packetNumber]))
                {
                    bytesSent = unreliableSend((char*) &packets[packetNumber], sizeof(Packet));
                    
                    if (bytesSent < 0)
                    {
                        error("Error sending packet");
                    }
                    else
                    {
                        std::cout << PREF_RDT << "Timeout! Re-sent " << packets[packetNumber].length << "-byte, " << description(messageType) << " type packet, sequence no. " << packetNumber << std::endl;
                        
                    }
                    
                    timers[packetNumber] = time();
                }
            }
        }
        
        // Terminate connection
        Packet terminalPacket(TRL_P, 0, EMPTYMSG, 0);
        int terminalTimer;
        
        for (;;)
        {
            bytesSent = unreliableSend((char*) &terminalPacket, sizeof(Packet));
            
            if (bytesSent < 0)
            {
                error("Error sending terminal packet");
            }
            else
            {
                std::cout << PREF_RDT << "Sent " << terminalPacket.length << "-byte, " << description(TRL_P) << " type packet, sequence no. " << 0 << std::endl;
            }
            
            terminalTimer = time();
            
            int bytesReceived;
            
            while ((bytesReceived = unreliableReceive((char*) &buffer)) < 0)
            {
                if (timeout(terminalTimer))
                {
                    continue;
                }
            }
            
            if (buffer.type == TRL_P)
            {
                std::cout << PREF_RDT << "Received " << terminalPacket.length << "-byte, " << description(buffer.type) << " type packet, sequence no. " << buffer.sequenceNumber << std::endl;
                
                break;
            }
        }
        
        return messageLength;
    }
    
    //
    // receive()
    // A blocking receive that will block forever until data is received. Use
    // only for something like listening for connections.
    //
    int receive(char** buffer)
    {
        int bytesReceived = 0, packetsReceived = 0, bytesWritten = 0;
        int bufferSize = BUFSIZE;
        
        *buffer = (char*) checked_malloc(BUFSIZE);
        
        Packet packetBuffer;
        
        for (;;)
        {
            bytesReceived = unreliableReceive((char*) &packetBuffer);
            
            if (bytesReceived < 0)
            {
                continue;
            }
            else
            {
                // Expand buffer if necessary
                if (packetsReceived * PACKETSIZE + PACKETSIZE >= bufferSize)
                {
                    bufferSize *= 2;
                    *buffer = (char*) checked_realloc(*buffer, bufferSize);
                }
                
                // For now, assume packets arrive in order
                memcpy((*buffer) + packetsReceived * PACKETSIZE, packetBuffer.data, packetBuffer.length);
                
                bytesWritten += packetBuffer.length;
                packetsReceived++;
                
                std::cout << PREF_RDT << "Received " << packetBuffer.length << "-byte, " << description(packetBuffer.type) << " type packet, sequence no. " << packetBuffer.sequenceNumber << std::endl;
                
                break;
            }
        }
        
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
        int bytesSent = toInt(sendto(socketFileDescriptor, message, messageLength, 0, (sockaddr*) &clientAddress, socketAddressDataLength));
        
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
        int bytesReceived = toInt(recvfrom(socketFileDescriptor, buffer, BUFSIZE, 0, (sockaddr*) &clientAddress, &socketAddressDataLength));
        
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
    sockaddr_in serverAddress, clientAddress;
    socklen_t socketAddressDataLength;
};

// Main //////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    std::string requestedFileName;
    
    // Reject if not enough arguments
    if (argc < 2)
    {
        usage(argv[0]);
    }
    
    // Make reliable socket
    Socket socket(atoi(argv[1]));
    
    std::cout << PREF_APP << "Created reliable socket" << std::endl;
    
    char* buffer;
    
    for (;;)
    {
        // Listen for file request
        std::cout << PREF_APP << "Listening for connections..." << std::endl;
        
        int bytesReceived = socket.receive(&buffer);
        
        if (bytesReceived > 0)
        {
            char* filename = new char[bytesReceived + 1];
            memcpy((void*) filename, (void*) buffer, bytesReceived);
            std::cout << PREF_APP << "Received file request: \"" << filename << "\"" << std::endl;
            
            requestedFileName = filename;
        }
        
        std::cout << PREF_APP << "Now beginning upload..." << std::endl;
        
        std::ifstream is;
        
        is.open(requestedFileName.c_str(), std::ifstream::in);
        
        if (!is.is_open())
        {
            error("Error: could not open file");
        }
        
        int totalFileSize = 0;
        struct stat results;
        
        if (stat(requestedFileName.c_str(), &results) == 0)
        {
            totalFileSize = toInt(results.st_size);
        }
        else
        {
            error("Error: could not read file size");
        }
        
        int numberOfBytesToRead = totalFileSize;
        
        if (!is.read(buffer, numberOfBytesToRead))
        {
            error("Error: reading error occurred");
        }
        
        socket.send(buffer, numberOfBytesToRead, DAT_P);
        
        is.close();
        
        // Terminate connection
        std::cout << PREF_APP << "File sent. Terminating connection..." << std::endl;
    }
}