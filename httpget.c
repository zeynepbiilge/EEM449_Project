/*
 * Copyright (c) 2015-2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== httpget.c ========
 *  HTTP Client GET example application
 */
/*
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== empty_min.c ========
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// XDCtools Header files
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

/* TI-RTOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Idle.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>
#include <ti/net/http/httpcli.h>

#include "Board.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#define TASKSTACKSIZE   4096
#define SOCKETTEST_IP   "192.168.1.66"
#define OUTGOIN_PORT    5011
#define INCOMING_PORT   5030
#define DEVICE_ID       0x57

Task_Struct task0Struct;
Char task0Stack[TASKSTACKSIZE];

I2C_Handle      i2c;
I2C_Params      i2cParams;
I2C_Transaction i2cTransaction;


extern Mailbox_Handle mailbox0;
extern Swi_Handle swi0;

extern time_t ts;

Void startNTP(UArg arg1, UArg arg2);


/* Posts SWI every second to keep time current */
Void timerISR(UArg arg1){
    Swi_post(swi0);
}

/* Increments time */
Void swi_func(UArg arg1, UArg arg2){
    ts+=1;
}

/* Function to open I2C communication */
bool I2C_OpenCommunication(void)
{
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz; // select communication speed
    i2c = I2C_open(Board_I2C0, &i2cParams);

    bool returnValue = false;

    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }
    else {
        System_printf("I2C Initialized!\n");
        returnValue = true;
    }
    System_flush();
    return returnValue;
}

/* Function to close I2C communication */
void I2C_CloseCommunication(void)
{
    I2C_close(i2c);
    System_printf("I2C closed!\n");
    System_flush();
}

/* Function to write value to sensor registers */
bool I2C_writeRegister(int device_ID, int addr, uint8_t val)
{
    uint8_t txBuffer[2];
    uint8_t rxBuffer[2];

    txBuffer[0] = addr;
    txBuffer[1] = val;

    // set I2C parameters
    i2cTransaction.slaveAddress = device_ID;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 2;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 0;

    bool returnValue=false;

    if (I2C_transfer(i2c, &i2cTransaction)) {
        System_printf("Writing register : (%d,%d)\n", addr, val);
        returnValue = true;
    }
    else {
        System_printf("I2C Bus fault\n");
    }
    System_flush();

    return returnValue;
}

/* Function to read data from sensor registers */
bool I2C_readRegister(int device_ID, int addr, int no_of_bytes, char *buf)
{
    uint8_t txBuffer[2];
    txBuffer[0] = addr;

    i2cTransaction.slaveAddress = device_ID;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = buf;
    i2cTransaction.readCount = no_of_bytes;

    bool returnValue=false;

    if (I2C_transfer(i2c, &i2cTransaction)) {

        returnValue=true;
    }
    else {
        System_printf("I2C Bus fault\n");
    }
    System_flush();

    return returnValue;
}

Void getBPMValue(UArg arg0, UArg arg1)
{
        char buffer[2];
        char BPMVal[8];
        int currentSensorData,DCFilterVal, BWFilterData;
        int cnt = 0;
        int pulseW = 0;
        int count = 0;
        int next1=0;
        I2C_OpenCommunication();
        int BPM;
        char mode;
        int pulStat = 0;
        int DC, DCOld,oldVal = 0;
        int BWOld,BWNew = 0;


        /* check if connection with sensor is established */
        I2C_readRegister(DEVICE_ID, 0xFF, 1, buffer);
        System_printf("WHO_AM_I register = 0x%x\n", buffer[0]);
        System_flush();

        /* select sensor mode */
        I2C_writeRegister(DEVICE_ID, 0x06, 0x03);

        /* set LED pulse width */
        I2C_writeRegister(DEVICE_ID,0x07,0x07);

        /* set IR and red LED currents*/
        I2C_writeRegister(DEVICE_ID,0x09,0x49);

        while(1) {

            I2C_readRegister(DEVICE_ID, 0x05, 4, buffer);
            currentSensorData = (buffer[0] << 8) | buffer[1];

            DCFilterVal = removeDCValue(currentSensorData); // get DC filtered value

            BWFilterData = butterworthFilter(DCFilterVal); //butterworth filter

            Task_sleep(20);

            if(BWFilterData > oldVal & pulStat == 0){
                pulStat = 1;

            }
            if(BWFilterData <= oldVal-20 & pulStat == 1){

                pulseW =count;
                pulStat = 0;
                count = 0;
                System_printf("pulse\n");
                System_flush();

            }
            if (cnt==50){
                System_printf("BPM = %d\n", BPM);
                System_flush();
                cnt=0;
                Mailbox_post(mailbox0, &BPM, BIOS_WAIT_FOREVER);

            }
            cnt++;
            count++;
            oldVal = BWFilterData;
            BPM = 1200 / pulseW;

        }

        I2C_CloseCommunication();
    }


int removeDCValue(int currentSensorData)
{
    int filteredValue,filteringValue;
    static int prevSensorData=0;

    filteringValue = currentSensorData + (0.85*prevSensorData);
    filteredValue = filteringValue - prevSensorData;
    prevSensorData = currentSensorData;

    return filteredValue;

}

