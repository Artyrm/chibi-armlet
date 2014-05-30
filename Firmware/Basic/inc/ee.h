/*
 * ee.h
 *
 *  Created on: 30 ��� 2014 �.
 *      Author: g.kruglov
 */

#ifndef EE_H_
#define EE_H_

#if 1 // ==== Addresses ====
// Addresses
#define EE_DEVICE_ID_ADDR       0
#define EE_DEVICE_TYPE_ADDR     4
#define EE_DOSETOP_ADDR         8  // ID is uint32_t
#define EE_REPDATA_ADDR         12
#endif

extern Eeprom_t EE;


#endif /* EE_H_ */
