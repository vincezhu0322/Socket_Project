//
// Created by wenxuan on 10/18/23.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define S_PORT 41986
#define M_PORT 44986
#define M_IP "127.0.0.1"

std::map<std::string, int> scienceStatus;

bool readStatus() {
    std::ifstream file("science.txt");
    if (!file.is_open()) {
        std::cerr << "No file science.txt found." << std::endl;
        return false;
    }

    std::string line;
    while (getline(file, line)) {
        std::stringstream ss(line);
        std::string bookCode;
        int count;
        getline(ss, bookCode, ',');
        ss >> count;
        scienceStatus[bookCode] = count;
    }

//    for (const auto& [bookCode, count] : status) {
//        std::cout << "Book Code: " << bookCode << ", Count: " << count << std::endl;
//    }

    file.close();
    return true;
}

std::string processQuery(const std::string& bookCode) {
    if (scienceStatus.find(bookCode) == scienceStatus.end()) {
        return "BOOK_NOT_FOUND";
    } else if (scienceStatus[bookCode] > 0) {
        scienceStatus[bookCode]--;
        return std::to_string(scienceStatus[bookCode] + 1);
    } else {
        return "0";
    }
}

bool sendToMainServer() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "ServerS UDP socket creation error." << std::endl;
        return false;
    }
    struct sockaddr_in serverMAdd;
    memset(&serverMAdd, 0, sizeof(serverMAdd));
    serverMAdd.sin_family = AF_INET;
    serverMAdd.sin_port = htons(M_PORT);
    inet_pton(AF_INET, M_IP, &serverMAdd.sin_addr);
    for (const auto& [bookCode, count] : scienceStatus) {
        std::string message = bookCode + "," + std::to_string(count);
        sendto(sockfd, message.c_str(), message.size(), 0,
               (struct sockaddr *)&serverMAdd, sizeof(serverMAdd));
    }
    std::string terminator = "END_OF_LIST_S";
    sendto(sockfd, terminator.c_str(), terminator.size(), 0,
           (struct sockaddr *)&serverMAdd, sizeof(serverMAdd));
    close(sockfd);
    return true;
}

int main() {
    if (!readStatus()) {
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "ServerS UDP socket creation error." << std::endl;
        return 1;
    }

    struct sockaddr_in serverSAddress;
    memset(&serverSAddress, 0, sizeof(serverSAddress));
    serverSAddress.sin_family = AF_INET;
    serverSAddress.sin_port = htons(S_PORT);
    serverSAddress.sin_addr.s_addr = INADDR_ANY;

    int bound = bind(sockfd, (struct sockaddr *)&serverSAddress, sizeof(serverSAddress));
    if (bound < 0) {
        std::cerr << "ServerS UDP socket bind error." << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "Server S is up and running using UDP on port " << S_PORT << "." << std::endl;

//    sendToMainServer();

    while (true) {
        char buffer[1024];
        struct sockaddr_in sourceAdd;
        socklen_t sourceAddLen = sizeof(sourceAdd);

        ssize_t recvBytes = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                     (struct sockaddr*)&sourceAdd, &sourceAddLen);
        if (recvBytes > 0) {
            buffer[recvBytes] = '\0';
            std::string bookCode(buffer);
            std::string response = processQuery(bookCode);

            std::cout << "Server S received " << bookCode << " code from the Main Server." << std::endl;

            sendto(sockfd, response.c_str(), response.length(), 0,
                   (struct sockaddr*)&sourceAdd, sizeof(sourceAdd));

            std::cout << "Server S finished sending the availability status of code " << bookCode <<
            " to the Main Server using UDP on port " << S_PORT << "." << std::endl;
        }
    }

    close(sockfd);
    return 0;
}