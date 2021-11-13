#include "CANConnector.h"
#include <iostream>

int main() {
    std::cout << "Hello from main" << std::endl;

    CANConnector myConnector;

    std::this_thread::sleep_for(std::chrono::seconds(5));


    return 0;
}
