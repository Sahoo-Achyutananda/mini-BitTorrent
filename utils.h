#if !defined(UTILS_H)
#define UTILS_H

#include <string>
#include <unistd.h>
using namespace std;

long long writeAll(int fd, const char* buf, long long len){
    long long total = 0;
    while (total < len){
        long long n = write(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

long long readFully(int fd, char* buf, long long len, int timeoutSec = 10){
    long long total = 0;
    while (total < len){
        long long n = read(fd, buf + total, len - total);
        if (n <= 0) return total; // 0 = EOF, -1 = error
        total += n;
    }
    return total;
}

// read a line from socket until '\n', return string without '\n' (accumulates across reads)
bool readLineFromSocket(int fd, string &out, int maxHeader=4096){
    out.clear();
    char ch;
    while ((int)out.size() < maxHeader){
        long long n = read(fd, &ch, 1);
        if (n <= 0) return false; // error or EOF
        if (ch == '\n') return true;
        out.push_back(ch);
    }
    return false; // header too long
}

string readLine(int sockfd) {
    char buffer[1024];
    string line;
    int n;

    while (true) {
        n = read(sockfd, buffer, sizeof(buffer));
        if (n < 0) {
            perror("read error");
            return "";
        } else if (n == 0) {
            // Connection closed
            break;
        }

        // Append received data
        line.append(buffer, n);

        // Check if newline exists
        size_t pos = line.find('\n');
        if (pos != std::string::npos) {
            // Optional: remove data after newline if you want only the line
            std::string result = line.substr(0, pos);
            // Keep remaining data for future reads if needed
            // line = line.substr(pos + 1);
            return result;
        }
    }

    return line;  // return what was read if connection closes without newline
}



#endif // UTILS_H
