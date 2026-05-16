#ifndef CHARGE_CTRL_H
#define CHARGE_CTRL_H

#include <stdint.h>
#include "ChargeState.h"

void ChargeCtrl_Init(void);
void ChargeCtrl_SetUsbChargeable(int chargeable);
void ChargeCtrl_NotifyUsbInserted(void);
void ChargeCtrl_NotifyUsbRemoved(void);
void ChargeCtrl_NotifyBatteryStopRequest(void);
void ChargeCtrl_NotifyBatteryAllowRequest(void);
void ChargeCtrl_NotifyBatteryComplete(void);
void ChargeCtrl_NotifyHostStop(void);
void ChargeCtrl_NotifyHostAllow(void);
void ChargeCtrl_NotifyFatalError(void);
void ChargeCtrl_DelayProc(void (*callback)(void *), uint32_t intervalMs, void *arg);
ChargeCtrl_State ChargeCtrl_GetState(void);
int ChargeCtrl_IsChargingActive(void);

#endif // CHARGE_CTRL_H
