//MATLAB INCLUDES
#include <matrix.h>
#include <mex.h>

//LIBUSBK INCLUDES
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include "libusbk.h"

#define DEBUG_SHOWCTRL 1
#define MAX_TRANSFER_SIZE 1023
#define MAX_PENDING_TRANSFERS 16384
#define MAX_PENDING_IO 16

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    //Vars:
        //Imports
            char* HANDLE_CHAR;
            KUSB_HANDLE handle = NULL;
            unsigned char pipeID = 0x83;
        //Internals
            unsigned char success;
            unsigned int transferred;
            DWORD errorCode = ERROR_SUCCESS;
            KSTM_HANDLE stm_handle = NULL;
        //Exports
            char stm_handle_string[17];

    
    //Initialise Data
        HANDLE_CHAR = mxArrayToString(prhs[0]);  
        sscanf(HANDLE_CHAR, "%16x", &handle);
        
        //Actual Function
        success = StmK_Init(
                      &stm_handle,
                      handle,
                      pipeID,
                      MAX_TRANSFER_SIZE,
                      MAX_PENDING_TRANSFERS,
                      MAX_PENDING_IO,
                      NULL,
                      KSTM_FLAG_NONE);
            if (!success){
                errorCode = GetLastError();
                printf("StmK_Init failed. ErrorCode: %08Xh\n", errorCode);
            }
            
            success = StmK_Start(stm_handle);
            if (!success){
                errorCode = GetLastError();
                printf("StmK_Start failed. ErrorCode: %08Xh\n", errorCode);
            }
            else printf("[Start Stream] successful!\n");
            printf("stm_handle is %d bytes long\n", sizeof(stm_handle));
            printf("Value of stm_handle is 0x%16x\n\n\n\n\n\n", stm_handle);
            

    //Export Data
            
    sprintf(stm_handle_string, "%016x", stm_handle);    
    plhs[0] = mxCreateString(stm_handle_string);  
            
    return;
}