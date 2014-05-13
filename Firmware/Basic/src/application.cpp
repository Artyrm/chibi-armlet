/*
 * application.cpp
 *
 *  Created on: Nov 9, 2013
 *      Author: kreyl
 */

#include "application.h"
#include "cmd_uart.h"
#include "pill_mgr.h"
#include "peripheral.h"
#include "evt_mask.h"
#include "eestore.h"
#include "radio_lvl1.h"
#include "adc15x.h"
#include <cstdlib>

App_t App;

#if 1 // ================================ Pill =================================
void App_t::OnPillConnect() {
    if(PillMgr.Read(PILL_I2C_ADDR, PILL_START_ADDR, &Pill, sizeof(Pill_t)) != OK) return;
    // Print pill
    Uart.Printf("#PillRead32 0 16\r\n");
    Uart.Printf("#PillData ");
    int32_t *p = (int32_t*)&Pill;
    for(uint32_t i=0; i<PILL_SZ32; i++) Uart.Printf("%d ", *p++);
    Uart.Printf("\r\n");
#ifndef DEVTYPE_PILLPROG
    uint8_t rslt __attribute__((unused));
    switch(Pill.TypeID) {
#if 0 // ==== Set ID ====
        case PILL_TYPEID_SET_ID:
            if(ID == 0) {
                Pill.DeviceID++;
                rslt = PillMgr.Write(PILL_I2C_ADDR, &Pill, (sizeof(Pill.TypeID) + sizeof(Pill.DeviceID)));
                if(rslt == OK) {
                    ISetID(Pill.DeviceID-1);
                    Led.StartBlink(LedPillIdSet);
                    Beeper.Beep(BeepPillOk);
                }
                else {
                    Uart.Printf("Pill Write Error\r");
                    Led.StartBlink(LedPillIdNotSet);
                    Beeper.Beep(BeepPillBad);
                }
            }
            else {
                Uart.Printf("ID already set: %u\r", ID);
                Led.StartBlink(LedPillIdNotSet);
                Beeper.Beep(BeepPillBad);
            }
            chThdSleepMilliseconds(1800);
            break;
#endif

#ifdef DEVTYPE_UMVOS // ==== Cure ====
        case PILL_TYPEID_CURE:
            if(Pill.ChargeCnt > 0) {
                rslt = OK;
                Pill.ChargeCnt--;
                //Pill.
                rslt = PillMgr.Write(PILL_I2C_ADDR, PILL_START_ADDR, &Pill, sizeof(Pill_t));
                    break;
            }
            else rslt = FAILURE; // Discharged pill

            if(rslt == OK) {
                Beeper.Beep(BeepPillOk);
                Led.StartBlink(LedPillCureOk);
                // Decrease dose if not dead, or if this is panacea
                if((Dose.State != hsDeath) or (Pill.Value == INFINITY32)) Dose.Decrease(Pill.Value, diNeverIndicate);
                chThdSleepMilliseconds(2007);    // Let indication to complete
                Dose.RenewIndication();
            }
            else {
                Beeper.Beep(BeepPillBad);
                Led.StartBlink(LedPillBad);
                chThdSleepMilliseconds(2007);    // Let indication to complete
            }
            break;

            // ==== Set consts ====
//        case PILL_TYPEID_SET_DOSETOP:
//            Dose.Consts.Setup(Pill.DoseTop);
//            SaveDoseTop();
//            Uart.Printf("Top=%u; Red=%u; Yellow=%u\r", Dose.Consts.Top, Dose.Consts.Red, Dose.Consts.Yellow);
//            Led.StartBlink(LedPillSetupOk);
//            Beeper.Beep(BeepPillOk);
//            chThdSleepMilliseconds(2007);
//            break;
#endif
        default:
            Uart.Printf("Unknown Pill\r");
            Beeper.Beep(BeepPillBad);
            break;
    } // switch
#else // DEVTYPE_PILLPROG
    Led.StartBlink(LedPillSetupOk);
    Beeper.Beep(BeepPillOk);
#endif
}
#endif

