#include <iostream>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <thread>
#include <chrono>

#include "rcci_client.h"

// Input data file is very simple:
// <int drive> <int steer>

int main(int argc, char *argv[])
{
    rcciClient client;

    if(argc != 4)
    {
        std::cerr << " Usage: " << argv[0] << " <hostname> <port> <input_data_file>"
                  << " [numOfLogEntries]" << std::endl;
        return -1;
    }

    if(client.connect(std::string(argv[1]), atoi(argv[2])) < 0)
    {
        std::cerr << "Can not connect to server" << std::endl;
        return -1;
    }

    if(client.drvConnect() < 0)
    {
        std::cerr << "sendInitMsg() failed" << std::endl;
        return -1;
    }

    std::ifstream infile(argv[3]);
    int drive, steer;
    while(infile >> drive >> steer)
    {
        int retVal = client.drvSendData(drive, steer);
        std::cout << "Sending " << drive << ", " << steer << " retVal=" << retVal << std::endl;
    }

    client.drvDisconnect();
    client.disconnect();
    return 0;
}
