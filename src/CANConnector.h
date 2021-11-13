/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      CANConnector.h
 \brief     The CANConnector enables the communication over a CAN interface.
            It builds upon the socketCAN BCM socket and boost::asio.
 \author    Matthias Bank
 \version   1.0.0
 \date      12.11.2021
 ******************************************************************************/
#ifndef CAN_BCM_BOOST_ASIO_CANCONNECTOR_H
#define CAN_BCM_BOOST_ASIO_CANCONNECTOR_H


/*******************************************************************************
 * INCLUDES
 ******************************************************************************/
// Project includes
#include "InterfaceIndexIO.h"
#include "CANConnectorConfig.h"

// System includes
#include <thread>
#include <iostream>
#include <linux/can.h>
#include <linux/can/bcm.h>
#include <boost/asio.hpp>
#include <boost/make_shared.hpp>


/*******************************************************************************
 * DEFINES
 ******************************************************************************/

/**
 * Defines how many frames can be put in a bcmMsgMultipleFrames operation.
 * The socketCAN BCM can send up to 256 CAN frames in a sequence in the case
 * of a cyclic TX task configuration. Check the socketCAN BCM documentation.
 */
#define MAXFRAMES 256


/*******************************************************************************
 * STRUCTS
 ******************************************************************************/

/**
 * Struct for a BCM message with multiple CAN frames.
 */
struct bcmMsgMultipleFramesCan{
    struct bcm_msg_head msg_head;
    struct can_frame canFrames[MAXFRAMES];
};


/*******************************************************************************
 * CLASS DECLARATIONS
 ******************************************************************************/

class CANConnector{

public:
    // Functions members
    CANConnector();
    ~CANConnector();

    // Data members

private:
    // Function members
    boost::asio::generic::datagram_protocol::socket createBcmSocket();

    void startProcessing();
    void stopProcessing();
    void ioContextThread();

    // Data members
    boost::shared_ptr<boost::asio::io_context> ioContext;
    boost::asio::generic::datagram_protocol::socket bcmSocket;
    std::vector<std::thread> threads;

};


#endif //CAN_BCM_BOOST_ASIO_CANCONNECTOR_H
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/