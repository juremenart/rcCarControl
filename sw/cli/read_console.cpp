#include <iostream>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include "rcci_client.h"

int main(int argc, char *argv[])
{
    rcciClient client;
    std::string logStr;
    int logEntryCounts = -1;

    if(argc < 3)
    {
        std::cerr << " Usage: " << argv[0] << " <hostname> <port>"
                  << " [numOfLogEntries]" << std::endl;
        return -1;
    }

    if(client.connect(std::string(argv[1]), atoi(argv[2])) < 0)
    {
        std::cerr << "Can not connect to server" << std::endl;
        return -1;
    }

    if(argc == 4)
    {
        logEntryCounts = atoi(argv[3]);
    }

    if(client.logConnect(logStr) < 0)
    {
        std::cerr << "logConnect() failed" << std::endl;
        client.disconnect();
        return -1;
    }

    std::cerr << "<=========== Logging started ============>" << std::endl;
    std::cerr << logStr;

    while((logEntryCounts != 0))
    {
        ssize_t bytes = client.logReadData(logStr);
        if(bytes < 0)
        {
            std::cerr << "Error reading from log server: " <<
                strerror(errno) << std::endl;
            break;
        }
        if(logEntryCounts > 0)
        {
            logEntryCounts--;
        }
        std::cout << logStr;
    }

    client.logDisconnect();
    client.disconnect();

    return 0;
}
