#include "winusbdriver.h"

winUsbDriver::winUsbDriver(QWidget *parent) : QLabel(parent)
{
    qDebug() << "Making USB Driver invisible!!";
    this->hide();
    //Load stack so that reset signal can be sent
    usbInit(0x03eb, 0xa000);
    setDeviceMode(deviceMode);
    newDig(digitalPinState);
    usbIsoInit();

    psuTimer = new QTimer();
    psuTimer->setTimerType(Qt::PreciseTimer);
    psuTimer->start(PSU_PERIOD);
    connect(psuTimer, SIGNAL(timeout()), this, SLOT(psuTick()));
}

unsigned char winUsbDriver::usbInit(ULONG VIDin, ULONG PIDin){
    unsigned char success;

    //qDebug() << "usbInit() entered";
    if (!LstK_Init(&deviceList, (KLST_FLAG) 0))	{
        qDebug("Error initializing device list");
        return 0;
    } //else qDebug() << "Device List initialised!";

    LstK_Count(deviceList, &deviceCount);
    if (!deviceCount) {
        qDebug("Device list empty");
        LstK_Free(deviceList);	// If LstK_Init returns TRUE, the list must be freed.
        return 0;
    } //else qDebug() << "Device Count initialised!";

    //qDebug("Looking for device VID = %04X, PID = %04X\n", VIDin, PIDin);

    LstK_FindByVidPid(deviceList, VIDin, PIDin, &deviceInfo);
    LstK_Free(deviceList);
    if (deviceInfo){
        /*qDebug("Using %04X:%04X (%s): %s - %s\n",
            deviceInfo->Common.Vid,
            deviceInfo->Common.Pid,
            deviceInfo->Common.InstanceID,
            deviceInfo->DeviceDesc,
            deviceInfo->Mfg);
            Causes exceptions in debug mode!!!!*/
    }
    else {
        qDebug("Could not find device VID = %04X, PID = %04X", VIDin, PIDin);
        return 0;
    }

    success = UsbK_Init(&handle, deviceInfo);
    if (!success){
        ec = GetLastError();
        qDebug("UsbK_Init failed. ErrorCode: %08Xh", ec);
    } else qDebug("Device opened successfully!");

    return success;
}

void winUsbDriver::usbSendControl(int RequestType, int Request, int Value, int Index, int Length, unsigned char *LDATA){

    //////////////////////////////////////////////////////////////////////////////////////////
    //IF YOU'RE SEEING AN ERROR, CHECK THAT REQUESTTYPE AND REQUEST ARE FORMATTED AS HEX
    //////////////////////////////////////////////////////////////////////////////////////////

    WINUSB_SETUP_PACKET setupPacket;
    unsigned char controlSuccess;
    UINT bytesTransferred = 0;
    unsigned char *controlBuffer;

    if (handle==NULL){
        qDebug("Null handle error in usbSendControl");
        return;
    }
    setupPacket.RequestType = (UCHAR) RequestType;
    setupPacket.Request = (UCHAR) Request;
    setupPacket.Value = (USHORT) Value;
    setupPacket.Index = (USHORT) Index;
    setupPacket.Length = (USHORT) Length;

    //qDebug("RequestType = %x, Request = %x, Value = %u, Index = %u, Length = %u", setupPacket.RequestType, setupPacket.Request, setupPacket.Value, setupPacket.Index, setupPacket.Length);

    if (LDATA==NULL){
        controlBuffer = (unsigned char *) malloc(256);
    }
    else controlBuffer = LDATA;

    //qDebug() << controlBuffer;
    //qDebug() << handle;

    //qDebug("SENDING CONTROL PACKET\n");
    controlSuccess = UsbK_ControlTransfer(handle, setupPacket, controlBuffer, setupPacket.Length, &bytesTransferred, NULL);
    if (controlSuccess) {
        qDebug("%d BYTES TRANSFERRED", bytesTransferred);
    }
    else qDebug("usbSendControl failed");

    if(LDATA == NULL){
        free(controlBuffer);
    }
}

void winUsbDriver::setPsu(double voltage){

    qDebug() << "New voltage =" << voltage;
    currentPsuVoltage = voltage;
    //if(deviceMode > 6) qFatal("setPsu is not configured for mode 7!!!");
    double vinp = voltage/11;
    double vinn = 0;
    //double vref = 1.65;
    double gainPsu = 1;

    dutyPsu = (int) ((vinp - vinn)/vref * gainPsu * PSU_ADC_TOP);

    qDebug() << "Going to send value " << dutyPsu;
}