int butterworthFilter(int DCFilterVal)
{
    static int prevResult=0;
    static int currResult;
    int result;

    currResult = (2.452372752527856026e-1 * DCFilterVal) + (0.50952544949442879485 * prevResult);
    result = currResult + prevResult;
    prevResult = currResult;
    return result;
}

void printError(char *errString, int code)
{
    System_printf("Error! code = %d, desc = %s\n", code, errString);
    BIOS_exit(code);
}

Void serverSocketTask(UArg arg0, UArg arg1)
{

    float temp;

    int serverfd, new_socket, valread, len;
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[30];
    char outstr[30], tmpstr[30];
    bool quit_protocol;

    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverfd == -1)
    {
        System_printf(
                "serverSocketTask::Socket not created.. quiting the task.\n");
        return;     // we just quit the tasks. nothing else to do.
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(INCOMING_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Attaching socket to the port
    //
    if (bind(serverfd, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0)
    {
        System_printf("serverSocketTask::bind failed..\n");

        // nothing else to do, since without bind nothing else works
        // we need to terminate the task
        return;
    }
    if (listen(serverfd, 3) < 0)
    {

        System_printf("serverSocketTask::listen() failed\n");
        // nothing else to do, since without bind nothing else works
        // we need to terminate the task
        return;
    }


    while(1) {

        len = sizeof(clientAddr);
        if ((new_socket = accept(serverfd, (struct sockaddr *)&clientAddr, &len))<0) {
            System_printf("serverSocketTask::accept() failed\n");
            continue;               // get back to the beginning of the while loop
        }

        System_printf("Accepted connection\n"); // IP address is in clientAddr.sin_addr
        System_flush();

        // task while loop
        //
        quit_protocol = false;
        do {

            // let's receive data string
            if((valread = recv(new_socket, buffer, 10, 0))<0) {

                // there is an error. Let's terminate the connection and get out of the loop
                //
                close(new_socket);
                break;
            }

            // let's truncate the received string
            //
            buffer[10]=0;
            if(valread<10) buffer[valread]=0;

            System_printf("message received: %s\n", buffer);

            if(!strcmp(buffer, "HELLO")) {
                strcpy(outstr,"GREETINGS 200\n");
                send(new_socket , outstr , strlen(outstr) , 0);
                System_printf("Server <-- GREETINGS 200\n");
            }
            else if(!strcmp(buffer, "GETTIME")) {
                getTimeStr(tmpstr);
                strcpy(outstr, "OK ");
                strcat(outstr, tmpstr);
                strcat(outstr, "\n");
                send(new_socket , outstr , strlen(outstr) , 0);
                quit_protocol = false;
            }
            else if(!strcmp(buffer, "QUIT")) {
                quit_protocol = true;     // it will allow us to get out of while loop
                strcpy(outstr, "BYE 200");
                send(new_socket , outstr , strlen(outstr) , 0);
            }
            else if (!strcmp(buffer, "READBPM"))
            {
                static int avgbpm;
                static float avg;
                int i;


                for(i=0;i<10;i++){ // 5 bpm values to be averaged
                Mailbox_pend(mailbox0, &avgbpm, BIOS_WAIT_FOREVER); //waiting for bpm values
                avg += avgbpm;
                }
                avg = avg/10;
                sprintf(outstr, "BPM %5.2f\n", avg);
                send(new_socket, outstr, strlen(outstr), 0);
            }

        }
        while(!quit_protocol);

        System_flush();
        close(new_socket);
    }

    close(serverfd);
    return;
}
void getTimeStr(char *str)
{

    sprintf(str,"Current time: %s\n", ctime(&ts));
}

void netIPAddrHook(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd)
{
    // Create a HTTP task when the IP address is added
    if (fAdd) {
        createTasks();
    }
}

bool createTasks(void)
{
    static Task_Handle taskHandle2, taskHandle1, taskHandle3;
    Task_Params taskParams;
    Error_Block eb;

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle1 = Task_create((Task_FuncPtr)getBPMValue, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle3 = Task_create((Task_FuncPtr)serverSocketTask, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = 5000;
    taskParams.priority = 1;
    taskHandle2 = Task_create((Task_FuncPtr) startNTP, &taskParams, &eb);


    if ( taskHandle2==NULL || taskHandle3 == NULL || taskHandle1==NULL) {
        printError("netIPAddrHook: Failed to create HTTP, Socket and Server Tasks\n", -1);
        return false;
    }

    return true;
}
/*
 *  ======== main ========
 */
int main(void)
{
     Board_initGeneral();
      Board_initGPIO();
      Board_initI2C();
      Board_initEMAC();

      /* Turn on user LED */
      GPIO_write(Board_LED0, Board_LED_ON);

      System_printf("Starting the HTTP GET example\nSystem provider is set to "
              "SysMin. Halt the target to view any SysMin contents in ROV.\n");
      /* SysMin will only print to the console when you call flush or exit */
      System_flush();


      /* Start BIOS */
      BIOS_start();

      return (0);
}

