
#ifndef MAX30100_H_
#define MAX30100_H_

#define TASKSTACKSIZE   4096
#define SOCKETTEST_IP   "192.168.1.66"
#define OUTGOIN_PORT    5011
#define INCOMING_PORT   5030
#define DEVICE_ID       0x57
#define MODE_SEL_REG    0x06
#define LED_PW_SEL_REG  0x07
#define WHO_AM_I        0xFF
#define LED_CUR_SEL_REG 0x09
#define READ_REG        0x05

int removeDCValue(int currentSensorData);
int butterworthFilter(int DCFilterVal);
bool pulseDetect(int sensorValue);
void getTimeStr(char *str);
bool createTasks(void);

#endif
