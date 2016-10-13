#ifndef WINUSBDRIVER_H
#define WINUSBDRIVER_H

#include <QWidget>
#include <QLabel>
#include <QDebug>
#include <QTimer>

#include "functiongencontrol.h"
#include "xmega.h"
#include "desktop_settings.h"
#include "buffercontrol.h"

//libusbK includes
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include "libusbk.h"

#define ISO_PACKET_SIZE 1023
#define ISO_PACKETS_PER_CTX 30
#define NUM_FUTURE_CTX 4
#define MAX_OVERLAP NUM_FUTURE_CTX+1
#define ISO_TIMER_PERIOD 8
#define MAX_VALID_INDEX 374

class winUsbDriver : public QLabel
{
    Q_OBJECT
public:
    explicit winUsbDriver(QWidget *parent = 0);
    char *isoRead(unsigned int *newLength);
    ~winUsbDriver();
    int deviceMode = INIT_DEVICE_MODE;
    void setBufferPtr(bufferControl *newPtr);
    double scopeGain = 0.5;
    int dutyTemp = 0;
    unsigned char *outBuffers[2];
    unsigned int bufferLengths[2];
private:
    //
    PKISO_CONTEXT isoCtx[NUM_FUTURE_CTX];
    KOVL_HANDLE ovlkHandle[NUM_FUTURE_CTX];
    KOVL_POOL_HANDLE ovlPool;
    unsigned char dataBuffer[NUM_FUTURE_CTX][ISO_PACKET_SIZE*ISO_PACKETS_PER_CTX];
    QTimer *isoTimer;
    unsigned char currentWriteBuffer = 0;
    //
    unsigned long timerCount = 0;
    //
    unsigned char fGenTriple=0;
    KUSB_HANDLE handle = NULL;
    unsigned short gainMask = 2056;
    KSTM_HANDLE stm_handle = NULL;
    unsigned char pipeID = 0x83;
    unsigned char usbInit(ULONG VIDin, ULONG PIDin);
    void usbSendControl(int RequestType, int Request, int Value, int Index, int Length, unsigned char *LDATA);
    void xmegaBreak(void);
    unsigned char usbIsoInit(void);
    int fGenChannel;
    functionGenControl *fGenPtr_CH1 = NULL, *fGenPtr_CH2 = NULL;
    bufferControl *bufferPtr = NULL;
    int dutyPsu = 0;
    QTimer *psuTimer;
    bool firstConnect = false;
    double currentPsuVoltage;
    KLST_DEVINFO_HANDLE deviceInfo = NULL;
    WINUSB_PIPE_INFORMATION pipeInfo;
    UINT deviceCount = 0;
    UCHAR pipeIndex = 0;
    DWORD ec = ERROR_SUCCESS;
    KLST_HANDLE deviceList = NULL;
    int digitalPinState = 0;
signals:
    void sendClearBuffer(bool ch3751, bool ch3752, bool ch750);
    void setVisible_CH2(bool visible);
    void gainBuffers(double multiplier);
    void disableWindow(bool enabled);
    void enableMMTimer();
    void checkXY(bool);
public slots:
    void setPsu(double voltage);
    void setFunctionGen(int channel, functionGenControl *fGenControl);
    void setDeviceMode(int mode);
    void newDig(int digState);
    void psuTick(void);
    void setGain(double newGain);
    void avrDebug(void);
    void isoTimerTick(void);
};

#endif // WINUSBDRIVER_H
