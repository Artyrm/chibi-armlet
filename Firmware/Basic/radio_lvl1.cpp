/*
 * radio_lvl1.cpp
 *
 *  Created on: Nov 17, 2013
 *      Author: kreyl
 */

#include <main.h>
#include "radio_lvl1.h"
#include "evt_mask.h"
#include "cc1101.h"
#include "uart.h"
// For test purposes
#include "led.h"
//extern LedRGB_t Led;

#define DBG_PINS

#ifdef DBG_PINS
#define DBG_GPIO1   GPIOC
#define DBG_PIN1    14
#define DBG1_SET()  PinSet(DBG_GPIO1, DBG_PIN1)
#define DBG1_CLR()  PinClear(DBG_GPIO1, DBG_PIN1)
#define DBG_GPIO2   GPIOC
#define DBG_PIN2    13
#define DBG2_SET()  PinSet(DBG_GPIO2, DBG_PIN2)
#define DBG2_CLR()  PinClear(DBG_GPIO2, DBG_PIN2)
#endif

rLevel1_t Radio;

#if 1 // ================================ Task =================================
static WORKING_AREA(warLvl1Thread, 256);
static void rLvl1Thread(void *arg) {
    chRegSetThreadName("rLvl1");
    while(true) Radio.ITask();
}

//#define TEST_TX
//#define TEST_RX
void rLevel1_t::ITask() {
#ifdef TEST_TX
    // Transmit
    DBG1_SET();
    CC.TransmitSync(&Pkt);
    DBG1_CLR();
//        chThdSleepMilliseconds(99);
#elif defined TEST_RX
    Color_t Clr;
    int8_t Rssi;
    uint8_t RxRslt = CC.ReceiveSync(306, &Pkt, &Rssi);
    if(RxRslt == OK) {
        Uart.Printf("%d\r", Rssi);
        Clr = clWhite;
        if     (Rssi < -100) Clr = clRed;
        else if(Rssi < -90) Clr = clYellow;
        else if(Rssi < -80) Clr = clGreen;
        else if(Rssi < -70) Clr = clCyan;
        else if(Rssi < -60) Clr = clBlue;
        else if(Rssi < -50) Clr = clMagenta;
    }
    else Clr = clBlack;
    Led.SetColor(Clr);
    chThdSleepMilliseconds(99);
#else
    // Iterate cycles
    for(uint32_t CycleN=0; CycleN < CYCLE_CNT; CycleN++) {
        // ==== New cycle begins ====
        // Decide when to transmit
        uint32_t TxSlot = rand() % SLOT_CNT;
//        Uart.Printf("\rTxSlot: %u", TxSlot);
        // If TX slot is not zero, receive at zero cycle or sleep otherwise
        uint32_t TimeBefore = TxSlot * SLOT_DURATION_MS;
        if(TimeBefore != 0) {
            if(CycleN == 0) TryToReceive(TimeBefore);
            else TryToSleep(TimeBefore);
        }
        // TX
        DBG1_SET();
        CC.TransmitSync(&PktTx);
        DBG1_CLR();
        // If TX slot is not last, receive at zero cycle or sleep otherwise
        TimeBefore = (SLOT_CNT - TxSlot - 1) * SLOT_DURATION_MS;
        if(TimeBefore != 0) {
            if(CycleN == 0) TryToReceive(TimeBefore);
            else TryToSleep(TimeBefore);
        }
    }
#endif
}
#endif // task

void rLevel1_t::TryToReceive(uint32_t RxDuration) {
    int8_t Rssi;
    uint32_t TimeEnd = chTimeNow() + RxDuration;
//    Uart.Printf("\r***End: %u", TimeEnd);
    while(true) {
        DBG2_SET();
        uint8_t RxRslt = CC.ReceiveSync(RxDuration, &PktRx, &Rssi);
        DBG2_CLR();
        if(RxRslt == OK) {
//            Uart.Printf("\rRID = %X", PktRx.UID);
            IdBuf.Put(PktRx.UID);
        }
//        Uart.Printf("\rNow: %u", chTimeNow());
        if(chTimeNow() < TimeEnd) RxDuration = TimeEnd - chTimeNow();
        else break;
    }
//    Uart.Printf("\rDif: %u", chTimeNow() - TimeEnd);
}

void rLevel1_t::TryToSleep(uint32_t SleepDuration) {
    if(SleepDuration < MIN_SLEEP_DURATION_MS) return;
    else {
        CC.EnterPwrDown();
        chThdSleepMilliseconds(SleepDuration);
    }
}

#if 1 // ============================
uint8_t rLevel1_t::Init() {
#ifdef DBG_PINS
    PinSetupOut(DBG_GPIO1, DBG_PIN1, omPushPull);
    PinSetupOut(DBG_GPIO2, DBG_PIN2, omPushPull);
#endif
    // Init radioIC
    if(CC.Init() == OK) {
        CC.SetTxPower(CC_Pwr0dBm);
        CC.SetPktSize(RPKT_LEN);
        CC.SetChannel(RCHNL);
        PktTx.UID = App.UID;
        // Thread
        chThdCreateStatic(warLvl1Thread, sizeof(warLvl1Thread), HIGHPRIO, (tfunc_t)rLvl1Thread, NULL);
//        Uart.Printf("\rCC init OK");
        return OK;
    }
    else {
        Uart.Printf("\rCC init error");
        return FAILURE;
    }
}
#endif