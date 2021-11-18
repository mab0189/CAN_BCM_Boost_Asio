/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      CANConnector.c
 \brief     The Connector enables the communication over a CAN/CANFD interface.
            It builds upon the socketCAN BCM socket and boost::asio.
 \author    Matthias Bank
 \version   1.0.0
 \date      12.11.2021
 ******************************************************************************/


/*******************************************************************************
 * INCLUDES
 ******************************************************************************/
#include "CANConnector.h"


/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

CANConnector::CANConnector() : ioContext(boost::make_shared<boost::asio::io_context>()), bcmSocket(createBcmSocket()){

    // Create the first receive operation
    receiveOnSocket();

    // Start the io context loop
    startProcessing();

    std::cout << "CAN Connector created" << std::endl;
}

CANConnector::~CANConnector(){

    // Stop the io context
    stopProcessing();

    std::cout << "CAN Connector destroyed" << std::endl;
}

/**
 * Creates the bcmSocket data member.
 *
 * @return The BCM socket
 */
boost::asio::generic::datagram_protocol::socket CANConnector::createBcmSocket() {

    // Error code return value
    boost::system::error_code errorCode;

    // Define Address family and protocol
    boost::asio::generic::datagram_protocol bcmProtocol(PF_CAN, CAN_BCM);

    // Create a BCM socket
    boost::asio::generic::datagram_protocol::socket socket(*ioContext, bcmProtocol);

    // Create an I/O command and resolve the interface name to an interface index
    InterfaceIndexIO interfaceIndexIO(INTERFACE);
    socket.io_control(interfaceIndexIO, errorCode);

    // Check if we could resolve the interface correctly
    if(errorCode){
        std::cout << "An error occurred on the io control operation: " << errorCode.message() << std::endl;
    }

    // Connect the socket
    sockaddr_can addr = {0};
    addr.can_family   = AF_CAN;
    addr.can_ifindex  = interfaceIndexIO.index();

    boost::asio::generic::datagram_protocol::endpoint bcmEndpoint{&addr, sizeof(addr)};
    socket.connect(bcmEndpoint, errorCode);

    // Check if we could connect correctly
    if(errorCode){
        std::cout << "An error occurred on the connect operation: " << errorCode.message() << std::endl;
    }

    // Note: In contrast to a raw CAN socket there is no need to
    // explicitly enable CANFD for an BCM socket with setsockopt!

    return socket;
}

/**
 * Stats the io context loop.
 */
void CANConnector::startProcessing(){

    // Run the io context in its own thread
    ioContextThread = std::thread(&CANConnector::ioContextThreadFunction, this, std::ref(ioContext));

    std::cout << "CAN Connector starting io context loop processing" << std::endl;
}

/**
 * Stops the io context loop.
 */
void CANConnector::stopProcessing(){

    // Check if we need to stop the io context loop
    if(!ioContext->stopped()){
        ioContext->stop();
    }

    // Join the io context loop thread
    if(ioContextThread.joinable()){
        ioContextThread.join();
    }else{
        std::cout << "Error ioContextThread was not joinable" << std::endl;
    }

    std::cout << "CAN Connector stopped io context loop processing" << std::endl;
}

/**
 * Thread for the io context loop.
 */
void CANConnector::ioContextThreadFunction(const boost::shared_ptr<boost::asio::io_context>& context){
    context->run();
}

/**
 * Receives on the BCM socket. The received data is stored in the rxBuffer.
 * After processing the receive operation the next receive operation is
 * created to keep  the io context loop running.
 */
void CANConnector::receiveOnSocket(){

    std::cout << "CAN Connector created new receive operation" << std::endl;

    // Create an async receive operation on the BCM socket
    bcmSocket.async_receive(boost::asio::buffer(rxBuffer),
                            [this](boost::system::error_code errorCode, std::size_t receivedBytes){

        // Lambda completion function for the async receive operation

        // Check the error code of the operation
        if(!errorCode){

            std::cout << "CAN Connector received: " << receivedBytes << std::endl;

            // We need to receive at least a whole bcm_msg_head
            if(receivedBytes >= sizeof(bcm_msg_head)){

                // Get the bcm_msg_head
                const auto* head = reinterpret_cast<const bcm_msg_head*>(rxBuffer.data());

                // Check if the message contains CAN or CANFD frames
                bool isCANFD = false;

                if(head->flags & CAN_FD_FRAME){
                    isCANFD = true;
                }

                // Calculate the expected size in bytes of the whole
                // message based upon the bcm_msg_head information.
                size_t expectedBytes = 0;

                if(isCANFD){
                    expectedBytes = head->nframes * sizeof(canfd_frame) + sizeof(bcm_msg_head);
                }else{
                    expectedBytes = head->nframes * sizeof(can_frame)   + sizeof(bcm_msg_head);
                }

                // Check if we received the whole message
                if(receivedBytes == expectedBytes){

                    // Get the pointer to the frames and call the next function to process the data
                    auto frames = reinterpret_cast<void *>(rxBuffer.data() + sizeof(bcm_msg_head));
                    handleReceivedData(head, frames, head->nframes, isCANFD);

                }else{
                    std::cout << "The expected amount of bytes is not equal to the received bytes" << std::endl;
                }

            }

        }else{
            std::cout << "An error occurred on the async receive operation: " << errorCode.message() << std::endl;
        }

        // Create the next receive operation
        receiveOnSocket();

    });

}

