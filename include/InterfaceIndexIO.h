/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      InterfaceIndexIO.h
 \brief     I/O control command class for getting the interface index.
            For further information see boost asio io_control.
 \author    Matthias Bank
 \version   1.0.0
 \date      12.11.2021
 ******************************************************************************/
#ifndef CAN_BCM_BOOST_ASIO_INTERFACEINDEXIO_H
#define CAN_BCM_BOOST_ASIO_INTERFACEINDEXIO_H


/*******************************************************************************
 * INCLUDES
 ******************************************************************************/
#include <net/if.h>


/*******************************************************************************
 * CLASS DECLARATIONS
 ******************************************************************************/
class InterfaceIndexIO{

public:
    explicit InterfaceIndexIO(const char* interfaceName);
    static int name();
    void* data();
    int index() const;

private:
    ifreq ifr{};

};


#endif //CAN_BCM_BOOST_ASIO_INTERFACEINDEXIO_H
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/