void winUsbDriver::setFunctionGen(int channel, functionGenControl *fGenControl){
        ////////////////////////////
       ////NO RESIZING (YET)!!!////
      ////////////////////////////

        //////////////////////////////////////
       //// CH1 is AUX!!  CH2 is "MAIN"!!////
      //////////////////////////////////////

    int length, maxLength, numDivides, maxDivides;
    double freq, amplitude, offset;
    unsigned char *samples;

    //For recalling on crash.
    fGenChannel = channel;
    if (channel == 0) fGenPtr_CH1 = fGenControl;
    else fGenPtr_CH2 = fGenControl;

    //Reading in data
    if (channel == 0){
        length = fGenControl->length_CH1;
        freq = fGenControl->freq_CH1;
        amplitude = fGenControl->amplitude_CH1;
        offset = fGenControl->offset_CH1;
        samples = (unsigned char *) malloc(length);
        memcpy(samples, fGenControl->samples_CH1, (unsigned int) length);
        numDivides = fGenControl->divisibility_CH1;
    }
    else if(channel == 1){
        length = fGenControl->length_CH2;
        freq = fGenControl->freq_CH2;
        amplitude = fGenControl->amplitude_CH2;
        offset = fGenControl->offset_CH2;
        samples = (unsigned char *) malloc(length);
        memcpy(samples, fGenControl->samples_CH2, (unsigned int) length);
        numDivides = fGenControl->divisibility_CH2;
    }

    //Triple mode
    if ((amplitude+offset) > FGEN_LIMIT){
        amplitude = amplitude / 3;
        offset = offset / 3;
        fGenTriple |= ((unsigned char) !channel + 1);
    }
    else fGenTriple &= ((unsigned char) (254 - !channel));

    //qDebug() << "fGenTriple = " << fGenTriple << "fGenControl = " << fGenControl << "length = " << length << "freq = " << freq << "amplitude = " << amplitude << "offset = " << offset << "samples = " << samples;

    //Waveform scaling in V
    double tempDouble;
    amplitude = (amplitude * 255) / FGEN_LIMIT;
    offset = (offset * 255) / FGEN_LIMIT;
    if (offset<FGEN_OFFSET){
        if (amplitude>5)
            amplitude -= FGEN_OFFSET;
        else
            amplitude = 0;
        offset = FGEN_OFFSET;
    }

#ifdef INVERT_TRIPLE
    unsigned char fGenTemp = 0;
    fGenTemp |= (fGenTriple & 0x01)<<1;
    fGenTemp |= (fGenTriple & 0x02)>>1;
    usbSendControl(0x40, 0xa4, fGenTemp, 0, 0, NULL);
#else
    usbSendControl(0x40, 0xa4, fGenTriple, 0, 0, NULL);
#endif

    //Applying amplitude and offset to all samples.
    for (int i=0;i<length;i++){
        tempDouble = (double) samples[i];
        tempDouble *= amplitude;
        tempDouble /= 255;
        tempDouble += offset;
        samples[i] = (unsigned char) tempDouble;
    }


    //Need to increase size of wave if its freq too high, or too low!
    maxDivides = numDivides;
    bool loop_entered = false;
    unsigned char *tempSamples = (unsigned char *) malloc(0);

    while(length * freq > DAC_SPS){
        loop_entered = true;
        numDivides--;
        if (numDivides==0){
            qDebug("numDivides = 0 - in T-stretching of winUsbDriver:: setFunctionGen");
        }

        int shiftTemp = (maxDivides - numDivides);
        length = length >> 1;

        free(tempSamples);
        tempSamples = (unsigned char *) malloc(length);
        for (int i=0; i<length;i++){
            tempSamples[i] = samples[i<<shiftTemp];
        }
    }
    if(loop_entered){
        samples = tempSamples;
    }

    //Scaling in t here:
    // Something something maxLength something

    //Timer Setup
    int validClockDivs[7] = {1, 2, 4, 8, 64, 256, 1024};

    int clkSetting;
    for(clkSetting = 0; clkSetting<7; clkSetting++){
        if ( (CLOCK_FREQ / (length * validClockDivs[clkSetting] * freq)) < 65535 )
             break;
    }
    int timerPeriod = CLOCK_FREQ / (length * freq * validClockDivs[clkSetting]);

    clkSetting++;  // Change from [0:n] to [1:n]

    if(deviceMode == 5){
        qDebug("DEVICE IS IN MODE 5");
    }

    //qDebug() << "First three samples:" << samples[0] << samples[1] << samples[2];

    if(channel){
    usbSendControl(0x40, 0xa1, timerPeriod, clkSetting, length, samples);
    }
    else usbSendControl(0x40, 0xa2, timerPeriod, clkSetting, length, samples);
    free(tempSamples);
    return;
}