#if 1 // ======================= Command processing ============================
void App_t::OnUartCmd(Cmd_t *PCmd) {
//    Uart.Printf("%S\r", PCmd->S);
    uint8_t b;
    uint32_t dw32 __attribute__((unused));  // May be unused in some cofigurations
    if(PCmd->NameIs("#Ping")) Uart.Ack();

#if 1 // ==== ID ====
    else if(PCmd->NameIs("#SetID")) {
        if(PCmd->TryConvertToNumber(&dw32) == OK) {  // Next token is number
            b = ISetID(dw32);
            Uart.Ack(b);
        }
        else Uart.Ack(CMD_ERROR);
    }
    else if(PCmd->NameIs("#GetID")) Uart.Printf("#ID %u\r\n", ID);
#endif

#if 1 // ==== Pills ====
    else if(PCmd->NameIs("#PillState")) {
        uint32_t PillAddr;
        if(PCmd->TryConvertToNumber(&PillAddr) == OK) {
            if((PillAddr >= 0) and (PillAddr <= 7)) {
                b = PillMgr.CheckIfConnected(PILL_I2C_ADDR);
                if(b == OK) Uart.Printf("#Pill %u Ok\r\n", PillAddr);
                else Uart.Printf("#Pill %u Fail\r\n", PillAddr);
                return;
            }
        }
        Uart.Ack(CMD_ERROR);
    }

    else if(PCmd->NameIs("#PillRead32")) {
        uint32_t PillAddr;
        if(PCmd->TryConvertToNumber(&PillAddr) == OK) {
            if((PillAddr >= 0) and (PillAddr <= 7)) {
                PCmd->GetNextToken();
                uint32_t Cnt = 0;
                uint8_t MemAddr = PILL_START_ADDR;
                if(PCmd->TryConvertToNumber(&Cnt) == OK) {
                    Uart.Printf("#PillData ");
                    for(uint32_t i=0; i<Cnt; i++) {
                        b = PillMgr.Read(PILL_I2C_ADDR, MemAddr, &dw32, 4);
                        if(b != OK) break;
                        Uart.Printf("%u ", dw32);
                        MemAddr += 4;
                    }
                    Uart.Printf("\r\n");
                    if(b != OK) Uart.Ack(b);
                    return;
                } // if data cnt
            } // if pill addr ok
        } // if pill addr
        Uart.Ack(CMD_ERROR);
    }

    else if(PCmd->NameIs("#PillWrite32")) {
        uint32_t PillAddr;
        if(PCmd->TryConvertToNumber(&PillAddr) == OK) {
            if((PillAddr >= 0) and (PillAddr <= 7)) {
                b = OK;
                uint8_t MemAddr = PILL_START_ADDR;
                // Iterate data
                while(true) {
                    PCmd->GetNextToken();   // Get next data to write
                    if(PCmd->TryConvertToNumber(&dw32) != OK) break;
//                    Uart.Printf("%X ", Data);
                    b = PillMgr.Write(PILL_I2C_ADDR, MemAddr, &dw32, 4);
                    if(b != OK) break;
                    MemAddr += 4;
                } // while
                Uart.Ack(b);
                return;
            } // if pill addr ok
        } // if pill addr
        Uart.Ack(CMD_ERROR);
    }

    else if(PCmd->NameIs("#PillRepeatWrite32")) {
        uint32_t PillAddr;
        if(PCmd->TryConvertToNumber(&PillAddr) == OK) {
            if((PillAddr >= 0) and (PillAddr <= 7)) {
                b = OK;
                // Iterate data
                WriteData.Size = 0;
                for(uint32_t i=0; i<PILL_SZ32; i++) {
                    PCmd->GetNextToken();   // Get next data to write
                    if(PCmd->TryConvertToNumber(&dw32) != OK) break;
//                  Uart.Printf("%X ", Data);
//                    WriteData


                } // while
                // Save data to EEPROM
                if(WriteData.Size > 0) {

                }
                Uart.Ack(b);
                return;
            } // if pill addr ok
        } // if pill addr
        Uart.Ack(CMD_ERROR);
    }
#endif

#ifdef DEVTYPE_UMVOS // ==== DoseTop ====
    else if(PCmd->NameIs("#SetDoseTop")) {
        uint32_t NewTop;
        if(PCmd->TryConvertToNumber(&NewTop) == OK) {  // Next token is number
            Dose.Consts.Setup(NewTop);
            SaveDoseTop();
            Uart.Printf("Top=%u; Red=%u; Yellow=%u\r", Dose.Consts.Top, Dose.Consts.Red, Dose.Consts.Yellow);
            Uart.Ack(OK);
        }
        else Uart.Ack(CMD_ERROR);
    }
    else if(PCmd->NameIs("#GetDoseTop")) Uart.Printf("#DoseTop %u\r\n", Dose.Consts.Top);

    else if(PCmd->NameIs("#SetDose")) {
        if(PCmd->TryConvertToNumber(&dw32) == OK) {  // Next token is number
            if(dw32 <= Dose.Consts.Top) {
                Dose.Set(dw32, diAlwaysIndicate);
                Uart.Ack(OK);
            }
        }
        else Uart.Ack(CMD_ERROR);
    }
    else if(PCmd->NameIs("#GetDose")) Uart.Printf("#Dose %u\r\n", Dose.Get());
#endif
    else Uart.Ack(CMD_UNKNOWN);
}
#endif

