/*
 * File:   main.cpp
 * Author: Kreyl
 * Project: Armlet2South
 *
 * Created on Feb 05, 2013, 20:27
 */

#include "kl_lib_L15x.h"
#include "ch.h"
#include "hal.h"
#include "clocking_L1xx.h"
#include "peripheral.h"
#include "sequences.h"
#include "pill_mgr.h"
#include "cmd_uart.h"
#include "application.h"
#include "radio_lvl1.h"

int main(void) {
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V2);
    //Clk.SetupFlashLatency(24);  // Setup Flash Latency for clock in MHz
//    Clk.SetupBusDividers(ahbDiv1, apbDiv1, apbDiv1);
    Clk.UpdateFreqValues();

    // ==== Init OS ====
    halInit();
    chSysInit();

    // ==== Init Hard & Soft ====
    Uart.Init(115200);
    Uart.Printf("Shiko AHB=%u; APB1=%u; APB2=%u\r", Clk.AHBFreqHz, Clk.APB1FreqHz, Clk.APB2FreqHz);
    Led.Init();
    Led.SetColor(clRed);
    chThdSleepMilliseconds(450);
    Led.SetColor(clGreen);
    chThdSleepMilliseconds(450);
    Led.SetColor(clBlue);
    chThdSleepMilliseconds(450);

    App.PThd = chThdSelf();
    App.Init();

    Beeper.Init();
    Beeper.Beep(BeepBeep);
    Vibro.Init();
    Vibro.Flinch(BrrBrr);
    PillMgr.Init();
    Radio.Init(0);

//    if(ClkResult) Uart.Printf("Clock failure\r");

    while(1) App.ITask();
}
