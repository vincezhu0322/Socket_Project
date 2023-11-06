//
// Created by wenxuan on 10/23/23.
//
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define M_TCP_PORT 45986
#define M_IP "127.0.0.1"

std::string encrypt(const std::string &input) {
    std::string encrypted = "";
    for (char c : input) {
        if (isalpha(c)) {
            if (isupper(c))
                encrypted += (c - 'A' + 5) % 26 + 'A';
            else
                encrypted += (c - 'a' + 5) % 26 + 'a';
        } else if (isdigit(c)) {
            encrypted += (c - '0' + 5) % 10 + '0';
        } else {
            encrypted += c;
        }
    }
    return encrypted;
}

int main() {
    std::cout << "Client is up and running." << std::endl;
    
    int sockfd;
    struct sockaddr_in serverMAdd;
    char buffer[1024];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Client socket creation error." << std::endl;
        exit(EXIT_FAILURE);
    }

    serverMAdd.sin_family = AF_INET;
    serverMAdd.sin_port = htons(M_TCP_PORT);
    inet_pton(AF_INET, M_IP, &serverMAdd.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serverMAdd, sizeof(serverMAdd)) < 0) {
        std::cerr << "TCP Connection to the server failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    bool isAuthenticated = false;

    while (!isAuthenticated) {
        std::string username, password;
        std::cout << "Please enter the username: ";
        std::cin >> username;
        std::cout << "Please enter the password: ";
        std::cin >> password;

        std::string credential = encrypt(username) + "," + encrypt(password);
        send(sockfd, credential.c_str(), credential.length(), 0);
//        std::cout << credential << std::endl;
        std::cout << username << " sent an authentication request to the Main Server." << std::endl;

        int recvlen = recv(sockfd, buffer, sizeof(buffer), 0);
        buffer[recvlen] = '\0';

        std::string response(buffer);
        struct sockaddr_in localAddress;
        socklen_t addressLength = sizeof(localAddress);
        getsockname(sockfd, (struct sockaddr*)&localAddress, &addressLength);
        int clientPort = ntohs(localAddress.sin_port);

        if (response == "AUTH_SUCCESS") {
            std::cout << username <<  " received the result of authentication from Main Server using TCP over port "
                      << clientPort << ". Authentication is successful." << std::endl;
            isAuthenticated = true;

            //todo: phase III
            while (true) {
                std::cout << "Please enter book code to query: ";
                std::string bookCode;
                std::cin >> bookCode;
                send(sockfd, bookCode.c_str(), bookCode.length(), 0);
                std::cout << username << " sent the request to the Main Server." << std::endl;

                memset(buffer, 0, sizeof(buffer));
                recvlen = recv(sockfd, buffer, sizeof(buffer), 0);
                if (recvlen < 0) {
                    std::cerr << "Failed to receive data from Main server." << std::endl;
                    break;
                }
                buffer[recvlen] = '\0';
                std::string response(buffer);
                std::cout << "Response received from the Main Server on TCP port: " << clientPort << "." << std::endl;

                if (response == "BOOK_AVAILABLE") {
                    std::cout << "The requested book " << bookCode << " is available in the library." << std::endl;
                } else if (response == "BOOK_NOT_AVAILABLE") {
                    std::cout << "The requested book " << bookCode << " is NOT available in the library." << std::endl;
                } else {
                    std::cout << "Not able to find the book-code " << bookCode << " in the system." << std::endl;
                }
            }

        } else if (response == "USERNAME_NOT_FOUND") {
            std::cout << username << " received the result of authentication from Main Server using TCP over port "
                      << clientPort << ". Authentication failed: Username not found." << std::endl;
        } else if (response == "PASSWORD_NOT_MATCH") {
            std::cout << username << " received the result of authentication from Main Server using TCP over port "
                      << clientPort << ". Authentication failed: Password does not match." << std::endl;
        }
    }


    close(sockfd);
    return 0;
}
