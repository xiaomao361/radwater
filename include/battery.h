#pragma once
#include <cstdint>
namespace pond {
// Voltage estimate, matching the locked M5Unified ADC mapping. No charge inference.
struct BatteryState {
    int millivolts=-1,percent=-1;
    bool update(int mv){
        int previousMv=millivolts,previousPercent=percent;
        if(mv<2500||mv>4500){millivolts=percent=-1;}
        else {
            millivolts=mv;
            int level=(mv-3300)*100/800;
            percent=level<0?0:level>100?100:level;
            // Five-percent steps express the limited precision of voltage-based estimates.
            percent=((percent+2)/5)*5;
        }
        return previousMv!=millivolts||previousPercent!=percent;
    }
};
}
