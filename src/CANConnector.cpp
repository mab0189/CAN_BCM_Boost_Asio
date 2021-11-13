/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      CANConnector.c
 \brief     The CANConnector enables the communication over a CAN interface.
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

CANConnector::CANConnector() : bcmSocket(createBcmSocket()){
    std::cout << "CAN Connector created" << std::endl;
    CANConnector::startProcessing();
}

CANConnector::~CANConnector(){
    CANConnector::stopProcessing();
}

/**
 * Creates the bcmSocket data member.
 *
 * @return The BCM socket
 */
boost::asio::generic::datagram_protocol::socket CANConnector::createBcmSocket() {

    // Create ioContext
    ioContext = boost::make_shared<boost::asio::io_context>();

    // Contains address family and protocol
    boost::asio::generic::datagram_protocol bcmProtocol(PF_CAN, CAN_BCM);

    // Create a BCM socket
    boost::asio::generic::datagram_protocol::socket socket(*ioContext, bcmProtocol);

    // Create an I/O command and resolve the interface name to an interface index
    InterfaceIndexIO interfaceIndexIO(INTERFACE);
    socket.io_control(interfaceIndexIO);

    // Connect the socket
    sockaddr_can addr = {0};
    addr.can_family   = AF_CAN;
    addr.can_ifindex  = interfaceIndexIO.index();

    boost::asio::generic::datagram_protocol::endpoint bcmEndpoint{&addr, sizeof(addr)};
    socket.connect(bcmEndpoint);

    // Note: In contrast to a raw CAN socket there is no need to
    // explicitly enable CANFD for an BCM socket with setsockopt!

    return socket;
}

/**
 * Stats the io context loop.
 */
void CANConnector::startProcessing(){

    std::cout << "CAN Connector starting" << std::endl;

    // Start the work guard so the io context keeps running when there are no operations to process.
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(ioContext->get_executor());

    // Run the io context in its own thread
    ioContextThread = std::thread(&CANConnector::ioContextThreadFunction,  ioContext);
}

/**
 * Stops the io context loop.
 */
void CANConnector::stopProcessing(){

    // Stop the io context loop gracefully and join its thread
    ioContext->stop();
    ioContextThread.join();

    std::cout << "CAN Connector stopped" << std::endl;
}

/**
 * Thread for the io context.
 */
void CANConnector::ioContextThreadFunction(boost::shared_ptr<boost::asio::io_context> context){
    context->run();
}

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/