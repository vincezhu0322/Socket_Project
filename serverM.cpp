//
// Created by wenxuan on 10/22/23.
//
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <unistd.h>
#include <fstream>
#include <thread>

#define M_UDP_PORT 44986
#define M_TCP_PORT 45986
#define S_UDP_PORT 41986
#define L_UDP_PORT 42986
#define H_UDP_PORT 43986

std::map<std::string, int> booksStatus;
std::map<std::string, std::string> members;
int udpSocket;

struct sockaddr_in serverSAdd, serverLAdd, serverHAdd;

void setupServerAddresses() {
    memset(&serverSAdd, 0, sizeof(serverSAdd));
    memset(&serverLAdd, 0, sizeof(serverLAdd));
    memset(&serverHAdd, 0, sizeof(serverHAdd));

    serverSAdd.sin_family = AF_INET;
    serverSAdd.sin_port = htons(S_UDP_PORT);

    serverLAdd.sin_family = AF_INET;
    serverLAdd.sin_port = htons(L_UDP_PORT);

    serverHAdd.sin_family = AF_INET;
    serverHAdd.sin_port = htons(H_UDP_PORT);

    inet_pton(AF_INET, "127.0.0.1", &serverSAdd.sin_addr);
    inet_pton(AF_INET, "127.0.0.1", &serverLAdd.sin_addr);
    inet_pton(AF_INET, "127.0.0.1", &serverHAdd.sin_addr);
}

void udpSendToBackend(int serverPort, const std::string& bookCode) {
    struct sockaddr_in serverAdd;
    memset(&serverAdd, 0, sizeof(serverAdd));
    serverAdd.sin_family = AF_INET;
    serverAdd.sin_port = htons(serverPort);
    serverAdd.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (sendto(udpSocket, bookCode.c_str(), bookCode.size(), 0,
               (struct  sockaddr*)&serverAdd, sizeof(serverAdd)) < 0) {
        std::cerr << "Error sending bookCode to backend server." << std::endl;
    }
}

std::string udpReceiveFromBackend(int serverPort) {
    struct sockaddr_in fromAdd;
    socklen_t fromAddLen = sizeof(fromAdd);
    char buffer[1024];

    ssize_t recvBytes = recvfrom(udpSocket, buffer, sizeof(buffer) - 1, 0,
                                 (struct sockaddr*)&fromAdd, &fromAddLen);
    if (recvBytes < 0) {
        std::cerr << "Error receiving from server." << std::endl;
        return "";
    }

    buffer[recvBytes] = '\0';

    if (ntohs(fromAdd.sin_port) != serverPort) {
        std::cerr << "Received Book Status from an unexpected port." << std::endl;
        return "";
    }

    return std::string(buffer);
}

void checkBookAvailability(int childsockfd, const std::string& bookCode) {
    std::cout << "Main Server received the book request from client using TCP over port " << M_TCP_PORT << "." << std::endl;
    char serverHead = toupper(bookCode[0]);
    int serverPort;
    std::string serverString;
    switch (serverHead) {
        case 'S':
            serverPort = S_UDP_PORT;
            serverString = "S";
            break;
        case 'L':
            serverPort = L_UDP_PORT;
            serverString = "L";
            break;
        case 'H':
            serverPort = H_UDP_PORT;
            serverString = "H";
            break;
        default:
            std::cout << "Did not find " << bookCode << " in the book code list." << std::endl;
            send(childsockfd, "Book code not found.", 19, 0);
            return;
    }

    std::cout << "Found " << bookCode << " located at Server " << serverString << ". Send to Server " << serverString << "." << std::endl;
    udpSendToBackend(serverPort, bookCode); // Todo

    std::string bookStatus = udpReceiveFromBackend(serverPort); // Todo
    std::cout << "Main Server received from server " << serverString << " the book status result using UDP over port "
              << M_UDP_PORT << ":" << std::endl
              << "Number of books " << bookCode << " available is: " << bookStatus << "." << std::endl;

    if (bookStatus != "BOOK_NOT_FOUND") {
        if (bookStatus == "0") {
            bookStatus = "BOOK_NOT_AVAILABLE";
        } else {
            bookStatus = "BOOK_AVAILABLE";
        }
    }
    send(childsockfd, bookStatus.c_str(), bookStatus.length(), 0);
    std::cout << "Main Server sent the book status to the client." << std::endl;
}

void udpListen() {
    int sockfd;
    struct sockaddr_in serverAdd, clientAdd;
    char buffer[1024];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "ServerM UDP socket creation error." << std::endl;
        exit(EXIT_FAILURE);
    }

    memset(&serverAdd, 0, sizeof(serverAdd));
    serverAdd.sin_family = AF_INET;
    serverAdd.sin_addr.s_addr = INADDR_ANY;
    serverAdd.sin_port = htons(M_UDP_PORT);

    if (bind(sockfd, (struct sockaddr *) &serverAdd, sizeof(serverAdd)) < 0) {
        std::cerr << "ServerM UDP socket binding error." << std::endl;
        exit(EXIT_FAILURE);
    }

    while (true) {
        socklen_t clientLen = sizeof(clientAdd);
        int recvlen = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &clientAdd, &clientLen);
        if (recvlen > 0) {
            buffer[recvlen] = 0;
            std::string data(buffer);
            if (data.find("END_OF_LIST") == 0) {
                char serverNum = data[data.length() - 1];
                std::cout << "Main Server received the book code list from server" << serverNum
                          << " using UDP over port " << M_UDP_PORT << "." << std::endl;
//                for (const auto& [bookCode, count] : booksStatus) {
//                    std::cout << "Book Code: " << bookCode << ", Count: " << count << std::endl;
//                }
                continue;
            }
            size_t splitter = data.find(",");
            std::string bookCode = data.substr(0, splitter);
            int status = std::stoi(data.substr(splitter + 1));
            booksStatus[bookCode] = status;
        } else if (recvlen < 0) {
            std::cerr << "ServerM recvfrom error." << std::endl;
        }
    }

    close(sockfd);
}

