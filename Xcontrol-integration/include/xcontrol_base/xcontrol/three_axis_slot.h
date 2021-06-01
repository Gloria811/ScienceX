#pragma once
#include <xcontrol/epos4_extended.h>

namespace xcontrol {

/** @brief This class is used to add a arm motor on EtherCAT buss
 */
class ThreeAxisSlot: public Epos4Extended {
public:
    /**
     * @brief Constructor of NavMotor class
     */
    ThreeAxisSlot(bool has_motor);
};

}