void winUsbDriver::xmegaBreak(void){
    usbSendControl(0x40, 0xa0, 0, 0, 0, NULL);
}

void winUsbDriver::newDig(int digState){
    qDebug() << "newDig";
    digitalPinState = digState;
    usbSendControl(0x40, 0xa6, digState, 0, 0, NULL);
}

unsigned char winUsbDriver::usbIsoInit(void){
    int n;
    bool success;
    DWORD errorCode = ERROR_SUCCESS;

    success = OvlK_Init(&ovlPool, handle, MAX_OVERLAP, (KOVL_POOL_FLAG) 0);
    if(!success){
        errorCode = GetLastError();
        qDebug() << "OvlK_Init failed with error code" << errorCode;
        return 0;
    }
    success = UsbK_ResetPipe(handle, pipeID);
    if(!success){
        errorCode = GetLastError();
        qDebug() << "UsbK_ResetPipe failed with error code" << errorCode;
        return 0;
    }

    for(n=0;n<NUM_FUTURE_CTX;n++){
        success = IsoK_Init(&isoCtx[n], ISO_PACKETS_PER_CTX, n*ISO_PACKETS_PER_CTX);
        if(!success){
            errorCode = GetLastError();
            qDebug() << "IsoK_Init failed with error code" << errorCode;
            qDebug() << "n =" << n;
            return 0;
        }

        success = IsoK_SetPackets(isoCtx[n], ISO_PACKET_SIZE);
        if(!success){
            errorCode = GetLastError();
            qDebug() << "IsoK_SetPackets failed with error code" << errorCode;
            qDebug() << "n =" << n;
            return 0;
        }

        success = OvlK_Acquire(&ovlkHandle[n], ovlPool);
        if(!success){
            errorCode = GetLastError();
            qDebug() << "OvlK_Acquire failed with error code" << errorCode;
            qDebug() << "n =" << n;
            return 0;
        }

        success = UsbK_IsoReadPipe(handle, pipeID, dataBuffer[n], sizeof(dataBuffer[n]), (LPOVERLAPPED) ovlkHandle[n], isoCtx[n]);
    }
    qDebug() << "Iso Setup Successful";
    return 1;
}

char *winUsbDriver::isoRead(){
    unsigned char *returnBuffer;

    bool success;
    DWORD errorCode = ERROR_SUCCESS;
    unsigned int allocBytes = 0;
    int n, i, j;
    unsigned int returnBufferOffset = 8;
    unsigned int dataBufferOffset;

    //Calculate how much data needs to be allocated
    for (n=0; n<NUM_FUTURE_CTX; n++){
        if(OvlK_IsComplete(ovlkHandle[n])){
            for(i=0;i<isoCtx[n]->NumberOfPackets;i++){
                allocBytes +=isoCtx[n]->IsoPackets[i].Length;
            }
        }
    }
    //Allocate it (why am I allocating a new buffer every time?)
    returnBuffer = (unsigned char *) malloc(allocBytes + 8); //Extra 8 bytes for the "header" that contains length of "packet" as unsigned int.
    ((unsigned int *) returnBuffer)[0] = allocBytes;

    //Fill the memory with the relevant samples
    //Note that this will return garbage if more than one isoCtx has completed.
    //The above condition should only ever occur if the polling rate is slower than ISO_PACKETS_PER_CTX, though.
    //(Or if some heinous external force stops the polling and gives it a long "temporary period"!!)
    for (n=0; n<NUM_FUTURE_CTX; n++){
        if(OvlK_IsComplete(ovlkHandle[n])){
            //qDebug("Transfer %d is complete!!", n);

            //Copy the iso packet
            char currentString[8];
            for(i=0;i<isoCtx[n]->NumberOfPackets;i++){
                dataBufferOffset = isoCtx[n]->IsoPackets[i].Offset;
                //memcpy(&returnBuffer[returnBufferOffset], &dataBuffer[n][dataBufferOffset], isoCtx[n]->IsoPackets[i].Length);
                for(int j=0;j<isoCtx[n]->IsoPackets[i].Length;j++){
                    returnBuffer[returnBufferOffset+j] = dataBuffer[n][dataBufferOffset+j];
                }
                returnBufferOffset += isoCtx[n]->IsoPackets[i].Length;
                }
            }


            //Setup next transfer
            UINT oldStart = isoCtx[n]->StartFrame;
            success = IsoK_ReUse(isoCtx[n]);
            if(!success){
                errorCode = GetLastError();
                qDebug() << "IsoK_Init failed with error code" << errorCode;
                qDebug() << "n =" << n;
                return (char*) returnBuffer;
            }
            isoCtx[n]->StartFrame = oldStart + ISO_PACKETS_PER_CTX*NUM_FUTURE_CTX;
            //qDebug() << oldStart;
            //qDebug() << isoCtx[n]->StartFrame;
            //qDebug() << handle;

            success = OvlK_ReUse(ovlkHandle[n]);
            if(!success){
                errorCode = GetLastError();
                qDebug() << "OvlK_ReUse failed with error code" << errorCode;
                qDebug() << "n =" << n;
                return (char*) returnBuffer;
            }
            success = UsbK_IsoReadPipe(handle, pipeID, dataBuffer[n], sizeof(dataBuffer[n]), (LPOVERLAPPED) ovlkHandle[n], isoCtx[n]);
        }
    //qDebug("%d bytes need to be allocated", allocBytes);

    return (char*) returnBuffer;
}

