/*
 * SensorTable.h
 *
 *  Created on: 02 ����. 2014 �.
 *      Author: Kreyl
 */

#ifndef SENSORTABLE_H_
#define SENSORTABLE_H_

#include "kl_lib_L15x.h"
#include "ch.h"
#include "evt_mask.h"

struct Row_t {
    uint8_t ID;
    uint8_t Type;
    int8_t Rssi;
};

#define MAX_ROW_CNT     99
struct Table_t {
    uint32_t Size;
    Row_t Row[MAX_ROW_CNT];
};

template <typename T>
class SnsTable_t {
private:
    Table_t ITbl[2], *PCurrTbl;
    void ISwitchTableI();
    Thread *IPThd;
public:
    void RegisterAppThd(Thread *PThd) { IPThd = PThd; }
    SnsTable_t() : PCurrTbl(&ITbl[0]), IPThd(nullptr), PTable(&ITbl[1]) {}
    Table_t *PTable;
    void PutInfo(uint8_t ID, uint32_t Level);
    void SendEvtReady();
};

#endif /* SENSORTABLE_H_ */
