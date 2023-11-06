//
// Created by wenxuan on 10/23/23.
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

#define H_PORT 43986
#define M_PORT 44986
#define M_IP "127.0.0.1"

std::map<std::string, int> historyStatus;

bool readStatus() {
    std::ifstream file("history.txt");
    if (!file.is_open()) {
        std::cerr << "No file history.txt found." << std::endl;
        return false;
    }

    std::string line;
    while (getline(file, line)) {
        std::stringstream ss(line);
        std::string bookCode;
        int count;
        getline(ss, bookCode, ',');
        ss >> count;
        historyStatus[bookCode] = count;
    }

//    for (const auto& [bookCode, count] : status) {
//        std::cout << "Book Code: " << bookCode << ", Count: " << count << std::endl;
//    }

    file.close();
    return true;
}

std::string processQuery(const std::string& bookCode) {
    if (historyStatus.find(bookCode) == historyStatus.end()) {
        return "BOOK_NOT_FOUND";
    } else if (historyStatus[bookCode] > 0) {
        historyStatus[bookCode]--;
        return std::to_string(historyStatus[bookCode] + 1);
    } else {
        return "0";
    }
}

bool sendToMainServer() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "serverH UDP socket creation error." << std::endl;
        return false;
    }
    struct sockaddr_in serverMAdd;
    memset(&serverMAdd, 0, sizeof(serverMAdd));
    serverMAdd.sin_family = AF_INET;
    serverMAdd.sin_port = htons(M_PORT);
    inet_pton(AF_INET, M_IP, &serverMAdd.sin_addr);
    for (const auto& [bookCode, count] : historyStatus) {
        std::string message = bookCode + "," + std::to_string(count);
        sendto(sockfd, message.c_str(), message.size(), 0,
               (struct sockaddr *)&serverMAdd, sizeof(serverMAdd));
    }
    std::string terminator = "END_OF_LIST_H";
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
        std::cerr << "serverH UDP socket creation error." << std::endl;
        return 1;
    }

    struct sockaddr_in serverHAddress;
    memset(&serverHAddress, 0, sizeof(serverHAddress));
    serverHAddress.sin_family = AF_INET;
    serverHAddress.sin_port = htons(H_PORT);
    serverHAddress.sin_addr.s_addr = INADDR_ANY;

    int bound = bind(sockfd, (struct sockaddr *)&serverHAddress, sizeof(serverHAddress));
    if (bound < 0) {
        std::cerr << "serverH UDP socket bind error." << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "server H is up and running using UDP on port " << H_PORT << "." << std::endl;

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

            std::cout << "Server H received " << bookCode << " code from the Main Server." << std::endl;

            sendto(sockfd, response.c_str(), response.length(), 0,
                   (struct sockaddr*)&sourceAdd, sizeof(sourceAdd));

            std::cout << "Server H finished sending the availability status of code " << bookCode <<
                      " to the Main Server using UDP on port " << H_PORT << "." << std::endl;
        }
    }
    
//    sendToMainServer();
    close(sockfd);
    return 0;
}