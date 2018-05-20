#ifndef _PIBUSRAIDER_BUSACCESS_H_
#define _PIBUSRAIDER_BUSACCESS_H_

extern void busSetup();
extern void busReleaseControl();
extern void busRequestControl();
extern void busTakeControl();
extern void busSetAddr(uint32_t addr);
extern void busSetData(uint8_t data);
extern void busWriteData(uint8_t data);
extern uint8_t busReadData();

#endif