/**
 * Create a non cyclic transmission task for a single CAN/CANFD frame.
 *
 * @param frame   - The frame that should be send.
 * @param isCANFD - Flag for CANFD frame.
 */
void CANConnector::txSendSingleFrame(struct canfd_frame frame, bool isCANFD){

    // BCM message we are sending with a single CAN or CANFD frame
    std::shared_ptr<void> msg = nullptr;
    size_t msgSize = 0;

    // Check if we are sending CAN or CANFD frames
    // and create the according struct
    if(isCANFD){
        msgSize = sizeof(struct bcmMsgSingleFrameCanFD);
        auto msgCANFD = std::make_shared<bcmMsgSingleFrameCanFD>();
        msg = std::reinterpret_pointer_cast<void>(msgCANFD);
    }else {
        msgSize = sizeof(struct bcmMsgSingleFrameCan);
        auto msgCAN = std::make_shared<bcmMsgSingleFrameCan>();
        msg = std::reinterpret_pointer_cast<void>(msgCAN);
    }

    // Error handling / Sanity check
    if(msg == nullptr){
        std::cout << "Error could not make message structure" << std::endl;
    }

    // Fill out the message
    if(isCANFD){
        std::shared_ptr<bcmMsgSingleFrameCanFD> msgCANFD = std::reinterpret_pointer_cast<bcmMsgSingleFrameCanFD>(msg);
        msgCANFD->msg_head.opcode  = TX_SEND;
        msgCANFD->msg_head.flags   = CAN_FD_FRAME;
        msgCANFD->msg_head.nframes = 1;
        msgCANFD->canfdFrame[0]    = frame;
    }else{
        std::shared_ptr<bcmMsgSingleFrameCan> msgCAN = std::reinterpret_pointer_cast<bcmMsgSingleFrameCan>(msg);
        msgCAN->msg_head.opcode    = TX_SEND;
        msgCAN->msg_head.nframes   = 1;
        msgCAN->canFrame[0]        = *((struct can_frame*) &frame);
    }

    // Note: buffer doesn't accept smart pointers. Need to use a regular pointer.
    boost::asio::const_buffer buffer = boost::asio::buffer(msg.get(), msgSize);

    // Note: Must guarantee the validity of the argument until the handler is invoked.
    // We guarantee the validity through the lambda capture with and the smart pointer.

    // Note: The TX_SEND operation can only handle exactly one frame!
    bcmSocket.async_send(buffer, [msg](boost::system::error_code errorCode, std::size_t size){

        // Lambda completion function for the async TX_SEND operation

        // Check boost asio error code
        if(!errorCode){
            std::cout << "Transmission of TX_SEND completed successfully" << std::endl;
        }else{
            std::cerr << "Transmission of TX_SEND failed" << std::endl;
        }

    });

}

/**
 * Create a non cyclic transmission task for multiple CAN/CANFD frames.
 * 
 * @param frames  - The frames that should be send.
 * @param nframes - The number of frames that should be send.
 * @param isCANFD - Flag for CANFD frames.
 */
void CANConnector::txSendMultipleFrames(struct canfd_frame frames[], int nframes, bool isCANFD){

    // Note: The TX_SEND operation can only handle exactly one frame!
    // That's why we should use this wrapper for multiple frames.
    for(int index = 0; index < nframes; index++){
        txSendSingleFrame(frames[index], isCANFD);
    }

}

/**
 * Create a cyclic transmission task for one or multiple CAN/CANFD frames.
 * If more than one frame should be send cyclic the provided sequence of
 * the frames is kept by the BCM.
 *
 * @param frames   - The array of CAN/CANFD frames that should be send cyclic.
 * @param nframes  - The number of CAN/CANFD frames that should be send cyclic.
 * @param count    - Number of times the frame is send with the first interval.
 *                   If count is zero only the second interval is being used.
 * @param ival1    - First interval.
 * @param ival2    - Second interval.
 * @param isCANFD  - Flag for CANFD frames.
 */
