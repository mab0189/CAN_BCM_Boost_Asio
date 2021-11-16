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
 * @param frame   - The frame that should be send
 * @param isCANFD - Flag for CANFD frame
 */
void CANConnector::txSendSingleFrame(struct canfd_frame frame, int isCANFD){

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
 * @param frames  - The frames that should be send
 * @param nframes - The number of frames that should be send
 * @param isCANFD - Flag for CANFD frames
 */
void CANConnector::txSendMultipleFrames(struct canfd_frame *frames, int nframes, int isCANFD){

    // Note: The TX_SEND operation can only handle exactly one frame!
    // Thats why we should use this wrapper for multiple frames. 
    for(int index = 0; index < nframes; index++){
        txSendSingleFrame(frames[index], isCANFD);
    }

}

/**
 * Decides what to do with the data we received on the socket.
 * TODO: Change frames type to struct canfd[]?
 *
 * @param head    - The received bcm msg head
 * @param frames  - The received CAN or CANFD frames
 * @param nframes - The number of the received frames
 * @param isCANFD - Flag for CANFD frames
 */
void CANConnector::handleReceivedData(const bcm_msg_head *head, void *frames, uint32_t nframes, int isCANFD) {
    std::cout << "Handling the receive" << std::endl;
}

/**
 * Decides what to do with the data we received from the simulation.
 */
void CANConnector::handleSendData(){

    // Test CAN Frame
    struct can_frame frameCAN1 = {0};
    frameCAN1.can_id  = 0x111;
    frameCAN1.can_dlc = 4;
    frameCAN1.data[0] = 0xDE;
    frameCAN1.data[1] = 0xAD;
    frameCAN1.data[2] = 0xBE;
    frameCAN1.data[3] = 0xEF;

    struct canfd_frame *frameCAN1_fd_ptr = (struct canfd_frame*) &frameCAN1;
    struct canfd_frame frameCAN1_fd = *frameCAN1_fd_ptr;

    // Test CANFD Frame
    struct canfd_frame frameCANFD1 = {0};
    frameCANFD1.can_id   = 0x222;
    frameCANFD1.len      = 16;
    frameCANFD1.data[0]  = 0xDE;
    frameCANFD1.data[1]  = 0xAD;
    frameCANFD1.data[2]  = 0xBE;
    frameCANFD1.data[3]  = 0xEF;
    frameCANFD1.data[4]  = 0xDE;
    frameCANFD1.data[5]  = 0xAD;
    frameCANFD1.data[6]  = 0xBE;
    frameCANFD1.data[7]  = 0xEF;
    frameCANFD1.data[8]  = 0xDE;
    frameCANFD1.data[9]  = 0xAD;
    frameCANFD1.data[10] = 0xBE;
    frameCANFD1.data[11] = 0xEF;
    frameCANFD1.data[12] = 0xDE;
    frameCANFD1.data[13] = 0xAD;
    frameCANFD1.data[14] = 0xBE;
    frameCANFD1.data[15] = 0xEF;

    // Test TX_SEND
    txSendSingleFrame(frameCAN1_fd, 0);
    txSendSingleFrame(frameCANFD1, 1);
}


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/