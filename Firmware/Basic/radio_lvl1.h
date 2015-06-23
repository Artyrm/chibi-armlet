/*
 * radio_lvl1.h
 *
 *  Created on: Nov 17, 2013
 *      Author: kreyl
 */

#ifndef RADIO_LVL1_H_
#define RADIO_LVL1_H_

#include "ch.h"
#include "rlvl1_defins.h"
#include "kl_lib.h"
#include "cc1101.h"
#include "kl_buf.h"

class rLevel1_t {
private:
    rPkt_t Pkt;
    void TryToReceive(uint32_t RxDuration);
    void TryToSleep(uint32_t SleepDuration);
public:
    uint8_t Init();
//    CircBufNumber_t<uint32_t, ID_BUF_SZ> IdBuf;
    // Inner use
    void ITask();
};

extern rLevel1_t Radio;

#endif /* RADIO_LVL1_H_ */
