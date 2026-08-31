#include "libs/network.h"

#include <cstdio>

int main() {
    if (!Libs::Network::Net::RunUdpLoopbackDiagnostic()) {
        std::fprintf(stderr, "UDP loopback diagnostic failed\n");
        return 1;
    }
    std::puts("UDP loopback diagnostic passed");
    return 0;
}
