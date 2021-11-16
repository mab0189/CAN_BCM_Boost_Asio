/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      main.cpp
 \brief     The main function.
 \author    Matthias Bank
 \version   1.0.0
 \date      12.11.2021
 ******************************************************************************/


/*******************************************************************************
 * INCLUDES
 ******************************************************************************/
#include "CANConnector.h"
#include <iostream>


/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/
int main() {

    std::cout << "Hello from main!" << std::endl;

    // Create a CAN connector
    CANConnector myConnector;

    // Test the connector
    myConnector.handleSendingData();

    // Let the CAN connector run for a few seconds
    std::this_thread::sleep_for(std::chrono::seconds(20));

    // Reaching the end of main will call the destructor of the CAN connector
    return 0;
}