void CANConnector::txSetupSequence(struct canfd_frame frames[], int nframes, uint32_t count,
                           struct bcm_timeval ival1, struct bcm_timeval ival2, bool isCANFD){

    // BCM message we are sending with a single CAN or CANFD frame
    std::shared_ptr<void> msg = nullptr;
    size_t msgSize = 0;

    // Check if we are sending CAN or CANFD frames
    // and create the according struct
    if(isCANFD){
        msgSize = sizeof(struct bcmMsgMultipleFramesCanFD);
        auto msgCANFD = std::make_shared<bcmMsgMultipleFramesCanFD>();
        msg = std::reinterpret_pointer_cast<void>(msgCANFD);
    }else {
        msgSize = sizeof(struct bcmMsgMultipleFramesCan);
        auto msgCAN = std::make_shared<bcmMsgMultipleFramesCan>();
        msg = std::reinterpret_pointer_cast<void>(msgCAN);
    }

    // Error handling / Sanity check
    if(msg == nullptr){
        std::cout << "Error could not make message structure" << std::endl;
    }

    // Fill out the message

    // Note: By combining the flags SETTIMER and STARTTIMER
    // the BCM will start sending the messages immediately
    if(isCANFD){
        std::shared_ptr<bcmMsgMultipleFramesCanFD> msgCANFD = std::reinterpret_pointer_cast<bcmMsgMultipleFramesCanFD>(msg);

        msgCANFD->msg_head.opcode  = TX_SETUP;
        msgCANFD->msg_head.flags   = CAN_FD_FRAME | SETTIMER | STARTTIMER;
        msgCANFD->msg_head.count   = count;
        msgCANFD->msg_head.ival1   = ival1;
        msgCANFD->msg_head.ival2   = ival2;
        msgCANFD->msg_head.nframes = nframes;

        size_t arrSize = sizeof(struct canfd_frame) * nframes;
        std::memcpy(msgCANFD->canfdFrames, frames, arrSize);
    }else{
        std::shared_ptr<bcmMsgMultipleFramesCan> msgCAN = std::reinterpret_pointer_cast<bcmMsgMultipleFramesCan>(msg);

        msgCAN->msg_head.opcode    = TX_SETUP;
        msgCAN->msg_head.flags     = SETTIMER | STARTTIMER;
        msgCAN->msg_head.count     = count;
        msgCAN->msg_head.ival1     = ival1;
        msgCAN->msg_head.ival2     = ival2;
        msgCAN->msg_head.nframes   = nframes;

        for(int index = 0; index < nframes; index++){
            struct can_frame *canFrame = (struct can_frame*) &frames[index];
            msgCAN->canFrames[index] = *canFrame;
        }
    }

    // Note: buffer doesn't accept smart pointers. Need to use a regular pointer.
    boost::asio::const_buffer buffer = boost::asio::buffer(msg.get(), msgSize);

    bcmSocket.async_send(buffer, [msg](boost::system::error_code errorCode, std::size_t size){

        // Lambda completion function for the async TX_SETUP operation

        // Check boost asio error code
        if(!errorCode){
            std::cout << "Transmission of TX_SETUP completed successfully" << std::endl;
        }else{
            std::cerr << "Transmission of TX_SETUP failed" << std::endl;
        }

    });

}

/**
 * Removes a cyclic transmission task for the given CAN ID
 *
 * @param canID - The CAN ID of the task that should be removed
 */
void CANConnector::txDelete(canid_t canID){

    // BCM message we are sending
    auto msg = std::make_shared<bcm_msg_head>();

    // Fill out the message
    msg->opcode = TX_DELETE;
    msg->can_id = canID;

    // Note: buffer doesn't accept smart pointers. Need to use a regular pointer.
    boost::asio::const_buffer buffer = boost::asio::buffer(msg.get(), sizeof(bcm_msg_head));

    bcmSocket.async_send(buffer, [msg](boost::system::error_code errorCode, std::size_t size){

        // Lambda completion function for the async TX_DELETE operation

        // Check boost asio error code
        if(!errorCode){
            std::cout << "TX_DELETE completed successfully" << std::endl;
        }else{
            std::cerr << "TX_DELETE failed" << std::endl;
        }

    });

}

/**
 * Decides what to do with the data we received on the socket.
 * TODO: Change frames type to struct canfd[]?
 *
 * @param head    - The received bcm msg head.
 * @param frames  - The received CAN or CANFD frames.
 * @param nframes - The number of the received frames.
 * @param isCANFD - Flag for CANFD frames.
 */