void readMembers() {
    std::ifstream inFile("member.txt");
    std::string username, password;
    while (inFile >> username >> password) {
        username.pop_back();
        members[username] = password;
    }
    inFile.close();
    std::cout << "Main Server loaded the member list." << std::endl;
}

bool authenticateUser(int childsockfd, const std::string &input) {
    size_t splitter = input.find(",");
    std::string username = input.substr(0, splitter);
    std::string password = input.substr(splitter + 1);
    std::cout << "Main Server received the username " << username
            << " and password " << password << " from the client using TCP over port "
            << M_TCP_PORT << "." << std::endl;

    std::string response;
    if (members.find(username) == members.end()) {
        response = "USERNAME_NOT_FOUND";
        std::cout << username << " is not registered. Send a reply to the client." << std::endl;
    } else if (members[username] == password) {
        response = "AUTH_SUCCESS";
        std::cout << "Password " << password << " matches the username. Send a reply to the client." << std::endl;
    } else {
        response = "PASSWORD_NOT_MATCH";
        std::cout << "Password " << password << " does not match the username. Send a reply to the client." << std::endl;
    }
    send(childsockfd, response.c_str(), response.length(), 0);

    return response == "AUTH_SUCCESS";
}

void tcpListen() {
    int sockfd, childsockfd;
    struct sockaddr_in serverMAdd, clientAdd;
    char buffer[1024];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "ServerM TCP socket creation error." << std::endl;
        exit(EXIT_FAILURE);
    }

    memset(&serverMAdd, 0, sizeof(serverMAdd));
    serverMAdd.sin_family = AF_INET;
    serverMAdd.sin_addr.s_addr = INADDR_ANY;
    serverMAdd.sin_port = htons(M_TCP_PORT);

    if (bind(sockfd, (struct sockaddr*)&serverMAdd, sizeof(serverMAdd)) < 0) {
        std::cerr << "ServerM TCP socket binding error." << std::endl;
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 3) < 0) {
        std::cerr << "ServerM TCP socket listening error." << std::endl;
        close(sockfd);
        exit(EXIT_FAILURE);
    }

//    std::cout << "TCP Server listening on port " << M_TCP_PORT << std::endl;

    socklen_t clientLen = sizeof(clientAdd);

    while (true) {
        childsockfd = accept(sockfd, (struct sockaddr*)&clientAdd, &clientLen);
        if (childsockfd < 0) {
            std::cerr << "Error accepting TCP connection." << std::endl;
            continue;
        }

        bool isAuthenticated = false;

        while (!isAuthenticated) {
            int recvlen = read(childsockfd, buffer, 1024);
            if (recvlen < 0) {
                std::cerr << "Error reading from TCP." << std::endl;
                break;
            } else if (recvlen == 0) {
                close(childsockfd);
                break;
            } else {
                buffer[recvlen] = '\0';
                isAuthenticated = authenticateUser(childsockfd, buffer);
            }
        }

        while (isAuthenticated) {
            int recvlen = read(childsockfd, buffer, 1024);
            if (recvlen < 0) {
                std::cerr << "Error reading from TCP." << std::endl;
                close(childsockfd);
                isAuthenticated = false;
            } else if (recvlen == 0) {
                close(childsockfd);
            } else {
                buffer[recvlen] = '\0';
                std::string bookCode(buffer);

                checkBookAvailability(childsockfd, bookCode); // todo
            }
        }

    }

    close(childsockfd);
}

int main() {
    std::cout << "Main Server is up and running." << std::endl;
    readMembers();
//    for (const auto& [username, passward] : members) {
//        std::cout << "username: " << username << ", password: " << passward << std::endl;
//    }
    setupServerAddresses();
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0) {
        std::cerr << "Error creating UDP socket." << std::endl;
        return 1;
    }
    struct sockaddr_in mainServerAddr;
    memset(&mainServerAddr, 0, sizeof(mainServerAddr));
    mainServerAddr.sin_family = AF_INET;
    mainServerAddr.sin_port = htons(M_UDP_PORT);
    mainServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udpSocket, (struct sockaddr *)&mainServerAddr, sizeof(mainServerAddr)) < 0) {
        std::cerr << "Error binding UDP socket to port." << std::endl;
        close(udpSocket);
        return 1;
    }
//    std::thread udpThread(udpListen);
    std::thread tcpThread(tcpListen);

//    udpThread.join();
    tcpThread.join();

    close(udpSocket);

    return 0;
}