// ==== Dose ====
#if defined DEVTYPE_UMVOS || defined DEVTYPE_DETECTOR
void App_t::OnRxTableReady() {
//    uint32_t TimeElapsed = chTimeNow() - LastTime;
//    LastTime = chTimeNow();
    // Radio damage
    Table_t *PTbl = RxTable.PTable;
    uint32_t NaturalDmg = 1, RadioDmg = 0;
    RxTable.dBm2PercentAll();
//    RxTable.Print();
    // Iterate received levels
    for(uint32_t i=0; i<PTbl->Size; i++) {
        Row_t *PRow = &PTbl->Row[i];
        int32_t rssi = PRow->Rssi;
        if(rssi >= PRow->LvlMin) {    // Only if signal level is enough
            if((PRow->DmgMax == 0) and (PRow->DmgMin == 0)) NaturalDmg = 0; // "Clean zone"
            else { // Ordinal lustra
                int32_t EmDmg = 0;
                if(rssi >= PRow->LvlMax) EmDmg = PRow->DmgMax;
                else {
                    int32_t DifDmg = PRow->DmgMax - PRow->DmgMin;
                    int32_t DifLvl = PRow->LvlMax - PRow->LvlMin;
                    EmDmg = (rssi * DifDmg + PRow->DmgMax * DifLvl - PRow->LvlMax * DifDmg) / DifLvl;
                    if(EmDmg < 0) EmDmg = 0;
//                    Uart.Printf("Ch %u; Dmg=%d\r", PRow->Channel, EmDmg);
                }
                RadioDmg += EmDmg;
            } // ordinal
        } // if lvl > min
    } // for
    Damage = NaturalDmg + RadioDmg;
//    Uart.Printf("Total=%d\r", Damage);
#ifdef DEVTYPE_UMVOS
    Dose.Increase(NaturalDmg + RadioDmg, diUsual);
    Uart.Printf("Dz=%u; Dmg=%u\r", Dose.Get(), NaturalDmg + RadioDmg);
#endif
}
#ifdef DEVTYPE_DETECTOR
void App_t::OnClick() {
    int32_t r = rand() % (DMG_SND_MAX - 1);
    int32_t DmgSnd = DMG2SNDDMG(Damage);
//    Uart.Printf("%d; %d\r", Damage, DmgSnd);
    if(r < DmgSnd) TIM2->CR1 = TIM_CR1_CEN | TIM_CR1_OPM;
}
#endif
#endif

#if 1 // ========================= Application =================================
void App_t::Init() {
    ID = EE.Read32(EE_DEVICE_ID_ADDR);  // Read device ID
#ifdef DEVTYPE_UMVOS
    // Read dose constants
    uint32_t FTop = EE.Read32(EE_DOSETOP_ADDR);
    if(FTop == 0) FTop = DOSE_DEFAULT_TOP;  // In case of clear EEPROM, use default value
    Dose.Consts.Setup(FTop);
    Uart.Printf("ID=%u; Top=%u; Red=%u; Yellow=%u\r", ID, Dose.Consts.Top, Dose.Consts.Red, Dose.Consts.Yellow);

    Dose.Load();
#endif
#ifdef DEVTYPE_DETECTOR
    rccEnableTIM2(FALSE);
    PinSetupAlterFunc(GPIOB, 3, omPushPull, pudNone, AF1);
    TIM2->CR1 = TIM_CR1_CEN | TIM_CR1_OPM;
    TIM2->CR2 = 0;
    TIM2->ARR = 22;
    TIM2->CCMR1 |= (0b111 << 12);
    TIM2->CCER  |= TIM_CCER_CC2E;

    uint32_t divider = TIM2->ARR * 1000;
    uint32_t FPrescaler = Clk.APB1FreqHz / divider;
    if(FPrescaler != 0) FPrescaler--;
    TIM2->PSC = (uint16_t)FPrescaler;
    TIM2->CCR2 = 20;
#endif
}
uint8_t App_t::ISetID(uint32_t NewID) {
    if(NewID > 0xFFFF) return FAILURE;
    uint8_t rslt = EE.Write32(EE_DEVICE_ID_ADDR, NewID);
    if(rslt == OK) {
        ID = NewID;
        Uart.Printf("New ID: %u\r", ID);
        return OK;
    }
    else {
        Uart.Printf("EE error: %u\r", rslt);
        return FAILURE;
    }
}
#endif
