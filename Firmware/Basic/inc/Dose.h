/*
 * Dose.h
 *
 *  Created on: 03 ��� 2014 �.
 *      Author: Kreyl
 */

#ifndef DOSE_H_
#define DOSE_H_

#include "eestore.h"
#include "rlvl1_defins.h"
#include "peripheral.h"
#include "colors_sounds.h"
#include "cmd_uart.h"
#include "ee.h"

struct DoseConsts_t {
    int32_t Top;       // Death; top value
    int32_t RedFast;   // Near death
    int32_t Red;       // Yellow if lower
    int32_t Yellow;    // Green if lower
    void Setup(int32_t NewTop) {
        Top = NewTop;
        RedFast = Top - 7;
        Red = (Top * 2) / 3;
        Yellow = Top / 3;
        Uart.Printf("Top=%u; Red=%u; Yellow=%u\r\n", Top, Red, Yellow);
    }
};

// Default Dose constants
#define DOSE_DEFAULT_TOP    10800

enum HealthState_t {hsGreen=0, hsYellow=1, hsRedSlow=2, hsRedFast=3, hsDeath=4};

#if 1 // ==== Drug ====
class Drug_t {
private:
    uint32_t ITimeLeft_ms;
    int32_t IValue;
public:
    void Set(int32_t AValue, int32_t ATime_s) {
        chSysLock();
        IValue = AValue;
        ITimeLeft_ms = (uint32_t)(ATime_s * 1000);  // convert seconds to milliseconds
        chSysUnlock();
    }
    bool IsActive() { return (IValue > 0); }
    void ModifyDamage(int32_t &Damage, uint32_t TimeElapsed_ms) {
        if(IValue <= 0) return; // Not active
        // Process damage
        IValue -= Damage;
        if(IValue < 0) Damage = -IValue;   // Drug have come to the end
        else {
            Damage = 0;
            // Process time
            if(TimeElapsed_ms > ITimeLeft_ms) IValue = 0;   // Drug have come to the end
            else ITimeLeft_ms -= TimeElapsed_ms;
        } // if value < 0
    } // Utilize
};
#endif

#if 1 // ==== Dose ====
class Dose_t {
private:
    EEStore_t EEStore;   // EEPROM storage for dose
    void ConvertDoseToState() {
        if     (Value >= Consts.Top)     State = hsDeath;
        else if(Value >= Consts.RedFast) State = hsRedFast;
        else if(Value >= Consts.Red)     State = hsRedSlow;
        else if(Value >= Consts.Yellow)  State = hsYellow;
        else                             State = hsGreen;
    }
public:
    int32_t Value;
    DoseConsts_t Consts;
    HealthState_t State;
    Drug_t Drug;
#if 1 // ==== Dose set/modify/reset ====
    void Set(int32_t ADose) {
        Value = ADose;
        ConvertDoseToState();
    }
    void Modify(int32_t Amount) {
        int32_t Dz = Value;
        if(Amount >= 0) { // Doze increase
            // Increase no more than up to near death
            if(Dz < Consts.RedFast) {
                if(Amount > (Consts.RedFast - Dz)) Dz = Consts.RedFast;
                else Dz += Amount;
            }
            // Near death, increase no more than 1 at a time
            else if(Dz < Consts.Top) Dz++;  // After death, no need to increase
        }
        else { // Amount < 0, Dose decrease
            if((- Amount) > Dz) Dz = 0;
            else Dz += Amount;
        }
        Set(Dz);
        Uart.Printf("Dz=%d; Dmg=%d\r", Value, Amount);
    }
    void Reset() { Modify(MIN_INT32); }
#endif

#if 1 // ==== Load/Save ====
    // Value: Save if changed
    uint8_t SaveValue() {
        int32_t OldValue = 0;
        if(EEStore.Get((uint32_t*)&OldValue) == OK) {
            if(OldValue == Value) return OK;
        }
        return EEStore.Put((uint32_t*)&Value);
    }
    // Try load from EEPROM, set 0 if failed
    void LoadValue() {
        int32_t FDose = 0;
        EEStore.Get((uint32_t*)&FDose);     // Try to read
        Set(FDose);
    }

    // ==== Top ====
    void SaveTop() { EE.Write32(EE_DOSETOP_ADDR, Consts.Top); }
    void LoadTop() {
        uint32_t tmp1 = EE.Read32(EE_DOSETOP_ADDR);
        if(tmp1 == 0) tmp1 = DOSE_DEFAULT_TOP;  // In case of clear EEPROM, use default value
        Consts.Setup(tmp1);
    }
#endif
};
#endif // Dose

#endif /* DOSE_H_ */
