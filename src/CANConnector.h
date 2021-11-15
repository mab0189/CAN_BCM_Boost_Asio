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
#include <boost/system/error_code.hpp>


/*******************************************************************************
 * DEFINES
 ******************************************************************************/

/**
 * Defines how many frames can be put in a bcmMsgMultipleFrames operation.
 * The socketCAN BCM can send up to 256 CAN frames in a sequence in the case
 * of a cyclic TX task configuration. See the socketCAN BCM documentation.
 */
#define MAXFRAMES 256


/*******************************************************************************
 * STRUCTS
 ******************************************************************************/

/**
 * Struct for a BCM message with a single CAN frame.
 */
struct bcmMsgSingleFrameCan{
    struct bcm_msg_head msg_head;
    struct can_frame canFrame[1];
};

/**
 * Struct for a BCM message with a single CANFD frame.
 */
struct bcmMsgSingleFrameCanFD{
    struct bcm_msg_head msg_head;
    struct canfd_frame canfdFrame[1];
};

/**
 * Struct for a BCM message with multiple CAN frames.
 */
struct bcmMsgMultipleFramesCan{
    struct bcm_msg_head msg_head;
    struct can_frame canFrames[MAXFRAMES];
};

/**
* Struct for a BCM message with multiple CANFD frames.
*/
struct bcmMsgMultipleFramesCanFD{
    struct bcm_msg_head msg_head;
    struct canfd_frame canfdFrames[MAXFRAMES];
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
    void handleSendData();

private:
    // Function members
    boost::asio::generic::datagram_protocol::socket createBcmSocket();

    void startProcessing();
    void stopProcessing();
    void ioContextThreadFunction(const boost::shared_ptr<boost::asio::io_context>& context);

    void receiveOnSocket();
    void txSendSingleFrame(struct canfd_frame frame, int isCANFD);
    void txSendMultipleFrames(struct canfd_frame frames[], int nframes, int isCANFD);
    void txTest(struct canfd_frame frame);

    void handleReceivedData(const bcm_msg_head* head, void* frames, uint32_t nframes, int isCANFD);

    // Data members
    boost::shared_ptr<boost::asio::io_context> ioContext;
    boost::asio::generic::datagram_protocol::socket bcmSocket;
    std::array<std::uint8_t, sizeof(struct bcmMsgMultipleFramesCanFD)> rxBuffer{0};
    std::thread ioContextThread;

};


#endif //CAN_BCM_BOOST_ASIO_CANCONNECTOR_H
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/