void CANConnector::handleReceivedData(const bcm_msg_head *head, void *frames, uint32_t nframes, bool isCANFD) {
    std::cout << "Handling the receive" << std::endl;
}

/**
 * Decides what to do with the data we received from the simulation.
 */
void CANConnector::handleSendingData(){

    // Test CAN Frame
    struct can_frame canFrame1 = {0};
    canFrame1.can_id  = 0x123;
    canFrame1.can_dlc = 4;
    canFrame1.data[0] = 0xDE;
    canFrame1.data[1] = 0xAD;
    canFrame1.data[2] = 0xBE;
    canFrame1.data[3] = 0xEF;

    struct can_frame canFrame2 = {0};
    canFrame2.can_id  = 0x345;
    canFrame2.can_dlc = 3;
    canFrame2.data[0] = 0xC0;
    canFrame2.data[1] = 0xFF;
    canFrame2.data[2] = 0xEE;

    // CANFD frame array containing CAN frames
    struct canfd_frame frameArrCAN[2];
    frameArrCAN[0] = *((struct canfd_frame*) &canFrame1);
    frameArrCAN[1] = *((struct canfd_frame*) &canFrame2);

    // Test CANFD Frame
    struct canfd_frame canfdFrame1 = {0};
    canfdFrame1.can_id   = 0x567;
    canfdFrame1.len      = 16;
    canfdFrame1.data[0]  = 0xDE;
    canfdFrame1.data[1]  = 0xAD;
    canfdFrame1.data[2]  = 0xBE;
    canfdFrame1.data[3]  = 0xEF;
    canfdFrame1.data[4]  = 0xDE;
    canfdFrame1.data[5]  = 0xAD;
    canfdFrame1.data[6]  = 0xBE;
    canfdFrame1.data[7]  = 0xEF;
    canfdFrame1.data[8]  = 0xDE;
    canfdFrame1.data[9]  = 0xAD;
    canfdFrame1.data[10] = 0xBE;
    canfdFrame1.data[11] = 0xEF;
    canfdFrame1.data[12] = 0xDE;
    canfdFrame1.data[13] = 0xAD;
    canfdFrame1.data[14] = 0xBE;
    canfdFrame1.data[15] = 0xEF;

    struct canfd_frame canfdFrame2 = {0};
    canfdFrame2.can_id   = 0x789;
    canfdFrame2.len      = 12;
    canfdFrame2.data[0]  = 0xC0;
    canfdFrame2.data[1]  = 0xFF;
    canfdFrame2.data[2]  = 0xEE;
    canfdFrame2.data[3]  = 0xC0;
    canfdFrame2.data[4]  = 0xFF;
    canfdFrame2.data[5]  = 0xEE;
    canfdFrame2.data[6]  = 0xC0;
    canfdFrame2.data[7]  = 0xFF;
    canfdFrame2.data[8]  = 0xEE;
    canfdFrame2.data[9]  = 0xC0;
    canfdFrame2.data[10] = 0xFF;
    canfdFrame2.data[11] = 0xEE;

    // CANFD frame array
    struct canfd_frame frameArrCANFD[2];
    frameArrCANFD[0] = canfdFrame1;
    frameArrCANFD[1] = canfdFrame2;

    // Test intervals
    struct bcm_timeval ival1 = {0};
    ival1.tv_sec  = 0;
    ival1.tv_usec = 500;

    struct bcm_timeval ival2 = {0};
    ival2.tv_sec  = 1;
    ival2.tv_usec = 0;

    // Test Mask
    struct canfd_frame mask = {0};
    mask.len     = 1;
    mask.data[0] = 0xFF;

    // Test TX_SEND with a single CAN frame
    //for(auto & i : frameArrCAN){
    //    txSendSingleFrame(i, false);
    //}

    // Test TX_SEND with a single CANFD frames
    //for(auto & i : frameArrCANFD){
    //    txSendSingleFrame(i, true);
    //}

    // Test TX_SEND with multiple CAN frames
    //txSendMultipleFrames(frameArrCAN, 2, false);

    // Test TX_SEND with multiple CANFD frames
    //txSendMultipleFrames(frameArrCANFD, 2, true);

    // Test TX_SETUP with multiple CAN frames
    //txSetupSequence(frameArrCAN, 2, 3, ival1, ival2, false);

    // Test TX_SETUP with multiple CANFD frames
    //txSetupSequence(frameArrCANFD, 2, 3, ival1, ival2, true);

    // Test TX_DELETE
    txSetupSequence(&canfdFrame1, 1, 3, ival1, ival2, true);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    txDelete(0x567);

}


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/