winUsbDriver::~winUsbDriver(void){
    qDebug() << "\n\nwinUsbDriver destructor ran!";
    unsigned char success;
    DWORD errorCode = ERROR_SUCCESS;

    UsbK_Free(handle);
}

void winUsbDriver::setBufferPtr(bufferControl *newPtr){
    bufferPtr = newPtr;
}

void winUsbDriver::setDeviceMode(int mode){
    int oldMode = deviceMode;
    deviceMode = mode;
    usbSendControl(0x40, 0xa5, (mode == 5 ? 0 : mode), gainMask, 0, NULL);

    if (fGenPtr_CH1!=NULL) setFunctionGen(0, fGenPtr_CH1);
    if (fGenPtr_CH2!=NULL) setFunctionGen(1, fGenPtr_CH2);

    //switch on new deviceMode!!
    switch(deviceMode){
        case 0:
            if(oldMode > 2) sendClearBuffer(1,0,0);
            setVisible_CH2(0);
            checkXY(0);
            break;
        case 1:
            if(oldMode > 2) sendClearBuffer(1,0,0);
            sendClearBuffer(0,1,0);
            setVisible_CH2(1);
            checkXY(0);
            break;
        case 2:
            if(oldMode > 2) sendClearBuffer(1,0,0);
            sendClearBuffer(0,1,0);
            setVisible_CH2(1);
            break;
        case 3:
            if(oldMode != 4) sendClearBuffer(1,0,0);
            sendClearBuffer(0,1,0);
            setVisible_CH2(0);
            checkXY(0);
            break;
        case 4:
            if(oldMode != 3) sendClearBuffer(1,0,0);
            sendClearBuffer(0,1,0);
            setVisible_CH2(1);
            checkXY(0);
            break;
        case 5:
            setVisible_CH2(0);
            checkXY(0);
            break;
        case 6:
            sendClearBuffer(0,0,1);
            setVisible_CH2(0);
            checkXY(0);
            break;
        case 7:
            sendClearBuffer(1,0,0);
            enableMMTimer();
            checkXY(0);
            break;
        default:
            qFatal("Error in winUsbDriver::setDeviceMode.  Invalid device mode.");
    }

}

void winUsbDriver::psuTick(){
    if(dutyTemp == dutyPsu) return;

    if (dutyTemp > dutyPsu){
        if((dutyTemp - dutyPsu)> PSU_STEP){
            dutyTemp -= PSU_STEP;
        } else dutyTemp = dutyPsu;
    }

    if (dutyTemp < dutyPsu){
        if((dutyPsu - dutyTemp)> PSU_STEP){
            dutyTemp += PSU_STEP;
        } else dutyTemp = dutyPsu;
    }

    if ((dutyTemp>106) || (dutyTemp<21)){
        qDebug("PSU DUTY CYCLE TOO LARGE (could underflow on SOF)!!!  ABORTING!!!");
    }
    usbSendControl(0x40, 0xa3, dutyTemp, 0, 0, NULL);
}

void winUsbDriver::setGain(double newGain){
    if (newGain == scopeGain) return; //No update!
    gainBuffers(scopeGain/newGain);
    scopeGain = newGain;
    if (newGain == 0.5){
        gainMask = 7<<2 | 7<<10;
    }
    else gainMask = (unsigned short)(log2(newGain))<<2 | (unsigned short)(log2(newGain))<<10;
    qDebug("newGain = %f", newGain);
    qDebug("gainMask = %d", gainMask);
    usbSendControl(0x40, 0xa5, deviceMode, gainMask, 0, NULL);
}

void winUsbDriver::avrDebug(void){
    usbSendControl(0x40, 0xa0, 0, 0, 0, NULL);
}
