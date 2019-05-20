// RfCardDlg.cpp : implementation file
//

#include "stdafx.h"
#include "RfCard.h"
#include "RfCardDlg.h"
#include "fstream.h"
#include "iostream.h"

//#include "mwrf32.h"
//#include "dcrf32.h"
#include <afxmt.h>

#define	RF35	0
#define RF100	1

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
static unsigned char DataLength=0;
static unsigned char BlockSector=0;
static unsigned char KeyA[6]={0xff,0xff,0xff,0xff,0xff,0xff};
static unsigned char KeyB[6]={0xff,0xff,0xff,0xff,0xff,0xff};
static unsigned int	 Num_strtotal=0;
static	unsigned char Modecg=0xc6;
CString str_ID[60000],str_x[60000],str_y[60000],user_ID;
static unsigned char Block3_Ddata[16],user_IDdata[10]={0x49,0x00,0x2B,0x9D};//49002B9E
static long  fig_times=0; 
static long  Error_sum=0; 
static BOOL  bSavelogfile=0; 
	static long  Assert_sum=0; 
	static long  bJudgError=0; 
static BOOL  bbJudgError =0;
/////////////////////
	HANDLE icdev;
		__int16 st;
	static	unsigned long Snr;
	unsigned char m_keymode=0;
///////////////////////


long time_0 = 0;
long time_1 = 0;
long time_2 = 0;
//EE series
#define SERIES_NUM 4
#define MAXTYPE 10
static char* chDeviceList[SERIES_NUM][MAXTYPE] = {
	{"IS24C01", "IS24C02", "IS24C04", "IS24C08", "IS24C16", "IS24C32", "IS24C64", "IS24C128", "IS24C256"},
	{"IS34C02"},
	{"IS25C01", "IS25C02", "IS25C04", "IS25C08", "IS25C16", "IS25C32", "IS25C64", "IS25C128", "IS25C256"},
	{"IS93C46", "IS93C56", "IS93C66", "IS93C76", "IS93C86"}
};

////////////////////////////////////For communication
unsigned char SendBuffer[3072];
int g_sendLen = 0;
BOOL g_bExit = FALSE;
BOOL g_bPoll_mcu0 = FALSE;
CWinThread *g_pollThread = NULL;
CWinThread *g_pollWriteThread = NULL;

CCriticalSection g_criticalsection; 
CEvent endEvent_mcu0(FALSE, TRUE); //manual reset
CEvent threadEndEvent(FALSE, TRUE); //manual reset

CEvent ReadComEvent(FALSE, TRUE); //manual reset

static UartDataHead g_uartWriteDataHead = {0};
CMutex s_uartWriteDataMutex(FALSE, "writeUart");

CEvent WriteComEvent(FALSE, TRUE); //manual reset
CEvent GuiThreadEvent(FALSE, TRUE); //manual reset
CEvent WriteEndEvent(FALSE, TRUE); //manual reset


CMutex s_uartReadDataMutex(FALSE, "readUart");
CMutex s_readFromUart_mutex(FALSE, "readfromuart");


CSemaphore g_clsSemaphore(0,1);


static AT_RESPONSE_TAB g_extAtResTab[]=
{
		{EXT_AT_TOKEN_MYNETWRITE,"AT$MYNETWRITE","\r\n$MYNETWRITE:",app_extAt_myNetWrite_res},
		{EXT_AT_TOKEN_URC,NULL,"\r\n$MYURC",app_extAt_myURC_res},
};


ReceiveData g_data[MAX_RESPONSE_TIMES];
NetSendData g_netSendData = {0};

int g_nResponseTimes = 0;
int g_nLastIndex = 0;
DWORD g_nWaitMs = 0;

int g_nPoolingTime = 0;

Port g_port;
HANDLE g_hDriver = NULL;
DRIVER_CONTEXT DrvCtx;
////////////////////////////////////////End

//USB operation
void ResetPipe(HANDLE hDevice);
BOOL ConnectUsb(); //get usb driver handle
void   DisconnectUsb(); //disconnet usb driver
HANDLE OpenUsbPort(); //get one usb device handle
void CloseUsbPort();
BOOL ReadUsbPort(HANDLE hPort, unsigned char *buffer, unsigned int nLen, LPDWORD lpBytesRead);
BOOL WriteUsbPort(HANDLE hPort, unsigned char *buffer, unsigned int nLen);
//COM operation
HANDLE OpenComPort(int nComNum, unsigned long BaudRate);
void CloseComPort();
BOOL ReadComPort(HANDLE hComPort, unsigned char *buffer, unsigned int nLen, LPDWORD lpBytesRead);
BOOL WriteComPort(HANDLE hComPort, char *buffer, unsigned int nLen);
int  app_at_enQueue_uartdata(UartDataHead *pHead,char *pData,int dataLen,CMutex *mutex);
int app_at_get_uartdata_queue_num(UartDataHead *pHead,CMutex *mutex);
int app_at_deQueue_uartdata(UartDataHead *pHead,char** pData,CMutex *mutex);

int  app_at_enQueue_uartdata(UartDataHead *pHead,char *pData,int dataLen,CMutex *mutex)
{
	UartDataNode *pNode = NULL;
	int dataNum = -1;
	
	if(NULL == pHead || NULL ==  pData)
	{
		return -1;
	}

	pNode = (UartDataNode *)malloc(sizeof(UartDataNode));
	if(NULL == pNode)
	{
		return -1;
	}
	else
	{
		memset(pNode,0,sizeof(UartDataNode));
		pNode->uartData.pUartData = pData;
		pNode->uartData.dataLen = dataLen;
	}

	if(NULL != mutex)
		mutex->Lock();
	
	if(0 == pHead->dataNum)
	{
		pHead->pHeadNode = pNode;
		pHead->pRearNode = pNode;
	}
	else
	{
		pHead->pRearNode->pNext = pNode;
		//pNode->pPre = pHead->pRearNode;

		pHead->pRearNode = pNode;
	}

	pHead->dataNum++;	
	dataNum = pHead->dataNum;

	if(NULL != mutex)
		mutex->Unlock();

	
	return dataNum;
}

int app_at_deQueue_uartdata(UartDataHead *pHead,char** pData,CMutex *mutex)
{
	UartDataNode *pNode = NULL;
	int dataLen = 0;
	

	if(NULL == pHead || NULL ==  pData)
	{
		return -1;
	}
	
	if(NULL != mutex)
		mutex->Lock();
	
	if(0 == pHead->dataNum)
	{
		if(NULL != mutex)
		mutex->Unlock();
		return -1;
	}

	if(1 == pHead->dataNum)
	{
		pNode = pHead->pHeadNode;
		pHead->pHeadNode = NULL;
		pHead->pRearNode = NULL;
	}
	else
	{
		pNode = pHead->pHeadNode;
		pHead->pHeadNode = pNode->pNext;
	}

	pHead->dataNum--;
	
	if(NULL != mutex)
	mutex->Unlock();

	if(NULL != pNode)
	{
		*pData = pNode->uartData.pUartData;
		dataLen = pNode->uartData.dataLen;

		free(pNode);
		pNode = NULL;
		
		return dataLen;	
	}
	else
	{
		return -1;
	}	
}

void app_extAt_myNetWrite_res(ReceiveData *repsonse_buff)
{
	int                 socketId;
	int count = 0;
	/*get socket ID*/
	if(false == getExtendedParameter (repsonse_buff, &socketId, ULONG_MAX))
	{
		//error
		return;
	}
	/*get data number*/
	if(false == getExtendedParameter (repsonse_buff, &count, ULONG_MAX))
	{
		//error
		return;
	}
	g_netSendData.partCount = count;
	
	ReadComEvent.SetEvent();
	return;
}

void app_extAt_myURC_res(ReceiveData *repsonse_buff)
{
	return;
}

void app_extAt_normal_res(ReceiveData *repsonse_buff)
{
	return;
}


int app_at_get_uartdata_queue_num(UartDataHead *pHead,CMutex *mutex)
{
	int dataNum = 0;
	

	if(NULL == pHead )
	{
		return -1;
	}
	if(NULL != mutex)
	{
		mutex->Lock();
	}
	
	dataNum = pHead->dataNum;

	if(NULL != mutex)
	{
		mutex->Unlock();
	}

	
	return dataNum;
}



////////////////////////////////////For usb///////////////////////////////
BOOL DLLCALLCONV DeviceAttach(WDU_DEVICE_HANDLE hDevice, 
    WDU_DEVICE *pDeviceInfo, PVOID pUserData)
{
    DRIVER_CONTEXT *pDrvCtx = (DRIVER_CONTEXT *)pUserData;
    DEVICE_CONTEXT *pDevCtx, **ppDevCtx;
    DWORD dwInterfaceNum = pDeviceInfo->pActiveInterface->pActiveAltSetting->Descriptor.bInterfaceNumber;
    DWORD dwAlternateSetting = 0;
    
    /*
    // NOTE: For dwAlternateSetting != 0, call WDU_SetInterface() here
    // (by default alternate setting 0 is set as the active alternate setting)
    DWORD dwAttachError;

    dwAttachError = WDU_SetInterface(hDevice, dwInterfaceNum, dwAlternateSetting);
    if (dwAttachError)
    {
        ERR("DeviceAttach: WDU_SetInterface failed (num. %ld, alternate %ld) device 0x%p: error 0x%lx (\"%s\")\n",
            dwInterfaceNum, dwAlternateSetting, hDevice, 
            dwAttachError, Stat2Str(dwAttachError));

        return FALSE;
    }
    */

    TRACE("DeviceAttach: received and accepted attach for vendor id 0x%x, "
        "product id 0x%x, interface %ld, device handle 0x%p\n",
        pDeviceInfo->Descriptor.idVendor, pDeviceInfo->Descriptor.idProduct,
        dwInterfaceNum, hDevice);
    
    // Add our device to the device list
    pDevCtx = (DEVICE_CONTEXT *)malloc(sizeof(DEVICE_CONTEXT));
    if (!pDevCtx)
    {
        TRACE("DeviceAttach: failed allocating memory\n");
        return FALSE;
    }
    BZERO(*pDevCtx);
    pDevCtx->hDevice = hDevice;
    pDevCtx->dwInterfaceNum = dwInterfaceNum;
    pDevCtx->dwVendorId = pDeviceInfo->Descriptor.idVendor;
    pDevCtx->dwProductId = pDeviceInfo->Descriptor.idProduct;
    pDevCtx->dwAlternateSetting = dwAlternateSetting;
    
    OsMutexLock(pDrvCtx->hMutex);
    for (ppDevCtx = &pDrvCtx->deviceContextList; *ppDevCtx; ppDevCtx = &((*ppDevCtx)->pNext));
    *ppDevCtx = pDevCtx;
    pDrvCtx->dwDeviceCount++;
    OsMutexUnlock(pDrvCtx->hMutex);
    
    OsEventSignal(pDrvCtx->hEvent);
    // Accept control over this device
    return TRUE;
}
VOID DLLCALLCONV DeviceDetach(WDU_DEVICE_HANDLE hDevice, PVOID pUserData)
{
    DRIVER_CONTEXT *pDrvCtx = (DRIVER_CONTEXT *)pUserData;
    DEVICE_CONTEXT **pCur;
    DEVICE_CONTEXT *pTmpDev;
    BOOL bDetachActiveDev = FALSE;

    TRACE("DeviceDetach: received detach for device handle 0x%p\n", hDevice);

    OsMutexLock(pDrvCtx->hMutex);
    for (pCur = &pDrvCtx->deviceContextList; 
        *pCur && (*pCur)->hDevice != hDevice; 
        pCur = &((*pCur)->pNext));
    
    if (*pCur == pDrvCtx->pActiveDev)
    {
        bDetachActiveDev = TRUE;
        pDrvCtx->pActiveDev = NULL;
    }

    pTmpDev = *pCur;
    *pCur = pTmpDev->pNext;
    free(pTmpDev);
    
    pDrvCtx->dwDeviceCount--;
    OsMutexUnlock(pDrvCtx->hMutex);

    // Detach callback must not return as long as hDevice is being used.
    if (bDetachActiveDev)
        OsEventWait(pDrvCtx->hDeviceUnusedEvent, INFINITE);
}
void ResetPipe(HANDLE hDevice) 
{
	unsigned char pipenumber;
    DWORD dwError = WDU_ResetPipe(hDevice, USB_PIPENUMBER);
    if (dwError)
		TRACE("DeviceDiagMenu: WDU_ResetPipe() failed!");
    else
        TRACE("DeviceDiagMenu: WDU_ResetPipe() completed successfully\n");
	pipenumber = (unsigned char)USB_PIPENUMBER+0x80;
    dwError = WDU_ResetPipe(hDevice, pipenumber);
    if (dwError)
		TRACE("DeviceDiagMenu: WDU_ResetPipe() failed!");
    else
        TRACE("DeviceDiagMenu: WDU_ResetPipe() completed successfully\n");
}
#define MAX_DEVICENUM 1
//Connect usb driver
BOOL ConnectUsb()
{
	HANDLE hDevice = NULL;
	WORD wVendorId = 0;
	WORD wProductId = 0;
    WDU_MATCH_TABLE matchTable;
    WDU_EVENT_TABLE eventTable;

	BZERO(DrvCtx);  
    wVendorId = USE_DEFAULT;
    wProductId = USE_DEFAULT;
    // use defaults
    if (wVendorId == USE_DEFAULT)
        wVendorId = DEFAULT_VENDOR_ID;
    if (wProductId == USE_DEFAULT)
        wProductId = DEFAULT_PRODUCT_ID;

    DWORD dwError = OsEventCreate(&DrvCtx.hEvent);
    if (dwError)
    {
		//MessageBox("main: OsEventCreate() failed on event!", "Error", MB_ICONEXCLAMATION | MB_OK);
    }

    dwError = OsMutexCreate(&DrvCtx.hMutex);
    if (dwError)
    {
		//MessageBox("main: OsMutexCreate() failed on mutex!", "Error", MB_ICONEXCLAMATION | MB_OK);
    }
    
    // When hDeviceUnusedEvent is not signaled, hDevice is in use, and therefore the 
    // detach callback will wait for it before returning.
    // When it is signaled - hDevice is not being used.
    dwError = OsEventCreate(&DrvCtx.hDeviceUnusedEvent);
    if (dwError)
    {
		//MessageBox("main: OsMutexCreate() failed on mutex!", "Error", MB_ICONEXCLAMATION | MB_OK);
    }
    OsEventSignal(DrvCtx.hDeviceUnusedEvent);
    
    BZERO(matchTable);
    matchTable.wVendorId = wVendorId;
    matchTable.wProductId = wProductId;

    BZERO(eventTable);
    eventTable.pfDeviceAttach = DeviceAttach;
    eventTable.pfDeviceDetach = DeviceDetach;
    eventTable.pUserData = &DrvCtx;

    dwError = WDU_Init(&g_hDriver, &matchTable, 1, &eventTable, DEFAULT_LICENSE_STRING, WD_ACKNOWLEDGE);
    if (dwError)
    {
   		//MessageBox("main: failed to initialize USB driver!", "Error", MB_ICONEXCLAMATION | MB_OK);
		return NULL;
    }
   
   // printf("Please make sure the device is attached:\n");

    // Wait for the device to be attached
	int nDevice = 0;
	do
	{
    dwError = OsEventWait(DrvCtx.hEvent, ATTACH_EVENT_TIMEOUT);
	Sleep(500);

    if (dwError)
    {
        if (dwError==WD_TIME_OUT_EXPIRED)
           // ERR("Timeout expired for connection with the device.\n" 
             //   "Check that the device is connected and try again.\n");
			TRACE("Timeout expired for connection with the device.");
		else
           // ERR("main: OsEventWait() failed on event 0x%p: error 0x%lx (\"%s\")\n",
             //   DrvCtx.hEvent, dwError, Stat2Str(dwError));
			 TRACE("main: OsEventWait() failed on event");
		break;
    }
	}while (++nDevice < MAX_DEVICENUM);

    if (!DrvCtx.dwDeviceCount)
    {
        TRACE("No Devices are currently connected.\n");
		return FALSE;
    }

    OsMutexLock(DrvCtx.hMutex);
    if (!DrvCtx.dwDeviceCount)
    {
        OsMutexUnlock(DrvCtx.hMutex);
    }
        
    if (!DrvCtx.pActiveDev)
        DrvCtx.pActiveDev = DrvCtx.deviceContextList;
        
    OsMutexUnlock(DrvCtx.hMutex);

    OsEventReset(DrvCtx.hDeviceUnusedEvent);
        
    OsMutexLock(DrvCtx.hMutex);
    //hDevice =DrvCtx.pActiveDev->hDevice;
    OsMutexUnlock(DrvCtx.hMutex);

	return TRUE;
}
//Disconnect usb driver
void DisconnectUsb()
{
	if (g_hDriver)
	{
		if (DrvCtx.hEvent)
			OsEventClose(DrvCtx.hEvent);
		if (DrvCtx.hMutex)
			OsMutexClose(DrvCtx.hMutex);
		if (DrvCtx.hDeviceUnusedEvent)
			OsEventClose(DrvCtx.hDeviceUnusedEvent);
	}
    if (g_hDriver && (DrvCtx.dwDeviceCount > 0))
        WDU_Uninit(g_hDriver);
	g_hDriver = NULL;
}
//Close usb port
void CloseUsbPort()
{
	g_port.hPort = NULL;
}
//Open usb port
HANDLE OpenUsbPort()
{
	HANDLE hDevice = NULL;

	if (g_hDriver)
	{
		int nIndex = 0;
		DEVICE_CONTEXT *pDevice = DrvCtx.deviceContextList;
		if (pDevice)
			hDevice = pDevice->hDevice;
	}

	return hDevice;
}
BOOL ReadUsbPort(HANDLE hPort, unsigned char *buffer, unsigned int nLen, LPDWORD lpBytesRead)
{
	if (hPort == NULL)
		return FALSE;

	unsigned char pipenumber;
	pipenumber = (unsigned char)USB_PIPENUMBER+0x80;
	DWORD dwError = WDU_TransferIsoch(hPort, pipenumber,
    1, 0, buffer, nLen,  lpBytesRead,TRANSFER_TIMEOUT);


    if (dwError)
       //  ERR("ReadWritePipesMenu: WDU_Transfer() failed: error 0x%x (\"%s\")\n",
       //      dwError, Stat2Str(dwError));
		//MessageBox("WDU_Transfer() failed: error!", "Error", MB_ICONEXCLAMATION | MB_OK);
		return FALSE;

	return TRUE;
}
BOOL WriteUsbPort(HANDLE hPort, unsigned char *buffer, unsigned int nLen)
{
	if (hPort == NULL)
		return FALSE;

	DWORD dwError, dwBytesTransferred = 0;
	unsigned char SetupPacket[8];

	//char testBuf[256];
	//strcpy(testBuf, "Before sending command\n");
	//if (pFile)
	//	fwrite(testBuf, strlen(testBuf), 1, pFile);

	if(((nLen<=16)&&(USB_PIPENUMBER<2))|((nLen<=64)&&(USB_PIPENUMBER==2)))
	{
		if(USB_PIPENUMBER==0)
		{
			for(unsigned int i=0;i<8;i++)
				SetupPacket[i] = buffer[i];
			nLen=8;
			dwError = WDU_Transfer(hPort, USB_PIPENUMBER, 0, USB_ISOCH_ASAP|USB_ISOCH_RESET, buffer, 
								   nLen, &dwBytesTransferred, SetupPacket, TRANSFER_TIMEOUT);
		}
		else
			dwError = WDU_TransferIsoch(hPort, USB_PIPENUMBER,
					0, 0, buffer, nLen, &dwBytesTransferred,TRANSFER_TIMEOUT);

		if (dwError)
			//AfxMessageBox("WDU_Transfer() failed: error!");
			return FALSE;
	}
	else
		//AfxMessageBox("Please input less 16bytes (pipe=0/1) or 64bytes (pipe=2) !");
		return FALSE;

	//strcpy(testBuf, "After sending command\n");
	//if (pFile)
	//	fwrite(testBuf, strlen(testBuf), 1, pFile);

	return TRUE;
}
/////////////////////////////////////////End for usb///////////////////////////////


//////////////////////////////////////For COM//////////////////////////////////////
//Open com port
HANDLE OpenComPort(int nComNum, unsigned long BaudRate)
{
	HANDLE hComPort = NULL;
	char *PortName;
    char *com[]={"COM1", "COM2", "COM3", "COM4", "COM5"};
    BOOL bOpenResult = TRUE;
	PortName = com[nComNum];
	COMMTIMEOUTS CommTimeOuts;
    DCB	dcb ;
    
	switch(BaudRate)
    {
        case 1200   :   BaudRate = CBR_1200     ;    break; 
        case 2400   :   BaudRate = CBR_2400     ;    break;            
        case 4800   :   BaudRate = CBR_4800     ;    break;
        case 9600   :   BaudRate = CBR_9600     ;    break;
        case 14400  :   BaudRate = CBR_14400    ;    break;
        case 19200  :   BaudRate = CBR_19200    ;    break;
        case 38400  :   BaudRate = CBR_38400    ;    break;
        case 57600  :   BaudRate = CBR_57600    ;    break;
        case 115200 :   BaudRate = CBR_115200   ;    break;
        default     :   BaudRate = CBR_57600     ; 
                        printf(" Baud rate not supported!!\n Opening com port at %ld baudrate\n",BaudRate);
                        break;
    }

	//open com
	hComPort = CreateFile(PortName, 
				GENERIC_READ|GENERIC_WRITE,
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
	if (hComPort == INVALID_HANDLE_VALUE)
	{
		printf(("Open %s... failed\n", PortName));
		return NULL;
	}

	//set buffer size
	if (!SetupComm(hComPort, INPUT_BUFFERSIZE, OUTPUT_BUFFERSIZE))
	{
		printf("SetupComm failed\n");
		CloseHandle(hComPort);
		return NULL;
	}

	//purge info in buffer
	if (!PurgeComm(hComPort, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR))
	{
		printf(" PurgeComm failed\n");
		CloseHandle(hComPort);
		return NULL;
	}

	// set the time-out parameters for all read and write operations
	CommTimeOuts.ReadIntervalTimeout = 50 ;
	CommTimeOuts.ReadTotalTimeoutMultiplier =5 ;
	CommTimeOuts.ReadTotalTimeoutConstant =200 ;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0 ;
	CommTimeOuts.WriteTotalTimeoutConstant = 500 ;	
	if (!SetCommTimeouts(hComPort, &CommTimeOuts)) 
	{
		printf("SetCommTimeouts failed\n");
		CloseHandle(hComPort);
		return NULL;
	}

	//set state
	dcb.DCBlength = sizeof( DCB ) ;
	if (!GetCommState(hComPort, &dcb))
	{
		printf(" GetCommState failed\n");
		CloseHandle(hComPort);
		return NULL;
	}
	dcb.BaudRate = BaudRate;
	dcb.Parity = FALSE ;
	dcb.fBinary = TRUE ;
	dcb.Parity = NOPARITY ;
	dcb.ByteSize = 8 ;
	dcb.StopBits = ONESTOPBIT ;
// Flow Control Settings
	dcb.fRtsControl = RTS_CONTROL_DISABLE;//RTS_CONTROL_HANDSHAKE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;//DTR_CONTROL_HANDSHAKE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fAbortOnError = FALSE;
	if (!SetCommState(hComPort, &dcb))
	{
		printf(" SetCommState failed\n");
		CloseHandle(hComPort);
		return NULL;
	}

	return hComPort;
}
//close handle
void CloseComPort()
{
	if (g_port.hPort != NULL)
		CloseHandle(g_port.hPort);
	g_port.hPort = NULL;
}
//read data
BOOL ReadComPort(HANDLE hComPort, unsigned char *buffer, unsigned int nLen, LPDWORD lpBytesRead)
{
	if (hComPort == NULL)
		return FALSE;
	if (!ReadFile(hComPort, buffer, nLen, lpBytesRead, NULL))
		return FALSE;

	return TRUE;
}
//transmit data
BOOL WriteComPort(HANDLE hComPort, unsigned char *buffer, unsigned int nLen)
{
	if (hComPort == NULL)
		return FALSE;
	DWORD dwBytesWritten;
	if (!WriteFile(hComPort, buffer, nLen, &dwBytesWritten, NULL))
		return FALSE;

	return TRUE;
}
/////////////////////////////////////////////////End for COM///////////////////


/////////////////////////////////////////Functions for communication/////////////////
//init response data
void InitResponseData()
{
	for (int i = 0; i < MAX_RESPONSE_TIMES; i++)
	{
		g_data[i].responseId = -2;
		g_data[i].nDataLen = 0;
		g_data[i].data = NULL;
	}
}
void ClearResponseData()
{
	g_nResponseTimes = 0;
	for (int i = 0; i < MAX_RESPONSE_TIMES; i++)
	{
		g_data[i].responseId = -2;
		g_data[i].nDataLen = 0;
		if (g_data[i].data != NULL)
			delete g_data[i].data;
		g_data[i].data = NULL;
	}
}

//Parse response data
int ParseResponseData(int nReceived, unsigned char *buf)
{
	int bResult = -1;
	//int                 socketId;
	//int count = 0;
	int KeywordCnt = 0;
	if(nReceived > 0)
	{
		g_data[g_nResponseTimes].data = new unsigned char[nReceived+1]; 
		memset(g_data[g_nResponseTimes].data,0,nReceived + 1);
		memcpy(g_data[g_nResponseTimes].data,buf,nReceived);
		g_data[g_nResponseTimes].nDataLen = nReceived + 1;
		g_data[g_nResponseTimes].responseId = 0;
		g_nResponseTimes++;	
		int len = strlen(NETWRITE_RESPONSE);
		g_data[g_nResponseTimes-1].position = 0;

		KeywordCnt = sizeof(g_extAtResTab)/sizeof(g_extAtResTab[0]);
		for(int i = 0;i < KeywordCnt;i++)//to find the keyword
		{
			if(0 == memcmp(g_data[g_nResponseTimes-1].data,g_extAtResTab[i].ReponseCmd,strlen(g_extAtResTab[i].ReponseCmd))) //find the keyword
			{
				g_data[g_nResponseTimes-1].position = strlen(g_extAtResTab[i].ReponseCmd) ;
				g_extAtResTab[i].resCbfun(g_data);//to  parse data accordig the at detail respectively
				bResult = 0;
				break;
			}
		}
		if(bResult != 0)
		{
			ReadComEvent.SetEvent();
		}
	}

	return bResult;
}

int generateRandomArray(int rangeL,int rangeR)
{        
	int ret;      
	srand(time(NULL));       
	ret=rand()%(rangeR-rangeL+1)+rangeL;       
	return ret;
}


BOOL CRfCardDlg::SendATCommand(Port port, CCommand *pCmd)
{
	BOOL bResult = FALSE;
	char *pTemp = NULL;
	int dataNum = 0;
	int len = 0;
	int Count = 0;
	int nLength = pCmd->strCmdData.GetLength();

	if((pCmd->uDataLen == 0) && (g_netSendData.partCount >= 0) && !memcmp(pCmd->strCmdName,MYCOMMON_CMD,strlen(MYCOMMON_CMD)))
	{	
		int data_len = 0;
		if(g_netSendData.partCount == 0)
		{
			data_len = generateRandomArray(1,2048);
			g_netSendData.partCount = data_len;
		}
		Count = g_netSendData.partCount;
		
		len = nLength + Count;
		pTemp = new char[len];	
		memset(pTemp,0,len);
		memcpy(pTemp,pCmd->strCmdData.GetBuffer(nLength),nLength*sizeof(char));
		if((g_netSendData.pos + Count)>= g_netSendData.len)
		{
			int remainLen = g_netSendData.len - g_netSendData.pos;
			if(remainLen > 0)
			{
				memcpy(pTemp+nLength,g_netSendData.data + g_netSendData.pos,remainLen);
			}
			g_netSendData.pos = 0;
			memcpy(pTemp+nLength+remainLen,g_netSendData.data + g_netSendData.pos,Count - remainLen); 
			g_netSendData.pos = Count - remainLen;
		}else
		{
			memcpy(pTemp+nLength,g_netSendData.data+g_netSendData.pos,Count);
			
			g_netSendData.pos += Count;
		}
	}else{
		len = nLength + strlen(END_CHAR) + pCmd->uDataLen;
		
		pTemp = new char[len];     
		memset(pTemp,0,len);
		memcpy(pTemp,pCmd->strCmdData.GetBuffer(nLength),nLength*sizeof(char));
		if(pCmd->uDataLen > 0)
		{
			memcpy(pTemp+nLength,pCmd->pData,pCmd->uDataLen*sizeof(char));
		}
		nLength += pCmd->uDataLen;
		memcpy(pTemp+nLength,END_CHAR,strlen(END_CHAR)*sizeof(char));
	}
	/*
	unsigned char checkSum = 0x0;

	SendBuffer[0] = pCmd->uCmdType; //command type
	SendBuffer[1] = pCmd->uCmdId;   //command id
	SendBuffer[2] = pCmd->uDataLen; //data length
	for (int i = 0; i < 3; i++)
		checkSum ^= SendBuffer[i];
	//data
	for (i = 0; i < pCmd->uDataLen; i++)
	{
		SendBuffer[3+i] = pCmd->pData[i];
		checkSum ^= pCmd->pData[i];
	}
	//check sum
	int nLength = 3+pCmd->uDataLen;
	SendBuffer[nLength] = checkSum;
	nLength++;
	*/
	/*
	//Reset pipe
	if (port.nType == 0) //usb port
		ResetPipe(port.hPort);
		*/
	//send command
	if (len > 0)
	{
		bResult = TRUE;
		s_uartWriteDataMutex.Lock();
		ofstream outfile;
		
		outfile.open("myfile.txt", ios::app);
		if(outfile) 
		{
			outfile << pTemp << endl <<endl;
			outfile.close();
		}
		dataNum = app_at_enQueue_uartdata(&g_uartWriteDataHead,pTemp,len,NULL);
		if(dataNum > 0)
		{
			CDialog::KillTimer(1);
		}
		s_uartWriteDataMutex.Unlock();
		/*
		if (port.nType == 0) //usb port
			bResult = WriteUsbPort(port.hPort, SendBuffer, nLength);
		else //com port
			bResult = WriteComPort(port.hPort, SendBuffer, nLength);
		*/
	}

	//caculate TIMEOUT ms
	g_nWaitMs = WAIT_TIMEOUT_MS;
	if ((pCmd->uCmdType==0x00) && (pCmd->uCmdId==0x07)) //Common:SpecialCMD
	{
		g_nWaitMs  = 500 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x01) && (pCmd->uCmdId==0x13)) //IIC:bytewrite
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x01) && (pCmd->uCmdId==0x14)) //IIC:pagewrite
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * 64 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x01) && (pCmd->uCmdId==0x15)) //IIC:pagewrite1
	{
		int nStartAddr = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEndAddr   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEndAddr-nStartAddr) * 64 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x02) && (pCmd->uCmdId==0x14)) //SPI:write
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x02) && (pCmd->uCmdId==0x15)) //SPI:pagewrite
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * 64 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x02) && (pCmd->uCmdId==0x16)) //SPI:pagewrite1
	{
		int nStartAddr = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEndAddr   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEndAddr-nStartAddr) * 64 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x03) && (pCmd->uCmdId==0x0B)) //MW:erase
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x03) && (pCmd->uCmdId==0x13)) //IIC:writebyte
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x03) && (pCmd->uCmdId==0x14)) //IIC:writeword
	{
		int nStartAddr = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEndAddr   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEndAddr-nStartAddr) * WAIT_WRITEBYTE_MS;
	}
	if (g_nWaitMs <= 0)
		g_nWaitMs = WAIT_TIMEOUT_MS;

	return bResult;
}


BOOL SendCommand(Port port, CCommand *pCmd)
{
	BOOL bResult = FALSE;
	unsigned char checkSum = 0x0;

	SendBuffer[0] = pCmd->uCmdType; //command type
	SendBuffer[1] = pCmd->uCmdId;   //command id
	SendBuffer[2] = pCmd->uDataLen; //data length
	for (int i = 0; i < 3; i++)
		checkSum ^= SendBuffer[i];
	//data
	for (i = 0; i < pCmd->uDataLen; i++)
	{
		SendBuffer[3+i] = pCmd->pData[i];
		checkSum ^= pCmd->pData[i];
	}
	//check sum
	int nLength = 3+pCmd->uDataLen;
	SendBuffer[nLength] = checkSum;
	nLength++;

	//Reset pipe
	if (port.nType == 0) //usb port
		ResetPipe(port.hPort);
	//send command
	if (nLength > 0)
	{
		if (port.nType == 0) //usb port
			bResult = WriteUsbPort(port.hPort, SendBuffer, nLength);
		else //com port
			bResult = WriteComPort(port.hPort, SendBuffer, nLength);
	}

	//caculate TIMEOUT ms
	g_nWaitMs = WAIT_TIMEOUT_MS;
	if ((pCmd->uCmdType==0x00) && (pCmd->uCmdId==0x07)) //Common:SpecialCMD
	{
		g_nWaitMs  = 500 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x01) && (pCmd->uCmdId==0x13)) //IIC:bytewrite
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x01) && (pCmd->uCmdId==0x14)) //IIC:pagewrite
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * 64 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x01) && (pCmd->uCmdId==0x15)) //IIC:pagewrite1
	{
		int nStartAddr = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEndAddr   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEndAddr-nStartAddr) * 64 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x02) && (pCmd->uCmdId==0x14)) //SPI:write
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x02) && (pCmd->uCmdId==0x15)) //SPI:pagewrite
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * 64 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x02) && (pCmd->uCmdId==0x16)) //SPI:pagewrite1
	{
		int nStartAddr = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEndAddr   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEndAddr-nStartAddr) * 64 * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x03) && (pCmd->uCmdId==0x0B)) //MW:erase
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x03) && (pCmd->uCmdId==0x13)) //IIC:writebyte
	{
		int nStart = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEnd   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEnd-nStart) * WAIT_WRITEBYTE_MS;
	}
	if ((pCmd->uCmdType==0x03) && (pCmd->uCmdId==0x14)) //IIC:writeword
	{
		int nStartAddr = (pCmd->pData[0]<<8) + pCmd->pData[1];
		int nEndAddr   = (pCmd->pData[2]<<8) + pCmd->pData[3];
		g_nWaitMs  = (nEndAddr-nStartAddr) * WAIT_WRITEBYTE_MS;
	}
	if (g_nWaitMs <= 0)
		g_nWaitMs = WAIT_TIMEOUT_MS;

	return bResult;
}

 /*--------------------------------------------------------------------------
 *
 * Function:    getDecimalValue
 *
 * Parameters:  (InOut) commandBuffer_p - a pointer to a CommandLine_t
 *              structure with the position field indicating the start of
 *              where a decimal value may be.
 *
 * Returns:     Int32 containing the value equivalent to the decimal value
 *              found, or zero if no decimal value found.
 *
 * Description: Returns the value equivalent to the decimal value
 *              found at position, or zero if no decimal value found,
 *              and moves position to after the decimal value.
 *
 *-------------------------------------------------------------------------*/

int getDecimalValue (ReceiveData *commandBuffer_p)
{
  int value = 0;

  while ((commandBuffer_p->position < commandBuffer_p->nDataLen - 1) &&
         (isdigit (commandBuffer_p->data[commandBuffer_p->position])))
  {
    value *= 10;
    value += (commandBuffer_p->data[commandBuffer_p->position] - '0');
    commandBuffer_p->position++;
  }

  return (value);
}


/*--------------------------------------------------------------------------
 *
 * Function:    getHexaValue
 *
 * Parameters:  (In) c - a character, that scould be converted to the Hexa value
 *              (Out) value - the Hexa value
 *
 * Returns:     Boolean indicating the success or failure
 *
 * Description:
 *
 *-------------------------------------------------------------------------*/

bool getHexaValue (char c, unsigned char  *value)
{
  bool result = TRUE;

  if ((c >= '0') && (c <= '9'))
  {
    *value = c - '0';
  }
  else
  {
    if ((c >= 'a') && (c <= 'f'))
    {
      *value = c - 'a' + 10;
    }
    else
    {
      if ((c >= 'A') && (c <= 'F'))
      {
        *value = c - 'A' + 10;
      }
      else
      {
        result = FALSE;
      }
    }
  }

  return (result);
}



/*--------------------------------------------------------------------------
 *
 * Function:    getExtendedParameter
 *
 * Parameters:  (InOut) commandBuffer_p - a pointer to a CommandLine_t
 *              structure with the position field indicating the start of
 *              where a decimal value may be.
 *              (Out) value_p - a pointer to an Int32 value where the
 *              equivalent to the decimal value if given, or the value
 *              of the thrid parameter if not, is to be stored.
 *              (In) inValue - an Int32 value to be used if the parameter
 *              is not supplied.
 *
 * Returns:     Boolean indicating the success or failure in obtaining an
 *              extended AT command's parameter value.
 *
 * Description: Attempts to obtain the extended AT command parameter at
 *              position, moving position to the point after the parameter
 *              and returning TRUE if the extended parameter value returned
 *              in *value_p is OK, else FALSE if it is not.
 *
 *-------------------------------------------------------------------------*/

bool getExtendedParameter (ReceiveData *commandBuffer_p,
                               int *value_p,
                                int inValue)
{
  bool result = TRUE;
  bool useInValue = TRUE;
  int   value;
  if (commandBuffer_p->position < commandBuffer_p->nDataLen -1)
  {
    if (commandBuffer_p->data[commandBuffer_p->position] == COMMA_CHAR)
    {
      commandBuffer_p->position++;
    }
    else
    {
      if (commandBuffer_p->data[commandBuffer_p->position] != SEMICOLON_CHAR)
      {
        if (isdigit (commandBuffer_p->data[commandBuffer_p->position]))
        {
          value = getDecimalValue (commandBuffer_p);
          *value_p = value;
          useInValue = FALSE;
          if (commandBuffer_p->position < commandBuffer_p->nDataLen - 1)
          {
            if (commandBuffer_p->data[commandBuffer_p->position] == COMMA_CHAR)
            {
              commandBuffer_p->position++;
            }
          }
        }
        else
        {
          result = FALSE;
        }
      }
    }
  }

  if (useInValue == TRUE)
  {
    *value_p = inValue;
  }

  return (result);
}

/*--------------------------------------------------------------------------
 *
 * Function:    getExtendedString
 *
 * Parameters:  (InOut) commandBuffer_p - a pointer to a CommandLine_t
 *              structure with the position field indicating the start of
 *              where a string value may be.
 *              (Out) outString - pointer to string obtained
 *              (Out) outStringLength - length of string returned.
 *
 * Returns:     Boolean indicating the success or failure in obtaining an
 *              AT command's string parameter.
 *
 * Description: Attempts to obtain the AT command string parameter at
 *              position, moving position to the point after the parameter
 *              and returning TRUE if the string parameter returned
 *              in outString is OK, else FALSE if it is not.
 *              The size of outString must be one larger than maxStringLength
 *              since an additional null terminator will be appended!!!
 *
 *-------------------------------------------------------------------------*/

bool getExtendedString (ReceiveData *commandBuffer_p,
                            unsigned char *outString,
                             unsigned int maxStringLength,
                             unsigned int *outStringLength)
{
  bool result = TRUE;
  unsigned  int nDataLen = 0;
  unsigned char    first, second;

  *outString = (unsigned char)0;
  *outStringLength = 0;
  if ( commandBuffer_p->position < commandBuffer_p->nDataLen - 1 )
  {
    if (commandBuffer_p->data[commandBuffer_p->position] == COMMA_CHAR)
    {
      commandBuffer_p->position++;
    }
    else
    {
      if (commandBuffer_p->data[commandBuffer_p->position] != SEMICOLON_CHAR)
      {
        if (commandBuffer_p->data[commandBuffer_p->position] != QUOTES_CHAR)
        {
			
          result = FALSE;
        }
        else
        {
          commandBuffer_p->position++;
          while ((commandBuffer_p->position < commandBuffer_p->nDataLen - 1) &&
                 (commandBuffer_p->data[commandBuffer_p->position] != QUOTES_CHAR) &&
                 (nDataLen < maxStringLength) &&
                 (result == TRUE))
          {
            if (commandBuffer_p->data[commandBuffer_p->position] == '\\')
            {
              commandBuffer_p->position++;
              /* here must come two hexa digits */
              if (commandBuffer_p->position + 2 < commandBuffer_p->nDataLen - 1)
              {
                if ((getHexaValue (commandBuffer_p->data[commandBuffer_p->position++], &first)) &&
                    (getHexaValue (commandBuffer_p->data[commandBuffer_p->position++], &second)))
                {
                  *outString++ = (char)(16 * first + second);
                  nDataLen++;
                }
                else
                {
                 result = FALSE;
                }
              }
              else
              {
                result = FALSE;
              }
            }
            else
            {
              *outString++ = (char)(commandBuffer_p->data[commandBuffer_p->position++]);
              nDataLen++;
            }
          }

          *outStringLength = nDataLen;
          *outString = (unsigned char)0;

          if (((commandBuffer_p->data[commandBuffer_p->position] == QUOTES_CHAR) &&
               (nDataLen <= maxStringLength)) == FALSE)
          {
            result = FALSE;
          }
          else
          {
            commandBuffer_p->position++;
            if (commandBuffer_p->data[commandBuffer_p->position] == COMMA_CHAR)
            {
              commandBuffer_p->position++;
            }
          }
        }
      }
    }
  }
  return (result);
}


UINT pollingThreadWritePort(LPVOID pParam)
{
	CRfCardDlg* pWnd = (CRfCardDlg*)pParam;
	BOOL bResult = FALSE;
	int writeLen = 0;
	char *pWriteData = NULL;

	while(!g_bExit) 
	{
		s_uartWriteDataMutex.Lock();
		if(app_at_get_uartdata_queue_num(&g_uartWriteDataHead,NULL) > 0)
		{
			time_2 = 0;
			time_2 = GetTickCount();
			writeLen = app_at_deQueue_uartdata(&g_uartWriteDataHead,&pWriteData,NULL);
		}
		s_uartWriteDataMutex.Unlock();
		//Reset pipe
		if(writeLen > 0 && NULL != pWriteData){
			if (g_port.nType == 0) //usb port
				ResetPipe(g_port.hPort);
			
			if (g_port.nType == 0) //usb port
				bResult = WriteUsbPort(g_port.hPort, (unsigned char*)pWriteData, writeLen);
			else //com port
				bResult = WriteComPort(g_port.hPort,(unsigned char*) pWriteData, writeLen);
			time_1 = 0;
			time_1 = GetTickCount();
			delete[] pWriteData;
			pWriteData = NULL;	
			
			WaitForSingleObject(ReadComEvent, INFINITE);
			pWnd->PostMessage(WM_CONTINUE_TIMER,0,0);
			ReadComEvent.ResetEvent();
		}
	}
	return 0;
}


UINT pollingThreadReadPort(LPVOID pParam)
{
	CRfCardDlg* pWnd = (CRfCardDlg*)pParam;

	while (!g_bExit)
	{
		//WaitForSingleObject(poolEvent_mcu0, INFINITE);
		g_nPoolingTime ++;

			if (g_port.hPort != NULL)
			{
				BOOL bResult = FALSE;
				unsigned char RespBuff[1024] = {0};
				unsigned long nReceived0 = 0;
				CString str;
				long t1=GetTickCount();//程序段开始前取得系统运行时间(ms)
				if (g_port.nType == 0) //usb port
					bResult = ReadUsbPort(g_port.hPort, RespBuff, sizeof(RespBuff), &nReceived0);
				else
				{
					bResult = ReadComPort(g_port.hPort, RespBuff, 1, &nReceived0);
					if(nReceived0 > 0)
					{
						ReadComPort(g_port.hPort, RespBuff+1, sizeof(RespBuff)-1, &nReceived0);
					}
				}
				long t2=GetTickCount();//程序段结束后取得系统运行时间(ms)
				str.Format("time:%dms",t2-t1);//前后之差即 程序运行时间
				if (nReceived0 > 0)
				{
					InitResponseData();
					int nResult = ParseResponseData(nReceived0 + 1, RespBuff);
					pWnd->PostMessage(WM_DISPLAY_CHANGE,0,0);
				}
			}
	}


	return 0;
}
/*
UINT pollingThread(LPVOID pParam)
{
	CRfCardDlg* pWnd = (CRfCardDlg*)pParam;

	while (!g_bExit)
	{
		//Sleep(5);
		if (g_bPoll_mcu0)
		{
			//WaitForSingleObject(poolEvent_mcu0, INFINITE);
			//Sleep(10);
			if (g_port.hPort != NULL)
			{
				BOOL bResult = FALSE;
				unsigned char RespBuff[1024];
				unsigned long nReceived0 = 0;
				if (g_port.nType == 0) //usb port
					bResult = ReadUsbPort(g_port.hPort, RespBuff, sizeof(RespBuff), &nReceived0);
				else
					bResult = ReadComPort(g_port.hPort, RespBuff, sizeof(RespBuff), &nReceived0);
				//if (bResult == FALSE)
				//	break;
				//append to RespBuff
				//for (int i = 0; i < (int)nTempReceived; i++)
				//	RespBuff[nReceived0++] = tempBuf[i];
				if (nReceived0 > 0)
				{
					int nResult = ParseResponseData(nReceived0, RespBuff);
					if ((nResult != 0) || (g_nResponseTimes >= MAX_RESPONSE_TIMES))
					{
						g_bPoll_mcu0 = FALSE;
						//poolEven_mcu0.ResetEvent();
						endEvent_mcu0.SetEvent();
					}
				}
			}
		}
	}

	g_pollThread = NULL;
	g_bPoll_mcu0 = FALSE;
	//if (endEvent_mcu0.m_hObject != NULL)
	//	endEvent_mcu0.ResetEvent();
	threadEndEvent.SetEvent();

	return 0;
}
*/
////////////////////////////////////////End for communications


////////////////////////////////////////////For string operation
CString RemoveWrappingQuotes(CString str)
{
  int len;
  str.TrimLeft();
  str.TrimRight();
  len = str.GetLength();
  if ((len > 2) && (str.GetAt(0) == '"') &&
		   (str.GetAt(len-1) == '"')) {
    str = str.Left(len-1);
    str = str.Right(len-2);
  }
  return str;
}

CString GetPathOnly(CString path)
{
	char pathonly[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	if ((path.GetLength() > 0) && (path[0] == _TCHAR('\"'))) {
	  CString cleanedPath = RemoveWrappingQuotes(path);
	  _splitpath(cleanedPath, drive, dir, NULL, NULL);
	} else {
	_splitpath(path, drive, dir, NULL, NULL);
	}
	_makepath(pathonly, drive, dir, NULL, NULL);
	return (CString)pathonly;
}

CString GetExePath()
{
	CString envPath;
	GetModuleFileName(GetModuleHandle(NULL), envPath.GetBuffer(MAX_PATH), MAX_PATH-1);
	envPath.ReleaseBuffer();
	return GetPathOnly(envPath);
}
///////////////////////////////////////End for string functions


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRfCardDlg dialog


CRfCardDlg::CRfCardDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRfCardDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRfCardDlg)
	m_lstAllCmdValue = _T("");
	m_write_hex_begin = _T("");
	m_Key_Value = _T("");
	m_Inc_Value = _T("");
	m_Trans_Value = _T("");
	m_Sector_Value = _T("0");
	m_Blk_Value = _T("0");
	m_Write2_Value = _T("");
	m_WinSel = 1;
	m_CrcErr = FALSE;
	m_ParaErr = FALSE;
	m_Comport = 0;
	m_BaudRate = 1;
	m_Mcm200_On = FALSE;
	m_ShowResp = FALSE;
	m_RunTimes = 0;
	m_version = 0;
	m_IDdata = _T("");
	m_cgmode = 192;
	m_modeedit = _T("");
	m_block00 = FALSE;
	m_block01 = FALSE;
	m_block02 = FALSE;
	m_block10 = FALSE;
	m_block11 = FALSE;
	m_block12 = FALSE;
	m_block20 = FALSE;
	m_block21 = FALSE;
	m_block22 = FALSE;
	m_block30 = FALSE;
	m_block31 = FALSE;
	m_block32 = FALSE;
	m_keya = _T("ffffffffffff");
	m_keyb = _T("ffffffffffff");
	m_ErrorSum = _T("0");
	m_StopAuthen = FALSE;
	m_StopAuthen2 = FALSE;
	m_EquSel = 0;
	m_strCombo = 4;
	m_strPara = _T("");
	m_bRunning = FALSE;
	m_nCurrentLine =0;
	m_nCurBrkpt = 0;
	m_nPreBrkpt = 0;
	m_bBreakOn = FALSE;
    m_bTestMode = FALSE;
	m_bKeyB_On = FALSE;
	m_delayTime = 500;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_pCommonCmds = new COMMANDLIST;
	m_pIICCmds = new COMMANDLIST;
	m_pSPICmds = new COMMANDLIST;
	m_pMWCmds = new COMMANDLIST;
	m_pATCmds = new COMMANDLIST;
}

void CRfCardDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRfCardDlg)
	DDX_Control(pDX, IDC_LIST_DEVICE, m_listDevice);
	DDX_Control(pDX, IDC_RCL, m_Spin_Times);
	DDX_Control(pDX, IDC_RUNPRGS, m_CmdFinishedProgress);
	DDX_Control(pDX, IDC_LIST_FROM_MCU, m_lstFromMcu);
	DDX_Control(pDX, IDC_LIST_CMD, m_lstCmd);
	DDX_Control(pDX, IDC_LIST_ALL_CMD, m_lstAllCmd);
	DDX_LBString(pDX, IDC_LIST_ALL_CMD, m_lstAllCmdValue);
	DDX_Radio(pDX, IDC_CMD, m_WinSel);
	DDX_Check(pDX, IDC_CRC, m_CrcErr);
	DDX_Check(pDX, IDC_PARABITY, m_ParaErr);
	DDX_CBIndex(pDX, IDC_COMM, m_Comport);
	DDX_CBIndex(pDX, IDC_BDRT, m_BaudRate);
	DDX_Check(pDX, IDC_STOP, m_stop);
	DDX_Check(pDX, IDC_ShOWRESP, m_ShowResp);
	DDX_Text(pDX, IDC_RECYCLE, m_RunTimes);
	DDX_Text(pDX, IDC_version, m_version);
	DDX_Text(pDX, IDC_ID, m_IDdata);
	DDX_Text(pDX, IDC_mode, m_cgmode);
	DDX_Text(pDX, IDC_MODEedit, m_modeedit);
	DDX_Text(pDX, IDC_ErrorSum, m_ErrorSum);
	DDX_Check(pDX, IDC_StopAthen, m_StopAuthen);
	DDX_Check(pDX, IDC_CHECK2, m_StopAuthen2);
	DDX_CBIndex(pDX, IDC_COMBO1, m_strCombo);
	DDX_Text(pDX, IDC_EDIT_PARA, m_strPara);
	DDX_Text(pDX, IDC_DELAYTIME, m_delayTime);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CRfCardDlg, CDialog)
	//{{AFX_MSG_MAP(CRfCardDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_OPEN, OnOpen)
	ON_BN_CLICKED(IDC_SAVE, OnSave)
	ON_BN_CLICKED(IDC_BTN_DELETE_ALL, OnBtnDeleteAll)
	ON_BN_CLICKED(IDC_BTN_DELETE, OnBtnDelete)
	ON_BN_CLICKED(IDC_RUN, OnRun)
	ON_BN_CLICKED(IDC_STEP, OnStep)
	ON_BN_CLICKED(IDC_LINK, OnLink)
	ON_LBN_DBLCLK(IDC_LIST_CMD, OnDblclkListCmd)
	ON_LBN_DBLCLK(IDC_LIST_ALL_CMD, OnDblclkListAllCmd)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_STOP, OnStop)
	ON_BN_CLICKED(IDC_BUTTON3, OnButton3)
	ON_BN_CLICKED(IDC_BUTTON4, OnButton4)
	ON_BN_CLICKED(IDC_BUTTON5, OnStepandnext)
	ON_CBN_SELCHANGE(IDC_COMBO1, OnSelchangeCombo1)
	ON_LBN_SELCHANGE(IDC_LIST_ALL_CMD, OnSelchangeListAllCmd)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BTN_ADDCOMMENT, OnBtnAddcomment)
	ON_LBN_SELCHANGE(IDC_LIST_DEVICE, OnSelchangeListDevice)
	
	ON_MESSAGE(WM_DISPLAY_CHANGE, OnDisplayChange)
	
	ON_MESSAGE(WM_CONTINUE_TIMER, OnContinueTimer)
	//}}AFX_MSG_MAP
	//ON_BN_CLICKED(IDC_RUN, OnRunStep)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRfCardDlg message handlers

BOOL CRfCardDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.
 	
	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
//#ifdef SUPPORT_2315_STS
//	Init_2315STS_Cmd();
//#endif

    m_Spin_Times.SetRange32(1,10000);
	m_Spin_Times.SetPos(1);//initial value
    //m_Spin_Sector.SetRange(0, 0x80);
    //m_Spin_Blk.SetRange(0, 3);
    //m_Spin_Sector.SetPos(0);
    //m_Spin_Blk.SetPos(0);

    //set progress bar color
//	m_CmdFinishedProgress.SetBkColor(RGB(255,0,0));

	//link comm port
	//OnLink();
	CheckRadioButton(IDC_RADIO_USB, IDC_RADIO_UART, IDC_RADIO_UART);
	m_bConnect = FALSE;
	InitPorts();
	InitResponseData();

	//init command list
	CString strFile = GetExePath() + "\\CmdList.cfg";
	ReadInfoFile(strFile);
	OnSelchangeCombo1();
	//init buttons
	EnableButtons();

	m_nDelayTimes = m_delayTime;
	//init device
	for (int i = 0; i < SERIES_NUM; i++)
	{
		for (int j = 0; j < MAXTYPE; j++)
		{
			if (chDeviceList[i][j])
				m_listDevice.AddString(chDeviceList[i][j]);
		}
	}

	
	strFile = GetExePath() + "\\writedata.txt";
	CStdioFile file;
	if (!file.Open(strFile, CFile::modeRead | CFile::typeText))
		return FALSE;
	else
	{
		CString strLine;
		int len = 0;
		if(file.ReadString(strLine))
		{
			len = strLine.GetLength();
			g_netSendData.data = new char[len+1];
			memset(g_netSendData.data,0,len+1);
			memcpy(g_netSendData.data,strLine.GetBuffer(len),len);
			g_netSendData.pos = 0;
			g_netSendData.len = len;
			g_netSendData.partCount = 0;
		}
	}
	m_listDevice.SetCurSel(0);
	//update ORG radio buttons
	BOOL bEnable = FALSE;
	GetDlgItem(IDC_STATIC_ORG)->ShowWindow(bEnable);
	GetDlgItem(IDC_RADIO_0)->ShowWindow(bEnable);
	GetDlgItem(IDC_RADIO_1)->ShowWindow(bEnable);
	UpdateData(FALSE);

	return TRUE;  // return TRUE  unless you set the focus to a control
	
}

void CRfCardDlg::EnableButtons()
{
	//buttons
	GetDlgItem(IDC_RUN)->EnableWindow(m_bConnect);
	GetDlgItem(IDC_STEP)->EnableWindow(m_bConnect);
	GetDlgItem(IDC_BUTTON5)->EnableWindow(m_bConnect);
	GetDlgItem(IDC_STOP)->EnableWindow(m_bConnect);
}

void ParseCommand(CCommand *pCmd, CString strText)
{
	strText.TrimLeft();
	int nIndex = strText.Find(':');

	CString strVal = strText.Mid(nIndex+1);
	strVal.TrimLeft();
	strVal.TrimRight();
	strVal.Remove(' ');
	//Cmd type
	char *stopstr;
	pCmd->uCmdType = (unsigned char)strtoul(strVal.Mid(0, 2), &stopstr, 16);
	//Cmd id
	pCmd->uCmdId = (unsigned char)strtoul(strVal.Mid(2, 2), &stopstr, 16);
	//data len
	if (strVal.GetLength() >= 6)
		pCmd->uDataLen = (unsigned char)strtoul(strVal.Mid(4, 2), &stopstr, 16);
	else
		pCmd->uDataLen = 0xff;
	//Cmd name
	pCmd->strCmdName = strText.Mid(0, nIndex);

}


void At_ParseCommand(CCommand *pCmd, CString strText)
{
	strText.TrimLeft();
	int nIndex = strText.Find(':');

	CString strVal = strText.Mid(nIndex+1);
	strVal.TrimLeft();
	strVal.TrimRight();
	strVal.Remove(' ');
	//Cmd type
	/*
	char *stopstr;
	pCmd->uCmdType = (unsigned char)strtoul(strVal.Mid(0, 2), &stopstr, 16);
	//Cmd id
	pCmd->uCmdId = (unsigned char)strtoul(strVal.Mid(2, 2), &stopstr, 16);
	//data len
	if (strVal.GetLength() >= 6)
		pCmd->uDataLen = (unsigned char)strtoul(strVal.Mid(4, 2), &stopstr, 16);
	else
		pCmd->uDataLen = 0xff;
		*/
    //Cmd data
    
	pCmd->strCmdData = strVal;
	//Cmd name
	pCmd->strCmdName = strText.Mid(0, nIndex);

}


BOOL CRfCardDlg::ReadInfoFile(CString strFile)
{
	CStdioFile file;
	if (!file.Open(strFile, CFile::modeRead | CFile::typeText))
		return FALSE;
	else
	{
		CString strLine;
		CString strKey;
		while (file.ReadString(strLine))
		{
			//common command
			strKey = KEY_COMMONCMD;
			if (strLine.Find(strKey) >= 0)
			{
				CCommand* pCmd = new CCommand;
				ParseCommand(pCmd, strLine);
				m_pCommonCmds->AddTail(pCmd);
				continue;
			}
			//IIC command
			strKey = KEY_IICCMD;
			if (strLine.Find(strKey) >= 0)
			{
				CCommand* pCmd = new CCommand;
				ParseCommand(pCmd, strLine);
				m_pIICCmds->AddTail(pCmd);
				continue;
			}
			//SPI command
			strKey = KEY_SPICMD;
			if (strLine.Find(strKey) >= 0)
			{
				CCommand* pCmd = new CCommand;
				ParseCommand(pCmd, strLine);
				m_pSPICmds->AddTail(pCmd);
				continue;
			}
			//MW command
			strKey = KEY_MWCMD;
			if (strLine.Find(strKey) >= 0)
			{
				CCommand* pCmd = new CCommand;
				ParseCommand(pCmd, strLine);
				m_pMWCmds->AddTail(pCmd);
				continue;
			}
			// AT command
			strKey = KEY_ATCMD;
			if (strLine.Find(strKey) >= 0)
			{
				CCommand* pCmd = new CCommand;
				At_ParseCommand(pCmd, strLine);
				m_pATCmds->AddTail(pCmd);
				continue;
			}
		}
	}

	file.Close();
	return TRUE;
}


void CRfCardDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CRfCardDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CRfCardDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

LRESULT CRfCardDlg::OnDisplayChange(WPARAM wParam,LPARAM lParam)
{
    //更新控件
    UpdateGUI();
	ClearResponseData();
    UpdateData(FALSE);
    return 0;
}

LRESULT CRfCardDlg::OnContinueTimer(WPARAM wParam,LPARAM lParam)
{
	if(m_bRunning)
	{
		SetTimer(1,m_nDelayTimes,NULL);
	}
    return 0;
}


void CRfCardDlg::AdjustHorizontalScroll(CListBox *pListBox)
{
	// Find the longest string in the list box.
	CSize    sz;
	int      dx = 0;
	TEXTMETRIC   tm;
	CDC*      pDC = pListBox->GetDC();
	CFont*      pFont = pListBox->GetFont();
	// Select the listbox font, save the old font
	CFont* pOldFont = pDC->SelectObject(pFont);
	// Get the text metrics for avg char width
	pDC->GetTextMetrics(&tm);	
	for (int i = 0; i < pListBox->GetCount(); i++)
	{
		CString str;
		pListBox->GetText(i, str);
		sz = pDC->GetTextExtent(str);		
		// Add the avg width to prevent clipping
		sz.cx += tm.tmAveCharWidth;		
		if (sz.cx > dx)	dx = sz.cx;
	}
	// Select the old font back into the DC
	pDC->SelectObject(pOldFont);
	pListBox->ReleaseDC(pDC);	
	// Set the horizontal extent so every character of all strings can be scrolled to.
	pListBox->SetHorizontalExtent(dx);
}

void CRfCardDlg::OnDblclkListAllCmd() 
{
	// TODO: Add your control notification handler code here
	OnAdd();
}

//get EE info
unsigned char GetEEInfo(char *strDevice, int nORG)
{
	unsigned char buf = 0xff;

	BOOL bFind = FALSE;
	int i = 0, j= 0;
	for (; i < SERIES_NUM; i++)
	{
		if (bFind == TRUE)
			break;
		for (j = 0; j < MAXTYPE; j++)
		{
			if (chDeviceList[i][j])
			{
				if (strcmp(strDevice, chDeviceList[i][j]) == 0)
				{
					bFind = TRUE;
					break;
				}
			}
		}
	}
	i--;
	buf = (i&0xff) | ((j<<3)&0xff) | ((nORG<<7)&0xff);

	return buf;
}

void CRfCardDlg::OnAdd() 
{
	UpdateData(true);

	LPCMDLIST pCmds;
	int nCmdType = ((CComboBox*)GetDlgItem(IDC_COMBO1))->GetCurSel();
	switch (nCmdType)
	{
	case COMMON_COMMAND:
		pCmds = m_pCommonCmds;
		break;
	case IIC_COMMAND:
		pCmds = m_pIICCmds;
		break;
	case SPI_COMMAND:
		pCmds = m_pSPICmds;
		break;
	case MW_COMMAND:
		pCmds = m_pMWCmds;
		break;
	case AT_COMMAND:
		pCmds = m_pATCmds;
		break;
	default:
		pCmds = NULL;
		break;
	}
	if (pCmds == NULL)
		return;

	int nCurIndex = m_lstAllCmd.GetCurSel();
	if (nCurIndex != LB_ERR)
	{
		//cmd name
		POSITION pos = pCmds->FindIndex(nCurIndex);
		CCommand *pCmd = pCmds->GetAt(pos);
		if (pCmd == NULL)
			return;
     	CString strTemp = m_lstAllCmdValue+", ";
		//cmd para
		if (m_lstAllCmdValue.CompareNoCase("Common_SetEEType") == 0)
		{
			char strDevice[256];
			m_listDevice.GetText(m_listDevice.GetCurSel(), strDevice);
			//ORG
			int nORG = 0;
			if (strstr(strDevice, "IS93") != NULL)
			{
				int nCheck = GetCheckedRadioButton(IDC_RADIO_0, IDC_RADIO_1);
				nORG = (nCheck == IDC_RADIO_0)?0:1;
			}
			char strPara[256];
			sprintf(strPara, "%02x", GetEEInfo(strDevice, nORG));
			strTemp += strPara;
		}
		else if(nCmdType == AT_COMMAND){
			// if is AT command no need to check the input para , just attach the tail directly
			m_strPara.TrimLeft();
			m_strPara.TrimRight();
			m_strPara.Remove(' ');
			int nLen = m_strPara.GetLength();
			strTemp += m_strPara;
		}else
		{
			m_strPara.TrimLeft();
			m_strPara.TrimRight();
			m_strPara.Remove(' ');
			int nLen = m_strPara.GetLength();
			if ((nLen%2) != 0)
			{
				m_lstFromMcu.AddString("Parameter must be bytes value with hex format!");
				return;
			}
			nLen = nLen / 2;
			if (nLen > 0xff)
			{
				m_lstFromMcu.AddString("Parameter can not be over 255 bytes!");
				return;
			}
			if (pCmd->uDataLen == 0xff)
				pCmd->uDataLen = nLen;
			if (nLen != pCmd->uDataLen)
			{
				char strMsg[MAX_PATH];
				sprintf(strMsg, "Please input %d bytes parameter, hex format", pCmd->uDataLen);
				m_lstFromMcu.AddString(strMsg);
				return;
			}
			strTemp += m_strPara;
		}
		
		//add to command window
		if (m_lstCmd.GetCurSel() == LB_ERR)//no selection
            m_lstCmd.InsertString(m_lstCmd.GetCount(),strTemp);//just add string at end of string list
		else
            m_lstCmd.InsertString(m_lstCmd.GetCurSel()+1,strTemp);
		TRACE("N=%d\n",m_lstCmd.GetCount());
	}
	
	AdjustHorizontalScroll(&m_lstCmd);
}

void CRfCardDlg::OnOpen() 
{
	// TODO: Add your control notification handler code here
	static char BASED_CODE szFilter[] = "Text Files (*.txt) |*.txt|";
	CFileDialog cfgOpen(TRUE, NULL, NULL,  OFN_HIDEREADONLY | OFN_FILEMUSTEXIST , (LPCTSTR)szFilter, NULL);

	if (cfgOpen.DoModal() != IDOK) return;
//	if (cfgOpen.DoModal())
	else
	{
		CString fileName = cfgOpen.GetFileName();
		if (fileName != "")
		{
			m_lstCmd.ResetContent();
			
			HANDLE hFile = CreateFile(fileName.GetBuffer(fileName.GetLength()),
				GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,NULL);
			bool bResult = false;
			CString strContent;
			if (hFile != NULL)
			{
				OVERLAPPED ov;
				memset(&ov,0,sizeof(ov));
				for ( ; ; )
				{
					DWORD dwRead;
					char c;
					if (ReadFile(hFile,&c,1,&dwRead,&ov) != 0)
					{
						//有数据
						if (dwRead > 0)	
						{
							if ((c != '\n') && (c != '\r'))
							{
								strContent += c;
							}
							else
							{
								if (strContent.GetLength() != 0)
								{
									m_lstCmd.AddString(strContent);
									strContent.Empty();
								}
							}
						}
					}
					else		//read error
					{
						if (GetLastError() == ERROR_HANDLE_EOF)	
						{
							if (strContent.GetLength() != 0)
							{
								m_lstCmd.AddString(strContent);
								strContent.Empty();
							}
							bResult = true;			
						}
						break;
					}
					ov.Offset += 1;
				}// end for read file
				if (!bResult)
				{
					MessageBox("Read File Error","Error",MB_OK);
				}
				CloseHandle(hFile);
			}
			fileName.ReleaseBuffer();
		}
	}

	AdjustHorizontalScroll(&m_lstCmd);
}

static bool MyWriteFile(HANDLE hFile, OVERLAPPED& r_ov, CString& r_strContent)
{
	DWORD dwCurWrite = 0;
	DWORD dwWrited;
	LPTSTR lpStr = r_strContent.GetBuffer(r_strContent.GetLength());
	for ( ; dwCurWrite < (DWORD)r_strContent.GetLength(); )
	{
		if (WriteFile(hFile, lpStr + dwCurWrite, r_strContent.GetLength() - dwCurWrite,
			&dwWrited, &r_ov) != 0)
		{
			dwCurWrite += dwWrited;
			r_ov.Offset += dwWrited;
		}
		else
		{
			return false;
		}
	}
	return true;
	
}

void CRfCardDlg::OnSave() 
{
	// TODO: Add your control notification handler code here
	// TODO: Add your control notification handler code here
	//CFileDialog cfgOpen(false);
	//if (cfgOpen.DoModal())
	UpdateData(true);
   	static char BASED_CODE szFilter[] = "Text Files (*.txt) |*.txt|Log Files (*.log) |*.log||";
	
	CFileDialog cfgOpen(false, NULL, NULL,  OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_FILEMUSTEXIST , (LPCTSTR)szFilter, NULL);

	if (cfgOpen.DoModal() != IDOK) 
		return;
	else
	{
		CString fileName = cfgOpen.GetFileName();
		if (fileName != "")
		{
			HANDLE hFile = CreateFile(fileName.GetBuffer(fileName.GetLength()),
				GENERIC_WRITE,FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,NULL);
			bool bResult = TRUE;
			if (hFile != NULL)
			{
				OVERLAPPED ov;
				memset(&ov,0,sizeof(ov));
				if(m_WinSel==0){//save command file
					for (long i=0 ; i < m_lstCmd.GetCount(); i++)
					{
						CString strContent;
						m_lstCmd.GetText(i,strContent);
						strContent += '\r';
						strContent += '\n';
						if ( !MyWriteFile(hFile,ov,strContent))
						{
							bResult = false;
							break;		
						}
					}
				}
				else{
					 for (long i=0 ; i < m_lstFromMcu.GetCount(); i++)
						{
							CString strContent;
							m_lstFromMcu.GetText(i,strContent);
							strContent += '\r';
							strContent += '\n';
							if ( !MyWriteFile(hFile,ov,strContent))
							{
								bResult = false;
								break;		
							}
						}
				}
				CloseHandle(hFile);
			}
			if (!bResult)
			{
				MessageBox("Write File Error","Error",MB_OK);
			}
			fileName.ReleaseBuffer();
			
		}
	}
}

void CRfCardDlg::OnBtnDeleteAll() 
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
	if(m_WinSel==0)
	{
    	m_lstCmd.ResetContent();
		AdjustHorizontalScroll(&m_lstCmd);
	}
	else
		m_lstFromMcu.ResetContent();
}

void CRfCardDlg::OnBtnDelete() 
{
	// TODO: Add your control notification handler code here
	if (m_lstCmd.GetCurSel() != LB_ERR)	
	{
		m_lstCmd.DeleteString(m_lstCmd.GetCurSel());
		AdjustHorizontalScroll(&m_lstCmd);
	}	
}

void CRfCardDlg::OnRun() 
{
    UpdateData(true);
    static long mPreTimes=1;
    if(m_lstCmd.GetCount()==0) //FIRST check whether the comands list is empty
		return;
    m_bRunning = ~m_bRunning;
	if(m_bRunning)
	{
		m_nTotallLine = m_lstCmd.GetCount();
		m_nTotallTimes = m_RunTimes;
		m_nDelayTimes = m_delayTime;
		if(m_nTotallTimes!= mPreTimes){
			m_bUpdateTimes = TRUE;
			mPreTimes = m_nTotallTimes;
		}
		else 
			m_bUpdateTimes = false;
		TRACE("times=%d",m_RunTimes);
		m_nTimerID=SetTimer(1,m_nDelayTimes,NULL);
		SetDlgItemText(IDC_RUN, "Pause");
	}
	else{
		KillTimer(1);
		SetDlgItemText(IDC_RUN, "Run");
	}
}

void CRfCardDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	static  long CurrentTimes=0;
	if(m_bUpdateTimes){
		CurrentTimes =0;
		m_bUpdateTimes=false;
	}
	if(nIDEvent==m_nTimerID ){
	
		if(m_nCurrentLine!=0)
             m_lstCmd.SetItemColor(m_nCurrentLine-1,RGB(0,0,0));//white
        m_lstCmd.SetItemColor(m_nCurrentLine,RGB(0,0,255));//blue
		m_lstCmd.SetCurSel(m_nCurrentLine);//visible
		
		OnStep();
		m_lstFromMcu.SetCurSel(m_lstFromMcu.GetCount()-1);//visible

		//display process
		if((m_lstFromMcu.GetCount() >60000)&&(CurrentTimes<(abs(m_nTotallTimes-100))))
				m_lstFromMcu.ResetContent();

/*		CString str11;
		str11.Format("%d%",fig_times);
		m_lstFromMcu.AddString(str11);
		fig_times++;*/
		CString str;
		str.Format("%d",m_nCurrentLine);
		SetDlgItemText(IDC_CmdRatio,str);

		CString str_2;
		str_2.Format("%d",CurrentTimes);
		SetDlgItemText(IDC_CycleRatio,str_2);
		m_CmdFinishedProgress.SetPos(m_nCurrentLine*100/m_nTotallLine);
//		m_CmdFinishedProgress.SetPos(CurrentTimes/m_nTotallTimes);
		m_nCurrentLine++;
		
		CString str_3;
		str_3.Format("%d",Error_sum);
		SetDlgItemText(IDC_ErrorSum,str_3);	

		str_3.Format("%d",Assert_sum);
		SetDlgItemText(IDC_ErrorSum2,str_3);

        if(m_bBreakOn && (m_nCurrentLine == m_nCurBrkpt+1)){//breakpoint mode
			CDialog::KillTimer(nIDEvent);
			SetDlgItemText(IDC_RUN, "Run");
			m_bRunning = FALSE;
            m_nCurrentLine=0;
			m_lstCmd.SetItemColor(m_lstCmd.GetCurSel(),RGB(255,0,0));//red mark 
		}
		else//normal mode
		{
			if(m_nCurrentLine==m_nTotallLine){//one recycle is finished
				CurrentTimes++;
				m_nCurrentLine=0;
				m_CmdFinishedProgress.SetPos(0);
				SetDlgItemText(IDC_CmdRatio,"");//clear didplay
				SetDlgItemText(IDC_CycleRatio,"");//clear didplay
				m_lstCmd.SetItemColor(m_nTotallLine-1,RGB(0,0,0));//white
			}
			
	        if( CurrentTimes ==m_nTotallTimes){//all recyceles have been executed
				CDialog::KillTimer(nIDEvent);
				SetDlgItemText(IDC_RUN, "Run");
				m_bRunning = FALSE;
				SetDlgItemText(IDC_CmdRatio,"");//clear didplay
				SetDlgItemText(IDC_CycleRatio,"");//clear didplay
				CurrentTimes =0;
				Error_sum=0;
				Assert_sum=0;
			}
	    	else
			    CDialog::OnTimer(nIDEvent);
		}
	}
}

void CRfCardDlg::OnStop() 
{
	// TODO: Add your control notification handler code here

    CDialog::KillTimer(1);
	m_lstCmd.SetItemColor(m_nCurBrkpt,RGB(0,0,0));
	m_lstCmd.SetItemColor(m_nCurrentLine-1,RGB(0,0,0));
	m_bRunning = FALSE;
    m_nCurrentLine =0;
	m_nCurBrkpt = 0;
	m_nPreBrkpt = 0;
	m_bBreakOn = false;
	m_CmdFinishedProgress.SetPos(0);
	SetDlgItemText(IDC_CmdRatio,"");//clear didplay
	SetDlgItemText(IDC_CycleRatio,"");//clear didplay
	SetDlgItemText(IDC_RUN, "Run");
	m_lstCmd.SetCurSel(0);
	Error_sum=0;
	Assert_sum =0;
} 

void CRfCardDlg::OnDblclkListCmd() 
{
	// TODO: Add your control notification handler code here

	// TODO: Add your control notification handler code here
	if(m_lstCmd.GetCurSel() != LB_ERR){
		if(!m_bBreakOn){
			m_lstCmd.SetItemColor(m_nPreBrkpt,RGB(0,0,0));//clear previous line color
    		m_lstCmd.SetItemColor(m_lstCmd.GetCurSel(),RGB(255,0,0));
			m_nPreBrkpt = m_nCurBrkpt;
			m_nCurBrkpt = m_lstCmd.GetCurSel();
			m_bBreakOn = TRUE;
		}
		else{  
			m_lstCmd.SetItemColor(m_lstCmd.GetCurSel(),RGB(0,0,0));
			m_nCurBrkpt = 0;
			m_nPreBrkpt = 0;
			m_bBreakOn = false;
		}
	}	
}

void CRfCardDlg::OnStep() 
{
	BeginWaitCursor();

	UpdateData(true);
	if(m_lstCmd.GetCurSel() == LB_ERR)	
		return;		//must have selection

	//get command
	CString strCmdLine;
	m_lstCmd.GetText(m_lstCmd.GetCurSel(),strCmdLine);
	strCmdLine.TrimLeft();
	strCmdLine.TrimRight();

	//check command
	BOOL bResult = CheckCommand(strCmdLine);
	if (FALSE == bResult)
	{
		m_lstFromMcu.AddString(strCmdLine);
		return;
	}

	//command translation
	CCommand *pCommand = TranslateCmd(strCmdLine);
	if (pCommand == NULL)
	{
		m_lstFromMcu.AddString("...Error... \t Command is invalid!");
		return;
	}

	//Handle Command
	bResult = HandleCommand(pCommand);
	if (FALSE == bResult)
		return;

	CString str_3;
	str_3.Format("%d",Error_sum);
	SetDlgItemText(IDC_ErrorSum,str_3);	
	str_3.Format("%d",Assert_sum);
	SetDlgItemText(IDC_ErrorSum2,str_3);

	EndWaitCursor();
}

//port link
HANDLE  CommPort = NULL;
void CRfCardDlg::OnLink() 
{
	// TODO: Add your control notification handler code here
	UpdateData(true);

	char RespBuffer[256];
	BOOL bResult = FALSE;
	BeginWaitCursor();

	//Connect or disconnect ports
	if (!m_bConnect)
	{
		bResult = LinkPorts();
		if(bResult)
		{
			if (g_pollThread == NULL)
			{
				ClearResponseData();
				g_pollThread = AfxBeginThread(pollingThreadReadPort, this, THREAD_PRIORITY_NORMAL, 0, 0, NULL); 
			}
			if(g_pollWriteThread == NULL)
			{
				g_pollThread = AfxBeginThread(pollingThreadWritePort, this, THREAD_PRIORITY_NORMAL, 0, 0, NULL); 
			}
		}
	}
	else
	{
		bResult = ClosePorts();
		if(bResult)
		{
			g_bExit = TRUE;
			CDialog::KillTimer(1);
			m_bRunning = FALSE;
		}
	}
	EndWaitCursor();
	if (!bResult)
	{
        strcpy(RespBuffer, "Failed to open/close ports!");
		AfxMessageBox(RespBuffer);
		//m_lstFromMcu.AddString(RespBuffer);
		return;
	}
	else
	{
        //strcpy(RespBuffer, "Success to open/close ports!");
		//m_lstFromMcu.AddString(RespBuffer);
	}

	//update gui if success
	m_bConnect = !m_bConnect;
	if (m_bConnect)
		GetDlgItem(IDC_LINK)->SetWindowText("&ClosePort");
	else
		GetDlgItem(IDC_LINK)->SetWindowText("&OpenPort");
	EnableButtons();

	UpdateData(FALSE);
}

void CRfCardDlg::InitPorts()
{
	g_port.hPort = NULL;
	g_port.nType = 1;
	g_port.nNum = 0;
	g_port.BaudRate = 115200;
	m_bUsb = FALSE;
}

void CRfCardDlg::SetPorts()
{
	m_bUsb = FALSE;
	//Port
	int nCheck = GetCheckedRadioButton(IDC_RADIO_USB, IDC_RADIO_UART);
	g_port.nType = (nCheck==IDC_RADIO_USB)?0:1;
	g_port.nNum = m_Comport;
	m_bUsb = (g_port.nType==0)?TRUE:FALSE;
}

BOOL CRfCardDlg::ClosePorts()
{
	DisconnectUsb();
	if (g_port.nType == 0) //USB port
		CloseUsbPort();
	if (g_port.nType == 1) //COM port
		CloseComPort();

	return TRUE;
}

BOOL CRfCardDlg::LinkPorts()
{
	BOOL bResult = TRUE;

	//close old ports
	ClosePorts();

	//get latest ports setting
	SetPorts();

	//connect usb
	if (m_bUsb == TRUE)
	{
		bResult = ConnectUsb();
		if (bResult == FALSE)
			return FALSE;
	}

	//link ports
	bResult = LinkOnePort();

	UpdateData(FALSE);
	return bResult;
}

BOOL CRfCardDlg::LinkOnePort()
{
	BOOL bResult = TRUE;

	if (g_port.hPort == NULL)
	{
		if (g_port.nType == 0) //USB port
			g_port.hPort = OpenUsbPort();
		if (g_port.nType == 1) //COM port
			g_port.hPort = OpenComPort(g_port.nNum, g_port.BaudRate);
	}
	if (g_port.hPort == NULL)
	{
		//CString strMessage = "Failed to open port, please retry!";
		//AfxMessageBox(strMessage);
		return FALSE;
	}

	if (g_port.nType == 1) //COM port
		//purge info in buffer
		PurgeComm(g_port.hPort, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	
	return bResult;
}


//  conver command string to command int code
unsigned short  CmdConvert(CString CmdString)
{
	CString TempStr[71]={"Reset",
		              "Request0",
					  "Request1",
					  "AntiColl",
					  "Select",
					  "Authen",
					  "Inc",
					  "Dec",
					  "Transfer",
					  "Restore",
					  "Write16B",
					  "Write4B",
					  "Read",
					  "Halt0",
					  "SetKeyA",
					  "SetKeyB",
					  "SetSector",
					  "SetBlock",
					  "Compare4B",
					  "Compare16B",
					  "Assert_Error",
					  "Assert_OK",
					  "TestMode",
					  "ExitTest",
					  "DirectCmd",
					  "Note",
					  "Pro_RATS",	
					  "Pro_DESELECT",	
					  "Pro_WriteBlock",	
					  "Pro_ReadBlock",	
					  "Pro_WriteByte",	
					  "Pro_ReadByte",	
					  "Pro_WriteSecurity",	
					  "Pro_ReadSecurity",
					  "Pro_Increase",	
					  "Pro_Decrease",	
					  "Pro_Restore",	
					  "Pro_Transfer",
					  "Pro_Write4B",
					  "Test_WriteBlock",
					  "Test_WriteByte",
	};
	unsigned short CmdVal[41]={0x10,0x26,0x52,0x9320,0x9370,0x60,0x0c1,0xc0,0x0b0,0xc2,0x0a0,0x18,
		                       0x30,0x50,0x11,0x12,0x13,0x14,0x21,0x22,0x48,0x49,0x40,0x43,0x47,0x46,
							   0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
	int i=0;
	while(CmdString!=TempStr[i]){
		i++;
		if(i>=70)//no such command
			return 0;
	}
	return CmdVal[i];
}

unsigned char CmdTranslation(CString CmdStr,unsigned char *CmdPt)
{
	char TempStr[100];
	unsigned char CmdLen=2,ArgLen;	
	strcpy(TempStr,(LPCTSTR)CmdStr);
	char seps[]   = " ";//
	char *token;
    token = strtok(TempStr, seps);
	CmdPt[0] = CmdConvert(token)&0x00ff;
	CmdPt[1] = (CmdConvert(token)&0xff00)>>8;//cmd high
	TRACE("Cmd: %x%x\n",CmdPt[1],CmdPt[0]);
	token = strtok(NULL,seps);
	TRACE("token=%s",token);
	if(token!=NULL)//command has arguements?
	{
    ArgLen = strlen(token);
    AsciiConvert(token,(CmdPt+2),ArgLen);
	CmdLen = ArgLen +2;
	DataLength= ArgLen/2;
	}
    return CmdLen;//cmd length
}

void AsciiConvert(char *AsciiChar,unsigned char *RetBuffer, unsigned char DataCount)
{
	unsigned char i,temp1,temp2;
	char tempchar[100];
	//if have only one number,a "0" must be added
	if(DataCount==1){
	   DataCount++;
	   strcpy(tempchar,"0");
	   strcat(tempchar,AsciiChar);
	   strcpy(AsciiChar,tempchar);
	   }
	TRACE("str:%s",AsciiChar);
    TRACE("DataCnt=%x",DataCount);
	TRACE("ASC=%x",AsciiChar[0]);
    TRACE("ASC=%x",AsciiChar[1]);
	for(i=0;i<((DataCount)/2);i++)  
	{   //ascii code convertion	   
		if((*AsciiChar>='0')&&(*AsciiChar<='9'))
			temp1 = *AsciiChar-0x30;
		else if((*AsciiChar>='a')&&(*AsciiChar<='f'))
            temp1 = *AsciiChar-'a'+10;
		else
            temp1 = *AsciiChar-'A'+10;
		if(((*(AsciiChar+1)>='0')&&(*(AsciiChar+1)<='9')))
			temp2= *(AsciiChar+1)-0x30;
		else if((*(AsciiChar+1)>='a')&&(*(AsciiChar+1)<='f'))
            temp2= *(AsciiChar+1)-'a'+10;
		else
            temp2= *(AsciiChar+1)-'A'+10;
		RetBuffer[i]= (temp1<<4 ) | temp2;  		
		TRACE("retbuf=%x",RetBuffer[i]);
		AsciiChar+=2;
	}
}
/*
void CRfCardDlg::CmdHandler(unsigned char *Cmd)
{
    int Result,i;
	unsigned short CmdType, Addr;
	char   RespBuff[100],tt[5];
	unsigned char CrcDat[2],Resp[200]={0,0,0,0,0,0,0,0,0,0,0,0,0,},TempBuffer[200],TempRecv[100]={0,0,0},SendBuff[200];
	static unsigned char UserID[5],ReadDat[20],Key[6]={0xff,0xff,0xff,0xff,0xff,0xff},RA[4]={0xcd,0x66,0x91,0x98},Sector,Block,RB1[4]={0x37,0xf5,0x75,0x99};
	static BOOL CrcSwitch,ParitySwitch;
	BOOL CrcErr=false,ParityErr=false;
//	for(i=0;i<10;i++)
//		TRACE("CMD2=%x\n",Cmd[i]);
	this->UpdateData(true);
		bJudgError= Error_sum; 
	memset(TempBuffer,0,0);
	CmdType = Cmd[0]|((Cmd[1]&0x00ff)<<8);
    switch(CmdType){
	case 0x10://reset
	if(m_EquSel == RF100)
	{

		SendBuff[0] = 0x10;
		SendBuff[1] = 0x00;     
		SendBuff[2] = 0x01;
		SendBuff[3] = 0x10;
		Result = protocol(CommPort,4, SendBuff,Resp);
		if(Resp[3]==0x00&&Resp[2]==1)
				strcpy(RespBuff,"Rest..........................OK"); 
		else
		{
			    strcpy(RespBuff,"Rest..........................Error"); 
				Error_sum+=1;
		}
	    m_lstFromMcu.AddString(RespBuff);	
		m_bTestMode = false;
		Result =0;

	}
	else if(m_EquSel == RF35)
	{
		st = rf_reset(icdev,50);
		if(st==0)
				strcpy(RespBuff,"Rest...............OK"); 
		else
		{
			    strcpy(RespBuff,"Reset..............Communication Error"); 
				Error_sum+=1;
		}
		m_lstFromMcu.AddString(RespBuff);	

	}

		break;
	case 0x11://setkeyA
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
        SendBuff[0] = 0x11;
		SendBuff[1] = 0x00;
        SendBuff[2] = 0x08;
        SendBuff[3] = 0x11;
		SendBuff[4] = BlockSector;
		for(i=0;i<6;i++)
		{
			Key[5-i] = Cmd[i+2];
		    KeyA[i]=Cmd[i+2];
		}
		TRACE("KEY=%X\n",Key[i]);   
		if(m_EquSel == RF100){
            	for(i=0;i<6;i++)
				SendBuff[10-i] = Cmd[i+2];
			//SendBuff[3+i] = Cmd[i+2];
			//		for(i=0;i<11;i++) TRACE("KEYA=%X",SendBuff[i]);
            Result = protocol(CommPort,11, SendBuff,Resp);
			m_bKeyB_On = false;
			strcpy(RespBuff,"SetKeyA  :"); 

				for(i=0;i<6;i++){//output fromat
							sprintf(tt,"%02x",Key[i]);
							strcat(RespBuff,tt);
							strcat(RespBuff," ");
				}
				m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF35)	 
		{
			m_keymode=0;
			st = rf_load_key(icdev,0,Sector,Key);
			if(st==0)
			{
				strcpy(RespBuff,"SetKeyA............OK:"); 

				for(i=0;i<6;i++){//output fromat
							sprintf(tt,"%02x",Key[5-i]);
							strcat(RespBuff,tt);
							strcat(RespBuff," ");
				}				
			}
			else
			{
				strcpy(RespBuff,"SetKeyA............Communication Error"); 
				Error_sum+=1;
			}
				m_lstFromMcu.AddString(RespBuff);


		}
	
		break;
	case 0x12://setkeyb
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
        SendBuff[0] = 0x12;
		SendBuff[1] = 0x00;
        SendBuff[2] = 0x08;
		SendBuff[3] = 0x12;
		SendBuff[4] = BlockSector;
		for(i=0;i<6;i++)
		{Key[5-i] = Cmd[i+2];KeyB[i]=Cmd[i+2];}
		TRACE("KEY=%X\n",Key[i]);   
		if(m_EquSel == RF100){
			    for(i=0;i<6;i++)
				SendBuff[10-i] = Cmd[i+2];
					for(i=0;i<11;i++) TRACE("KEYA=%X",SendBuff[i]);
            Result = protocol(CommPort,11, SendBuff,Resp);
			m_bKeyB_On = true;
			strcpy(RespBuff,"SetKeyB  :"); 

			for(i=0;i<6;i++){//output fromat
							sprintf(tt,"%02x",Key[i]);
							strcat(RespBuff,tt);
							strcat(RespBuff," ");
				}
				m_lstFromMcu.AddString(RespBuff);
		}

		else if(m_EquSel == RF35)	
		{
			m_keymode=4;
			st = rf_load_key(icdev,4,Sector,Key);
			if(st==0)
			{
				strcpy(RespBuff,"SetKeyB...........OK:"); 

				for(i=0;i<6;i++){//output fromat
							sprintf(tt,"%02x",Key[5-i]);
							strcat(RespBuff,tt);
							strcat(RespBuff," ");
				}				
			}
			else
			{
				strcpy(RespBuff,"SetKeyB............Communication Error"); 
				Error_sum+=1;
			}
				m_lstFromMcu.AddString(RespBuff);

		}
	
		break;

	case 0x13://set sector
		
		Sector = Cmd[2];
		TRACE("sector=%x",Sector);
		BlockSector=Block | (Sector<<2);
	    strcpy(RespBuff,"Set	Sector:");
        sprintf(tt,"%02x",Sector);
		strcat(RespBuff,tt);
        m_lstFromMcu.AddString(RespBuff);
		Result =0;
		break;
    case 0x14://set Block
		Block = Cmd[2];
		BlockSector=Block | (Sector<<2);
		strcpy(RespBuff,"Set	Block:");
        sprintf(tt,"%02x",Block);
		strcat(RespBuff,tt);
        m_lstFromMcu.AddString(RespBuff);
		Result =0;
		break;
	case  0x21://compare
  		if(m_EquSel == RF35)
		{
			i=0;
			while(ReadDat[12+i]==Cmd[i+2]&& i<4)
			i++;
			if(i==4)
				strcpy(RespBuff,"Compare4B..........OK"); 
			else
			{				Error_sum+=1;
				strcpy(RespBuff,"Compare4B..........Error"); 
			}
			  
			  m_lstFromMcu.AddString(RespBuff);
			  Result=0;
		}
     break;
     case  0x22://compare16B
  	  	if(m_EquSel == RF35)
		{
			i=0;
			while(ReadDat[i]==Cmd[i+2]&& i<16)
			i++;
			if(i==16)
				strcpy(RespBuff,"Compare16B.........OK"); 
			else
			{
								Error_sum+=1;
				strcpy(RespBuff,"Compare16B.........Error"); 
			}
			  
			  m_lstFromMcu.AddString(RespBuff);
			 Result =0;
		}
     break;
	case 0x26://request0
  	if(m_EquSel == RF100)	
	{
		SendBuff[0] = 0x26;
		SendBuff[1] = 0x00;     
		SendBuff[2] = 0x01;
		SendBuff[3] = 0x26;
		i=0;
		do{
				Result = protocol(CommPort,4,SendBuff,Resp);
				i++;
			}while(i<1);
		if(Resp[2]==2)
		{ 
			 strcpy(RespBuff,"Request0...........Ok"); 			
				
		}
		else if(Resp[2]==1)
		{
			strcpy(RespBuff,"Request0...........Communication Error");

			if((Resp[3]&0x40)==0x40)
			{	
				strcat(RespBuff,"TE");
				strcat(RespBuff," ");
			}
			if((Resp[3]&0x08)==0x08)
				strcat(RespBuff,"BE");
		}
		else
			strcpy(RespBuff,"Request0...........Communication Error");
			
			m_lstFromMcu.AddString(RespBuff);
				strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
	}
	else if(m_EquSel == RF35)
	{
			unsigned __int16 TagType;
			st = rf_request(icdev,0,&TagType);
			if(st==0)
			{ 
			   strcpy(RespBuff,"Request0...........Ok"); 
			}
			else
			{
				strcpy(RespBuff,"Request0...........Communication Error");
				Error_sum+=1;
			}
	    m_lstFromMcu.AddString(RespBuff);

	}
		break;
	case 0x52://request all
	if(m_EquSel == RF100)
	{
        SendBuff[0] = 0x52;
		SendBuff[1] = 0x00;     
		SendBuff[2] = 0x01;
		SendBuff[3] = 0x52;
	//	strcpy(RespBuff,"************LET'S GO*********************************"); 
	//    m_lstFromMcu.AddString(RespBuff);
		i=0;
			do{
				Result = protocol(CommPort,4,SendBuff,Resp);
	    		i++;
			}while(i<1);
			if(Resp[2]==2)
			{ 
			   strcpy(RespBuff,"Request1...........Ok"); 			
			}
			else if(Resp[2]==1)
			{
				strcpy(RespBuff,"Request1...........Communication Error");

				if((Resp[3]&0x40)==0x40)
				{	
					strcat(RespBuff,"TE");
					strcat(RespBuff," ");
				}
				if((Resp[3]&0x08)==0x08)
					strcat(RespBuff,"BE");
			}
			else
				strcpy(RespBuff,"Request1...........Communication Error");
	    m_lstFromMcu.AddString(RespBuff);
				strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
	}
	else if(m_EquSel == RF35)
	{
			unsigned __int16 TagType;
			st = rf_request(icdev,1,&TagType);
			if(st==0)
			{ 
			   strcpy(RespBuff,"Request1...........Ok:"); 
			}
			else
			{
				strcpy(RespBuff,"Request1...........Error:");
			}
	    m_lstFromMcu.AddString(RespBuff);
	}


		break;
		

	case 0x9320:
		if(m_EquSel == RF35)
		{	//	st=rf_card(icdev,0,&Snr);
			st = rf_anticoll(icdev,0, &Snr);
			if(st==0)
			{		
					unsigned long	temp_Snr;
					temp_Snr  =Snr;
					for(i=0;i<5;i++)
					{
						UserID[i] = (unsigned char)(temp_Snr%(256));
						temp_Snr= temp_Snr/(256);
					}
				strcpy(RespBuff,"AntiColl...User ID:");					
					for(i=0;i<4;i++){//output fromat
								sprintf(tt,"%02x",UserID[i]);
								strcat(RespBuff,tt);
								strcat(RespBuff," ");
					}
			}
			else
			{
				Error_sum+=1;
				strcpy(RespBuff,"AntiColl...........Communication Error");

			}
			m_lstFromMcu.AddString(RespBuff);
			

		}
		else if(m_EquSel == RF100)
		{//mcm200
			SendBuff[0] = 0x93;
			SendBuff[1] = 0x20;
			SendBuff[2] = 0x02;
			SendBuff[3] = 0x93;
			SendBuff[4] = 0x20;
            Resp[2]=0;
     		Result = protocol(CommPort,5, SendBuff,Resp);
			if(Resp[2]==5)
			{
					for(i=0;i<5;i++)
						UserID[i] = Resp[i+3];

					for(i=0;i<4;i++){//output fromat
					user_IDdata[i]=UserID[3-i];		
					}
					//display user ID
					strcpy(RespBuff,"UserID:");					
					for(i=0;i<5;i++){//output fromat
								sprintf(tt,"%02x",UserID[i]);
								strcat(RespBuff,tt);
								strcat(RespBuff," ");
					}
				//	m_lstFromMcu.AddString(RespBuff);
			}
			else if(Resp[2]==1)
			{

				strcpy(RespBuff,"AntiColl...........Error:");
				for(i=0;i<1;i++){//output fromat
						sprintf(tt,"%02x",Resp[3+i]);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
					}
				if((Resp[3]&0x40)==0x40)
				{	
					strcat(RespBuff,"TE");
					strcat(RespBuff," ");
				}
			    if((Resp[3]&0x08)==0x08)
					strcat(RespBuff,"BE");		
			}
			else
				strcpy(RespBuff,"AntiColl...........Communication Error");
			m_lstFromMcu.AddString(RespBuff);
		}
		break;
	case 0x9370://select
		if(m_EquSel == RF35)
		{
			unsigned char Size;
			st = rf_select(icdev,Snr,&Size);
			if(st==0)
			{
			   strcpy(RespBuff,"Select.............OK:"); 
			   for(i=0;i<1;i++)
			   {//output fromat
						sprintf(tt,"%02x",Size);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
			   }
			}
			else
			{
				strcpy(RespBuff,"Select.............Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
			SendBuff[0] = 0x93;
			SendBuff[1] = 0x70;
			SendBuff[2] = 0x07;
            SendBuff[3] = 0x93;
			SendBuff[4] = 0x70;
			for(i=0;i<5;i++)
				SendBuff[i+5] = UserID[i];
			Resp[2]=0;
			Result = protocol(CommPort,10, SendBuff,Resp);
			//display select 
			if(Resp[2]==1)
			{	
			   strcpy(RespBuff,"Select.............OK:"); 
			   for(i=0;i<1;i++)
			   {//output fromat
						sprintf(tt,"%02x",Resp[3+i]);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
			   }			
			}		
			else if(Resp[2]==2)
			{
				strcpy(RespBuff,"Select.............Error:");
 				for(i=0;i<1;i++){//output fromat
						sprintf(tt,"%02x",Resp[3+i]);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
				}
				if((Resp[3]&0x40)==0x40)
				{	
					strcat(RespBuff,"TE");
					strcat(RespBuff," ");
				}
			    if((Resp[3]&0x08)==0x08)
				{
					strcat(RespBuff,"BE");
					strcat(RespBuff," ");
				}
				if((Resp[3]&0x10)==0x10)
				{	
					strcat(RespBuff,"CE");
					strcat(RespBuff," ");
				}
			    if((Resp[3]&0x20)==0x20)
					strcat(RespBuff,"PE");				


			}
			else
				strcpy(RespBuff,"Select.............Communication Error");

			m_lstFromMcu.AddString(RespBuff);
			
			
		}

        break;
	case 0x60:
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
		if(m_EquSel == RF35)
		{

			st = rf_authentication(icdev,m_keymode,Sector);

			if(st==0)
			{	
				strcpy(RespBuff,"Authentication.....OK"); 	
			}
			else
			{
				strcpy(RespBuff,"Authentication.....Communication Error"); 
				Error_sum+=1;
			}
					m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
            SendBuff[0] = m_bKeyB_On?0x61:0x60; //key a or key b
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x02;
			SendBuff[3] = m_bKeyB_On?0x61:0x60;
			SendBuff[4] = BlockSector;//Block | (Sector<<2);//sector and block number
			Resp[2]=0;
			for(i=0;i<5;i++)
			TRACE("60OUT=%X",SendBuff[i]);
            Result = protocol(CommPort,5, SendBuff,Resp);
					if(Resp[2]==3)//&&Resp[3]==0)
			{
		    	strcpy(RespBuff,"Authentication ...............OK:"); 				
				m_lstFromMcu.AddString(RespBuff);
			}
			else
			{		
				Error_sum+=1;
				strcpy(RespBuff,"Authentication ...............Error"); 
				m_lstFromMcu.AddString(RespBuff);

			}
	
		}
        break;
	case 0xc1://increase
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
		if(m_EquSel == RF35)
		{
			unsigned long value;
			if(DataLength==4)
				value = Cmd[5];
			else
				value = Cmd[2];
			st = rf_increment(icdev,(Sector*4+Block),value);

			if(st==0){//return 16B	
				strcpy(RespBuff,"Increase...........OK");
			}
			else
			{
				strcpy(RespBuff,"Increase...........Communication Error"); 
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
            SendBuff[0] = 0x0c1; 
			SendBuff[1] = 0x00;//not encrypt
			SendBuff[2] = 0x06; 
			SendBuff[3] = 0x0c1;
			SendBuff[4] = BlockSector;//Block | (Sector<<2);
            for(i=0;i<4;i++)
				SendBuff[i+5] = Cmd[i+2];
			Resp[2]=0;
            Result = protocol(CommPort,9, SendBuff,Resp);
			if(Resp[2]!=0)
			{
				Error_sum+=1;
				if(Resp[3]==0x0a)
					{
					  if(Resp[4]==0x04)
					  {
						Error_sum =Error_sum-1;
					   strcpy(RespBuff,"Increase stage2..............OVERFLOW"); 
					  }
					  else if (Resp[4]==0x01)
					   strcpy(RespBuff,"Increase Stage2..............CRC ERROR"); 					  
					  else if (Resp[4]==0x0FF)
					  {
						strcpy(RespBuff,"Increase Stage2...............OK");
						Error_sum =Error_sum -1;
					  }
					  else if (Resp[4]==0x0FE)
					   strcpy(RespBuff,"Increase Stage2...............BERROR");
					  else{strcpy(RespBuff,"Increase Stage2.............UNKNOWN");} 
					}
				else if(Resp[3]==0x05)
					strcpy(RespBuff,"Increase Stage1.................CRC ERROR"); 
				else if(Resp[3]==0x04)
                    strcpy(RespBuff,"Increase stage1................CAN'T INC ERROR"); 
	    		else{
					strcpy(RespBuff,"Increase Stage1................UNKNOWN ERROR"); 	}					
				
			}	
			else
			{
				strcpy(RespBuff,"Increase ....................UNKNOWN ERROR");
				Error_sum+=1;
			}					

			m_lstFromMcu.AddString(RespBuff);
			Result=0;
		}
		break;
	case 0x0b0://trasnfer 
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
		if(m_EquSel == RF35)
		{
			st = rf_transfer(icdev,(Sector*4+Block));
			strcpy(RespBuff,"Transfer...........OK"); 
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
            SendBuff[0] = 0x0b0; 
			SendBuff[1] = 0x00;//not encrypt
			SendBuff[2] = 0x02; 
			SendBuff[3] = 0x0b0;
			SendBuff[4] = BlockSector;//Block | (Sector<<2);
           
			Resp[2]=0;
            Result = protocol(CommPort,5, SendBuff,Resp);
			if(Resp[2]!=0)
				strcpy(RespBuff,"Transfer .....................OK"); 
			else
			{
				strcpy(RespBuff,"Transfer......................Error"); 
				Error_sum+=1;
			}
	    	  m_lstFromMcu.AddString(RespBuff);
		}
			
		break;
	case 0x18://write2
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
		if(m_EquSel == RF35)
		{
				unsigned char temp_wdata[16];
            for(i=0;i<4;i++)
					temp_wdata[3-i] = Cmd[i+2];
		    for(i=4;i<8;i++)
					temp_wdata[i]=~temp_wdata[i-4];
			for(i=8;i<12;i++)
					temp_wdata[i]=temp_wdata[i-8];
			temp_wdata[12]=(Sector*4+Block);
			temp_wdata[13]=~(Sector*4+Block);
            temp_wdata[14]=(Sector*4+Block);
			temp_wdata[15]=~(Sector*4+Block);

		st=rf_write(icdev,(Sector*4+Block),temp_wdata);//
			if(st==0){//return 16B	
					strcpy(RespBuff,"Write .............OK"); 	
				}
			else
			{
					strcpy(RespBuff,"Write .............Communication Error");
					Error_sum+=1;
					
			}
			m_lstFromMcu.AddString(RespBuff);	


		}
		else if(m_EquSel == RF100)
		{
            SendBuff[0] = 0x0a0; 
			SendBuff[1] = 0x00;//not encrypt
			SendBuff[2] = 0x12; 
			SendBuff[3] = 0x0a0;
			SendBuff[4] =BlockSector;//Block | (Sector<<2);
            for(i=0;i<4;i++)
					SendBuff[8-i] = Cmd[i+2];
		    for(i=9;i<13;i++)
					SendBuff[i]=~SendBuff[i-4];
			for(i=13;i<17;i++)
					SendBuff[i]=SendBuff[i-8];
			SendBuff[17]=SendBuff[4];
			SendBuff[18]=~SendBuff[4];
            SendBuff[19]=SendBuff[4];
			SendBuff[20]=~SendBuff[4];
			Resp[2]=0;
            Result = protocol(CommPort,21, SendBuff,Resp);
			if(Resp[2]==2&&Resp[4]==0x0a&&Resp[3]==0x0a)
					strcpy(RespBuff,"Write ........................OK"); 
			else
			{
					strcpy(RespBuff,"Write.........................Error"); 

					Error_sum+=1;
			}
	    		m_lstFromMcu.AddString(RespBuff);
		}
		break;
	case 0x0a0://write
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
	    if(m_EquSel == RF35)
		{
			unsigned char temp_wdata[16];
            for(i=0;i<16;i++)
					temp_wdata[15-i] = Cmd[i+2];
		st=rf_write(icdev,(Sector*4+Block),temp_wdata);//
			if(st==0){//return 16B	
					strcpy(RespBuff,"Write .............OK"); 	
				}
			else
			{
					strcpy(RespBuff,"Write .............Communication Error"); 
					Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
                       SendBuff[0] = 0x0a0; 
			SendBuff[1] = 0x00;//not encrypt
			SendBuff[2] = 0x12; 
			SendBuff[3] = 0x0a0;
			SendBuff[4] = BlockSector;//Block | (Sector<<2);
            for(i=0;i<16;i++)
					SendBuff[20-i] = Cmd[i+2];
		    Resp[2]=0;
            Result = protocol(CommPort,21, SendBuff,Resp);
			if(Resp[2]==2&&Resp[4]==0x0a&&Resp[3]==0x0a)
					strcpy(RespBuff,"Write ........................OK"); 
			else
			{
					strcpy(RespBuff,"Write.........................Error"); 

					Error_sum+=1;
			}
	    	m_lstFromMcu.AddString(RespBuff);
			
		}
		break;
    case 0x30://Read
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
		if(m_EquSel == RF35)
		{
			st = rf_read(icdev,(Sector*4+Block),Resp);

			if(st==0){//return 16B	
			   strcpy(RespBuff,"Read	Data:"); 
			   for(i=0;i<16;i++){//output fromat
				   		ReadDat[i]=Resp[15-i];
						sprintf(tt,"%02x",Resp[15-i]);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
						//ReadDat[i]=Resp[18-i];
						//TRACE("rddat=%x",ReadDat[i]);
				   }
			}
			else
			{
				strcpy(RespBuff,"Read...............Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);

		}
		else if(m_EquSel == RF100)
		{
			SendBuff[0] = 0x30; 
			SendBuff[1] = 0x00;//not encrypt
			SendBuff[2] = 0x02;
			SendBuff[3] = 0x30;
			SendBuff[4] = BlockSector;//Block | (Sector<<2);
			TRACE("blksec=%x",SendBuff[4]);
			Resp[2]=0;
			Result = protocol(CommPort,5, SendBuff,Resp);
            if(Resp[2]==16){//return 16B	
			   strcpy(RespBuff,"Read Data:"); 
			   for(i=0;i<16;i++){//output fromat
						sprintf(tt,"%02x",Resp[18-i]);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
						ReadDat[i]=Resp[18-i];
						//TRACE("rddat=%x",ReadDat[i]);
				   }
			}
	     	else if(Resp[2]==2)
			{
                strcpy(RespBuff,"Read...............Error:"); 

 				for(i=0;i<2;i++){//output fromat
						sprintf(tt,"%02x",Resp[3+i]);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
				}
				if((Resp[3]&0x40)==0x40)
				{	
					strcat(RespBuff,"TE");
					strcat(RespBuff," ");
				}
			    if((Resp[3]&0x08)==0x08)
				{
					strcat(RespBuff,"BE");
					strcat(RespBuff," ");
				}
				if((Resp[3]&0x10)==0x10)
				{	
					strcat(RespBuff,"CE");
					strcat(RespBuff," ");
				}
			    if((Resp[3]&0x20)==0x20)
					strcat(RespBuff,"PE");				  
				
			}
			else
				strcpy(RespBuff,"Read...............Communication Error");
			m_lstFromMcu.AddString(RespBuff);

		}
		break;
    case 0xc0://decrease
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
		if(m_EquSel == RF35)
		{
			unsigned long value;
			if(DataLength==4)
				value = Cmd[5];
			else
				value = Cmd[2];
			st = rf_decrement(icdev,(Sector*4+Block),value);

			if(st==0){//return 16B	
				strcpy(RespBuff,"Decrease...........OK"); 
			}
			else
			{
				strcpy(RespBuff,"Decrease...........Communication Error"); 

				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
            SendBuff[0] = 0x0c0; 
			SendBuff[1] = 0x00;//not encrypt
			SendBuff[2] = 0x06; 
			SendBuff[3] = 0x0c0;
			SendBuff[4] = BlockSector;//Block | (Sector<<2);
            for(i=0;i<4;i++)
				SendBuff[i+5] = Cmd[i+2];
			Resp[2]=0;
            Result = protocol(CommPort,9, SendBuff,Resp);
			if(Resp[2]!=0)
			{
				Error_sum+=1;
				if(Resp[3]==0x0a)
					{				
					if(Resp[4]==0x04)
					{
						Error_sum =Error_sum-1;
					   strcpy(RespBuff,"Decrease stage2..............OVERFLOW"); 
					}
					  else if (Resp[4]==0x01)
					   strcpy(RespBuff,"Decrease Stage2..............CRC ERROR"); 					  
					  else if (Resp[4]==0x0FF)
					  {
						strcpy(RespBuff,"Decrease Stage2...............OK");
						Error_sum =Error_sum-1;
					  }
					  else if (Resp[4]==0x0FE)
					   strcpy(RespBuff,"Decrease Stage2...............BERROR");
					  else{strcpy(RespBuff,"Decrease Stage2.............UNKNOWN");} 

				}
				else if(Resp[3]==0x05)
					strcpy(RespBuff,"Decrease Stage1.................CRC ERROR"); 
				else if(Resp[3]==0x04)
                    strcpy(RespBuff,"Decrease stage1................CAN'T DEC ERROR"); 
	    		else{
					strcpy(RespBuff,"Decrease Stage1................UNKNOWN ERROR"); 	}					
				
			}	
			else{
				strcpy(RespBuff,"Decrease ....................UNKNOWN ERROR"); 
					Error_sum+=1;	
			}					

			m_lstFromMcu.AddString(RespBuff);
			Result=0;
		}
		break;
			
    case 0xc2://Restore
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
		if(m_EquSel == RF35)
		{	      
 			st = rf_restore(icdev,(Sector*4+Block));

			if(st==0){//return 16B	
				strcpy(RespBuff,"Restore............OK"); 
			}
			else
			{
				strcpy(RespBuff,"Restore............Communication Error"); 
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
            SendBuff[0] = 0x0c2; 
			SendBuff[1] = 0x00;//not encrypt
			SendBuff[2] = 0x06; 
			SendBuff[3] = 0x0c2;
			SendBuff[4] = BlockSector;//Block | (Sector<<2);
            for(i=0;i<4;i++)
				SendBuff[i+5] = Cmd[i+2];
			Resp[2]=0;
            Result = protocol(CommPort,9, SendBuff,Resp);
			if(Resp[2]!=0)
			{
				Error_sum+=1;	
				if(Resp[3]==0x0a)
					{
					 if (Resp[4]==0x01)
					   strcpy(RespBuff,"Restore Stage2..............CRC ERROR"); 					  
					  else if (Resp[4]==0x0FF)
					  {
					   	Error_sum=Error_sum-1;	
						strcpy(RespBuff,"Restore Stage2...............OK");

					  }
					  else if (Resp[4]==0x0FE)
					   strcpy(RespBuff,"Restore Stage2...............BERROR");
					  else{strcpy(RespBuff,"Restore Stage2.............UNKNOWN");} 

					}
				else if(Resp[3]==0x05)
					strcpy(RespBuff,"Restore Stage1.................CRC ERROR"); 
				else if(Resp[3]==0x04)
                    strcpy(RespBuff,"Restore stage1................CAN'T RES ERROR"); 
	    		else{
					strcpy(RespBuff,"Restore Stage1................UNKNOWN ERROR"); 	}					
				
			}	
			else{
				
				strcpy(RespBuff,"Restore ...................UNKNOWN ERROR"); 
				Error_sum+=1;	
			}					

			m_lstFromMcu.AddString(RespBuff);
			Result=0;
		}
		break;
	case 0x50://halt(no encrypt)
		if(Sector > 0x0f || Block > 3)
		{
			if(Sector > 0x0f)
				m_lstFromMcu.AddString("Error ...Sector out of range, input HEX!");
			if(Block > 0x03)
				m_lstFromMcu.AddString("Error ...Block out of range, input HEX!");
			Error_sum+=1;
			Sector = 0;
			Block = 0;
			break;
		}
		if(m_EquSel == RF35)
		{
			st=rf_halt(icdev);
			if(st==0){//return 16B	
				strcpy(RespBuff,"Halt...............OK");
			}
			else
			{
				 strcpy(RespBuff,"Halt...............Communication Error");
				 Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);

		}
        else if(m_EquSel == RF100)
		{
            SendBuff[0] = 0x50; 
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x01;
			SendBuff[3] = 0x50;
			SendBuff[4] = BlockSector;//Block | (Sector<<2);
            Result = protocol(CommPort,5, SendBuff,Resp);//first stage
			if((Resp[2]==2)&&((Resp[3]&0x08)!=0x08)&&(Resp[4]!=0x04)&&(Resp[4]!=0x05))
			{	strcpy(RespBuff,"Halt...............OK:");
						for(i=0;i<2;i++){//output fromat
						sprintf(tt,"%02x",Resp[3+i]);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
						}
			}
			else if(Resp[2]==2)
			{   strcpy(RespBuff,"Halt...............ERROR");
					for(i=0;i<2;i++){//output fromat
						sprintf(tt,"%02x",Resp[3+i]);
						strcat(RespBuff,tt);
						strcat(RespBuff," ");
						}
				if((Resp[3]&0x40)==0x40)
				{	
					strcat(RespBuff,"TE");
					strcat(RespBuff," ");
				}
			    if((Resp[3]&0x08)==0x08)
				{
					strcat(RespBuff,"BE");
					strcat(RespBuff," ");
				}  
			
			}
			else
			   strcpy(RespBuff,"Halt...............Communication Error");	
			m_lstFromMcu.AddString(RespBuff);

		}
		break;
		case 0x47://direct command
       	if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = DataLength - 2;
			for(i=0;i<DataLength - 2;i++)
				SendBuff[i+4] = Cmd[i+2];

			strcpy(RespBuff,"--------Direct Command------");
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] =0;
			strcpy(RespBuff,"SEND:"); 
		    for(i=0;i<DataLength;i++){//output fromat
	    		sprintf(tt,"%02x",Cmd[i+2]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);

			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			
			RespBuff[0] = 0;
	   		for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
			SendBuff[0] = 0x48;
			SendBuff[1] = 0x00;     
			SendBuff[2] = DataLength+1;
			SendBuff[3] = 0x48;
			for(i=0;i<DataLength;i++)
				SendBuff[i+4] = Cmd[i+2];

			strcpy(RespBuff,"--------Direct Command------");
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] =0;
			strcpy(RespBuff,"SEND:"); 
		    for(i=0;i<DataLength;i++){//output fromat
	    		sprintf(tt,"%02x",Cmd[i+2]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);

			Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}

		break;	
   	case 0x01:			  //"Pro_RATS",	
		if(m_EquSel == RF35)
		{
			st=rf_pro_rst(icdev,Resp);
			if(st==0)//return 16B
			{	
				strcpy(RespBuff,"RATS.....................OK:");
			}
			else
			{
				strcpy(RespBuff,"RATS...............Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	   		
			for(i=0;i<Resp[0];i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);

		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 0x04;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0xe0;
				SendBuff[5] = 0xf0;
				SendBuff[6] = 0xb6;
				SendBuff[7] = 0x00;

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[1]!=0x66){
						strcpy(RespBuff,"RATS.....................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"RATS.....................Error");
						Error_sum+=1;
				}
	    			m_lstFromMcu.AddString(RespBuff);

			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}

		break;
	case 0x02:			  //"Pro_DESELECT",	
		if(m_EquSel == RF35)
		{
			st=rf_pro_halt(icdev);
			if(st==0)//return 16B
			{	
				strcpy(RespBuff,"DESELECT............. OK:"); 
			}
			else
			{
				strcpy(RespBuff,"DESELECT.............Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);

		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 0x04;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0xc2;
				SendBuff[5] = 0xf0;
				SendBuff[6] = 0x35;
				SendBuff[7] = 0x10;

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[1]!=0x66){
						strcpy(RespBuff,"DESELECT.....................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"DESELECT.....................Error");
						Error_sum+=1;
				}
	    			m_lstFromMcu.AddString(RespBuff);
					RespBuff[0] = 0;
					strcpy(RespBuff,"RESP:"); 
	   				for(i=3;i<(Resp[2]+3);i++)
					{//output fromat
	    				sprintf(tt,"%02x",Resp[i]);
	    				strcat(RespBuff,tt);
	   					strcat(RespBuff," ");
					}
			m_lstFromMcu.AddString(RespBuff);
		}

		break;		

	case 0x03:			  //"Pro_WriteBlock",	
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x14;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0xa0;
			SendBuff[6] = Sector;
			SendBuff[7] = Block;
			for(i=0;i<16;i++)
				SendBuff[8+i] = Cmd[i+2];
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[4]!=0x66)
			{
				strcpy(RespBuff,"Write Block OK:"); 
			}
			else
			{
				strcpy(RespBuff,"Write Block..........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 23;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0xa0;
				SendBuff[7] = Sector;
				SendBuff[8] = Block;
				for(i=0;i<16;i++)
				SendBuff[9+i] = Cmd[i+2];

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i = 0; i < 20; i++) 
					CrcCreate(SendBuff[i+5],1,CrcDat);
				SendBuff[25] = CrcDat[1];
				SendBuff[26] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Write Block.....................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Write Block.....................Error");
						Error_sum+=1;
				}
	    		m_lstFromMcu.AddString(RespBuff);
				RespBuff[0] = 0;
				strcpy(RespBuff,"RESP:"); 
	   			for(i=3;i<(Resp[2]+3);i++)
				{//output fromat
	    			sprintf(tt,"%02x",Resp[i]);
	    			strcat(RespBuff,tt);
	   				strcat(RespBuff," ");
				}
				m_lstFromMcu.AddString(RespBuff);
			}


		break;		
	case 0x04:			  //"Pro_ReadBlock",	
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x04;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0x30;
			SendBuff[6] = Sector;
			SendBuff[7] = Block;
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"read Block OK:"); 
			}
			else
			{
				strcpy(RespBuff,"read Block..........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 7;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0x30;
				SendBuff[7] = Sector;
				SendBuff[8] = Block;

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i = 0; i < 4; i++) 
					CrcCreate(SendBuff[i+5],1,CrcDat);

				SendBuff[9] = CrcDat[1];
				SendBuff[10] = CrcDat[0];  
				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"read Block.....................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"read Block.....................Error");
						Error_sum+=1;
				}
	    	m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<Resp[2]+3;i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);

		}

		break;	
	case 0x05:			  //"Pro_WriteByte",	
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x05;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0xa1;
			for(i=0;i<3;i++)
				SendBuff[6+i] = Cmd[i+2];
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"Write Byte OK:"); 
			}
			else
			{
				strcpy(RespBuff,"Write Byte.......Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 8;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0xa1;
				for(i=0;i<3;i++)
				SendBuff[7+i] = Cmd[i+2];

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i = 0; i < 5; i++) 
					CrcCreate(SendBuff[i+5],1,CrcDat);

				SendBuff[10] = CrcDat[1];
				SendBuff[11] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Write Byte.................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Write Byte...............Error");
						Error_sum+=1;
				}
	    		m_lstFromMcu.AddString(RespBuff);
				RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);

		}

		break;		
	case 0x06:			  //"Pro_ReadByte",	
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x04;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0x31;
			for(i=0;i<2;i++)
				SendBuff[6+i] = Cmd[i+2];
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"Read byte OK:"); 
			}
			else
			{
				strcpy(RespBuff,"Read byte.........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 7;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0x31;
				for(i=0;i<2;i++)
				SendBuff[7+i] = Cmd[i+2];

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i = 0; i < 4; i++) 
					CrcCreate(SendBuff[i+5],1,CrcDat);

				SendBuff[9] = CrcDat[1];
				SendBuff[10] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Read byte.................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Read byte...............Error");
						Error_sum+=1;
				}
	    	m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);

		}

		break;
	case 0x07:			  //"Pro_WriteSecurity",	
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x13;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0xa2;
			SendBuff[6] = Block;
			for(i=0;i<16;i++)
				SendBuff[7+i] = Cmd[i+2];
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"Write Security OK:"); 
			}
			else
			{
				strcpy(RespBuff,"Write Security..........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 22;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
			    SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0xa2;
				SendBuff[7] = Block;
				for(i=0;i<16;i++)
				SendBuff[8+i] = Cmd[i+2];

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i = 0; i < 19; i++) 
					CrcCreate(SendBuff[i+5],1,CrcDat);
				SendBuff[24] = CrcDat[1];
				SendBuff[25] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Write Security.....................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Write Security.....................Error");
						Error_sum+=1;
				}
	    	m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}

		break;		
	case 0x08:			  //"Pro_ReadSecurity",
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x03;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0x32;
			SendBuff[6] = Block;
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"read Security OK:"); 
			}
			else
			{
				strcpy(RespBuff,"read Security..........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 6;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0x32;
				SendBuff[7] = Block;
                CrcCreate(SendBuff[4],0,CrcDat);
				for(i = 0; i<3;i++)
				CrcCreate(SendBuff[5+i],1,CrcDat);

				SendBuff[8] = CrcDat[1];
				SendBuff[9] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"read Security..................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"read Security................Error");
						Error_sum+=1;
				}
	    		m_lstFromMcu.AddString(RespBuff);
				RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
	   			strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}

		break;		
	case 0x09:			  //"Pro_Increase",	
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x08;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0xc1;
			SendBuff[6] = Sector;
			SendBuff[7] = Block;
			for(i=0;i<4;i++)
				SendBuff[11-i] = Cmd[i+2];
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"Increase  OK:"); 
			}
			else
			{
				strcpy(RespBuff,"Increase........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 11;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0xc1;
				SendBuff[7] = Sector;
			    SendBuff[8] = Block;

				for(i=0;i<4;i++)
				  SendBuff[12-i] = Cmd[i+2];

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i=0;i<8;i++)
					CrcCreate(SendBuff[5+i],1,CrcDat);

				SendBuff[13] = CrcDat[1];
				SendBuff[14] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Increase................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Increase..............Error");
						Error_sum+=1;
				}
	    		m_lstFromMcu.AddString(RespBuff);
				RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}

			m_lstFromMcu.AddString(RespBuff);
		}
		
		break;		
	case 0x0a:			  //"Pro_Decrease",	
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x08;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0xc0;
			SendBuff[6] = Sector;
			SendBuff[7] = Block;
			for(i=0;i<4;i++)
				SendBuff[11-i] = Cmd[i+2];
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"Decrease OK:"); 
			}
			else
			{
				strcpy(RespBuff,"Decrease.........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 11;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0xc0;
				SendBuff[7] = Sector;
			    SendBuff[8] = Block;

				for(i=0;i<4;i++)
				  SendBuff[12-i] = Cmd[i+2];

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i=0;i<8;i++)
					CrcCreate(SendBuff[5+i],1,CrcDat);

				SendBuff[13] = CrcDat[1];
				SendBuff[14] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Decrease................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Decrease..............Error");
						Error_sum+=1;
				}
	    	m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}

			m_lstFromMcu.AddString(RespBuff);
		}

		break;		
	case 0x0b:			  //"Pro_Restore",	
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x08;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0xc2;
			SendBuff[6] = Sector;
			SendBuff[7] = Block;
			for(i=0;i<4;i++)
				SendBuff[11-i] = Cmd[i+2];
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"Restore   OK:"); 
			}
			else
			{
				strcpy(RespBuff,"Restore ........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 11;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0xc2;
				SendBuff[7] = Sector;
			    SendBuff[8] = Block;

				for(i=0;i<4;i++)
				  SendBuff[12-i] = Cmd[i+2];

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i=0;i<8;i++)
					CrcCreate(SendBuff[5+i],1,CrcDat);

				SendBuff[13] = CrcDat[1];
				SendBuff[14] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Restore................OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Restore..............Error");
						Error_sum+=1;
				}
	    			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}

			m_lstFromMcu.AddString(RespBuff);
		}

		break;		
	case 0x0c:			  //"Pro_Transfer",
		if(m_EquSel == RF35)
		{
			SendBuff[0] = 0x00;
			SendBuff[1] = 0x00;
			SendBuff[2] = 0x00;
			SendBuff[3] = 0x04;
			SendBuff[4] = 0x80;
			SendBuff[5] = 0xb0;
			SendBuff[6] = Sector;
			SendBuff[7] = Block;
			st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
			if(Resp[0] != 0x66)
			{
				strcpy(RespBuff,"Transfer OK:"); 
			}
			else
			{
				strcpy(RespBuff,"Transfer.........Communication Error");
				Error_sum+=1;
			}
			m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
			for(i=4;i<(Resp[3]+4);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}
		else if(m_EquSel == RF100)
		{
				DataLength	= 7;
				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0xb0;
				SendBuff[7] = Sector;
			    SendBuff[8] = Block;

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i=0;i<4;i++)
					CrcCreate(SendBuff[5+i],1,CrcDat);

				SendBuff[9] = CrcDat[1];
				SendBuff[10] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Transfer..............OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Transfer............Error");
						Error_sum+=1;
				}
	    		m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}

			m_lstFromMcu.AddString(RespBuff);
		}

		break;	
	case 0x0d: // "Pro_Write4B",
			if(m_EquSel == RF35)
			{
				SendBuff[0] = 0x00;
				SendBuff[1] = 0x00;
				SendBuff[2] = 0x00;
				SendBuff[3] = 0x14;
				SendBuff[4] = 0x80;
				SendBuff[5] = 0xa0;
				SendBuff[6] = Sector;
			    SendBuff[7] = Block;

				for(i=0;i<4;i++)
					SendBuff[11-i] = Cmd[i+2];
				for(i=12;i<16;i++)
					SendBuff[i]=~SendBuff[i-4];
				for(i=16;i<20;i++)
					SendBuff[i]=SendBuff[i-8];
				SendBuff[20]=(Sector*4+Block);
				SendBuff[21]=~(Sector*4+Block);
				SendBuff[22]=(Sector*4+Block);
				SendBuff[23]=~(Sector*4+Block);


				st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
				if(st==0){//return 16B	
						strcpy(RespBuff,"Write 4B .............OK"); 	
					}
				else
				{
						strcpy(RespBuff,"Write 4B ............ Communication Error");
						Error_sum+=1;
						
				}
				m_lstFromMcu.AddString(RespBuff);	
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 	  
					  for(i=4;i<(Resp[3]+4);i++)
					{//output fromat
	    				sprintf(tt,"%02x",Resp[i]);
	    				strcat(RespBuff,tt);
						strcat(RespBuff," ");
					}
			m_lstFromMcu.AddString(RespBuff);
			}
			else if(m_EquSel == RF100)
			{
				DataLength	= 23;

				SendBuff[0] = 0x48;
				SendBuff[1] = 0x00;     
				SendBuff[2] = DataLength+1;
				SendBuff[3] = 0x48;
				SendBuff[4] = 0x02;
				SendBuff[5] = 0x80;
				SendBuff[6] = 0xa0;
				SendBuff[7] = Sector;
			    SendBuff[8] = Block;

				for(i=0;i<4;i++)
					SendBuff[12-i] = Cmd[i+2];
				for(i=13;i<17;i++)
					SendBuff[i]=~SendBuff[i-4];
				for(i=17;i<21;i++)
					SendBuff[i]=SendBuff[i-8];
				SendBuff[21]=(Sector*4+Block);
				SendBuff[22]=~(Sector*4+Block);
				SendBuff[23]=(Sector*4+Block);
				SendBuff[24]=~(Sector*4+Block);

                CrcCreate(SendBuff[4],0,CrcDat);
				for(i=0;i<20;i++)
					CrcCreate(SendBuff[5+i],1,CrcDat);

				SendBuff[25] = CrcDat[1];
				SendBuff[26] = CrcDat[0];

				Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
				TRACE("REP=%X",Resp[2]);
				if(Resp[4]!=0x66){
						strcpy(RespBuff,"Write 4B..............OK:");
			//			m_bTestMode = true; //enter test mode
				}
				else
				{
						strcpy(RespBuff,"Write 4B............Error");
						Error_sum+=1;
				}
	    	m_lstFromMcu.AddString(RespBuff);
			RespBuff[0] = 0;
			strcpy(RespBuff,"RESP:"); 
	   		for(i=3;i<(Resp[2]+3);i++)
			{//output fromat
	    		sprintf(tt,"%02x",Resp[i]);
	    		strcat(RespBuff,tt);
				strcat(RespBuff," ");
			}
			m_lstFromMcu.AddString(RespBuff);
		}


		break;
	case 0x0e: //test write block
			if(m_EquSel == RF35)
			{
				for(Sector = 1; Sector < 128; Sector++)
				{
					for(Block = 0; Block < 3; Block++)
					{
						SendBuff[0] = 0x00;
						SendBuff[1] = 0x00;
						SendBuff[2] = 0x00;
						SendBuff[3] = 0x14;
						SendBuff[4] = 0x80;
						SendBuff[5] = 0xa0;
						SendBuff[6] = Sector;
						SendBuff[7] = Block;
						for(i=0;i<16;i++)
						SendBuff[8+i] = Cmd[2];
						st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
					
						SendBuff[0] = 0x00;
						SendBuff[1] = 0x00;
						SendBuff[2] = 0x00;
						SendBuff[3] = 0x04;
						SendBuff[4] = 0x80;
						SendBuff[5] = 0x30;
						SendBuff[6] = Sector;
						SendBuff[7] = Block;
						st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
						for(i=0;i<16;i++)
						{
							if(Resp[i] != Cmd[2])
							{
								RespBuff[0] = 0;
								strcpy(RespBuff,"Error Add: Sector ");
								sprintf(tt,"%02x",Sector);
	    						strcat(RespBuff,tt);
								strcat(RespBuff,"Block ");
								sprintf(tt,"%02x",Block);
								strcat(RespBuff,tt);
								m_lstFromMcu.AddString(RespBuff);
								return;
							}
						}
					}
				}
				RespBuff[0] = 0;
				strcpy(RespBuff,"Write whole block ok!");
				m_lstFromMcu.AddString(RespBuff);
			}
			else if(m_EquSel == RF100)
			{
				for(Sector = 1; Sector < 128; Sector++)
				{
					for(Block = 0; Block < 3; Block++)
					{
						DataLength	= 23;
						SendBuff[0] = 0x48;
						SendBuff[1] = 0x00;     
						SendBuff[2] = DataLength+1;
						SendBuff[3] = 0x48;
						SendBuff[4] = 0x02;
						SendBuff[5] = 0x80;
						SendBuff[6] = 0xa0;
						SendBuff[7] = Sector;
						SendBuff[8] = Block;
						for(i=0;i<16;i++)
						SendBuff[9+i] = Cmd[2];

						CrcCreate(SendBuff[4],0,CrcDat);
						for(i = 0; i < 21; i++) 
							CrcCreate(SendBuff[i+5],1,CrcDat);
						SendBuff[25] = CrcDat[1];
						SendBuff[26] = CrcDat[0];
						Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);

						DataLength	= 7;
						SendBuff[0] = 0x48;
						SendBuff[1] = 0x00;     
						SendBuff[2] = DataLength+1;
						SendBuff[3] = 0x48;
						SendBuff[4] = 0x02;
						SendBuff[5] = 0x80;
						SendBuff[6] = 0x30;
						SendBuff[7] = Sector;
						SendBuff[8] = Block;
						CrcCreate(SendBuff[4],0,CrcDat);
						for(i = 0; i < 4; i++) 
							CrcCreate(SendBuff[i+5],1,CrcDat);
						SendBuff[9] = CrcDat[1];
						SendBuff[10] = CrcDat[0];  
						Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
						for(i=4;i<20;i++)
						{
							if(Resp[i] != Cmd[2])
							{
								RespBuff[0] = 0;
								strcpy(RespBuff,"Error Add: Sector ");
								sprintf(tt,"%02x",Sector);
	    						strcat(RespBuff,tt);
								strcat(RespBuff,"Block ");
								sprintf(tt,"%02x",Block);
								strcat(RespBuff,tt);
								m_lstFromMcu.AddString(RespBuff);
								return;
							}
						}
					}
				}
				RespBuff[0] = 0;
				strcpy(RespBuff,"Write whole block ok!");
				m_lstFromMcu.AddString(RespBuff);
			}
		break;
	case 0x0f://test write byte
			
			if(m_EquSel == RF35)
			{			
				for(Addr = 0; Addr < 0x2000; Addr++)
				{
					SendBuff[0] = 0x00;
					SendBuff[1] = 0x00;
					SendBuff[2] = 0x00;
					SendBuff[3] = 0x05;
					SendBuff[4] = 0x80;
					SendBuff[5] = 0xa1;
					SendBuff[6] = Addr>>8;
					SendBuff[7] = (unsigned char)Addr;
					SendBuff[8] = Cmd[2];
					st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);

					SendBuff[0] = 0x00;
					SendBuff[1] = 0x00;
					SendBuff[2] = 0x00;
					SendBuff[3] = 0x04;
					SendBuff[4] = 0x80;
					SendBuff[5] = 0x31;
					SendBuff[6] = Addr>>8;
					SendBuff[7] = (unsigned char)Addr;
					st=rf_pro_trn(icdev,SendBuff,(unsigned char* )Resp);
					if(Resp[0] != Cmd[2])
					{
							RespBuff[0] = 0;
							strcpy(RespBuff,"Error Addr:");
							sprintf(tt,"%02x",Addr);
	    					strcat(RespBuff,tt);
							m_lstFromMcu.AddString(RespBuff);
							return;
					}
				}
				RespBuff[0] = 0;
				strcpy(RespBuff,"Write whole byte ok!");
				m_lstFromMcu.AddString(RespBuff);
			}
			else if(m_EquSel == RF100)
			{
				for(Addr = 0; Addr < 0x2000; Addr++)
				{

					DataLength	= 8;
					SendBuff[0] = 0x48;
					SendBuff[1] = 0x00;     
					SendBuff[2] = DataLength+1;
					SendBuff[3] = 0x48;
					SendBuff[4] = 0x02;
					SendBuff[5] = 0x80;
					SendBuff[6] = 0xa1;
					SendBuff[7] = Addr>>8;
					SendBuff[8] = (unsigned char)Addr;
					SendBuff[9] = Cmd[2];
					CrcCreate(SendBuff[4],0,CrcDat);
					for(i = 0; i < 5; i++) 
						CrcCreate(SendBuff[i+5],1,CrcDat);
					SendBuff[10] = CrcDat[1];
					SendBuff[11] = CrcDat[0];
					Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);

					DataLength	= 7;
					SendBuff[0] = 0x48;
					SendBuff[1] = 0x00;     
					SendBuff[2] = DataLength+1;
					SendBuff[3] = 0x48;
					SendBuff[4] = 0x02;
					SendBuff[5] = 0x80;
					SendBuff[6] = 0x31;
					SendBuff[7] = Addr>>8;
					SendBuff[8] = (unsigned char)Addr;
					CrcCreate(SendBuff[4],0,CrcDat);
					for(i = 0; i < 4; i++) 
						CrcCreate(SendBuff[i+5],1,CrcDat);
					SendBuff[9] = CrcDat[1];
					SendBuff[10] = CrcDat[0];
					Result = protocol(CommPort,(DataLength+4), SendBuff,Resp);
					if(Resp[4] != Cmd[2])
					{
						RespBuff[0] = 0;
						strcpy(RespBuff,"Error Addr:");
						sprintf(tt,"%02x",Addr);
	    				strcat(RespBuff,tt);
						m_lstFromMcu.AddString(RespBuff);
						return;
					}
				}
				RespBuff[0] = 0;
				strcpy(RespBuff,"Write whole byte ok!");
				m_lstFromMcu.AddString(RespBuff);
			}
		break;
	case 0x40://test mode
       
		SendBuff[0] = 0x40;
		SendBuff[1] = 0x00;     
		SendBuff[2] = 0x01;
		SendBuff[3] = 0x40;
		Result = protocol(CommPort,4, SendBuff,Resp);
		TRACE("REP=%X",Resp[2]);
		if(Resp[2]==1&&Resp[3]==0x0a){
				strcpy(RespBuff,"Cmd40.........................OK:");
				m_bTestMode = true; //enter test mode
		}
		else
		{
                strcpy(RespBuff,"Cmd40.........................Error");
				Error_sum+=1;
		}
	    	m_lstFromMcu.AddString(RespBuff);
		break;
	case 0x43://
        SendBuff[0] = 0x43;
		SendBuff[1] = 0x00;     
		SendBuff[2] = 0x01;
		SendBuff[3] = 0x43;
		Result = protocol(CommPort,4, SendBuff,Resp);
	    if(Resp[2]==1&&Resp[3]==0x0a)
				strcpy(RespBuff,"Cmd43.........................OK:");
		else
		{
                strcpy(RespBuff,"Cmd43.........................Error");
				Error_sum+=1;
		}
	    	m_lstFromMcu.AddString(RespBuff);
		break;
	case 0x48://Assert_Error
 
		if(bbJudgError==1){
				strcpy(RespBuff,"Assert_Error..................OK:");
		}
		else
		{
                strcpy(RespBuff,"Assert_Error..................Error");
				Assert_sum+=1;
			if(m_StopAuthen2)
				CDialog::KillTimer(m_nTimerID);
		}
	    	m_lstFromMcu.AddString(RespBuff);
		break;	
	case 0x49://Assert_OK
 
		if(bbJudgError==0){
				strcpy(RespBuff,"Assert_OK.....................OK:");

		}
		else
		{
                strcpy(RespBuff,"Assert_OK.....................Error");
				Assert_sum+=1;
			if(m_StopAuthen2)
				CDialog::KillTimer(m_nTimerID);
		}
	    	m_lstFromMcu.AddString(RespBuff);
		break;		
	default:
		break;
	}   
	//display result

	if(bJudgError<Error_sum)
	{
		bbJudgError =1;
				if(m_StopAuthen)
				CDialog::KillTimer(m_nTimerID);
	}
	else
		bbJudgError =0;	
}
*/

unsigned char b1(unsigned char a, unsigned char b, unsigned char c, unsigned char d);
unsigned char b2(unsigned char a, unsigned char b, unsigned char c, unsigned char d);
unsigned char b3(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned char e);


unsigned char crypt(unsigned char inbyte, int status, int shiftnumber)
{
  // status = 0 , the inbyte is the valid data and the switch_A is open 
  // status = 1 , the inbyte is the valid data and the switch_A is close
  // status = 2 , the inbyte is not valid data to shift in the 48lfsr and switch_A is close
  // the shiftnumber is shift number
  static unsigned char x[48];
  int i,j; 
   
  unsigned char fd[16];
  unsigned char fd0;
  unsigned char net146, net141, net52, net57, net61, keyout;
  unsigned char temp;
 
  for (i=0; i<shiftnumber; i++)
   {
       fd[0]= (x[47] != x[42]);
       fd[1]= (fd[0] != x[38]);
       fd[2]= (fd[1] != x[37]);
       fd[3]= (fd[2] != x[35]);
       fd[4]= (fd[3] != x[33]);
       fd[5]= (fd[4] != x[32]);
       fd[6]= (fd[5] != x[30]);
       fd[7]= (fd[6] != x[28]);	
       fd[8]= (fd[7] != x[23]);	
       fd[9]= (fd[8] != x[22]);	
       fd[10]=(fd[9] != x[20]);	
       fd[11]=(fd[10]!= x[18]);	
       fd[12]=(fd[11]!= x[12]);	
       fd[13]=(fd[12]!= x[8]);	
       fd[14]=(fd[13]!= x[6]);	
       fd[15]=(fd[14]!= x[5]);	
       fd[16]=(fd[15]!= x[4]);	
 
      temp = inbyte << (7-i);
      temp = temp >> 7;
       
       if (status == 0)       
        fd0 = temp;
       else if (status == 1)
        fd0 = (fd[16] != temp);
       else
        fd0 = fd[16];
        
      for(j=47; j>0; j--)
      {
         x[j] = x[j-1];
      } 
       
     x[0] = fd0;
   }

	net146=b2( x[38], x[36], x[34], x[32]);
	net141=b1(x[30], x[28], x[26], x[24]);
	net52 =b1(x[22], x[20], x[18], x[16]);
	net57= b2( x[14], x[12], x[10], x[8]);
	net61= b1(x[6], x[4], x[2], x[0]);	
	keyout = b3( net146, net141, net52, net57, net61);	
	return keyout;
}
unsigned char b1(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
	unsigned char net11, net13, net9;
	unsigned char net28, net24, net32, net22;
	unsigned char net16, net19;
	net13 = !a;
	net11 = !b;
	net9  = !c;
	
	
	net28 = !(net11 && net13 && c);
	net24 = !(b && net9 && d);
	net32 = !(net9 && a && d);
	net22 = !(a && b);
	
	net16 = !(net28 && net24);
	net19 = !(net32 && net22);
	
	return !(net16 || net19);
	
}
unsigned char b2(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
	unsigned char net18, net16, net14, net12;
	unsigned char net28, net24, net32, net36, net40;
	unsigned char net45, net20;
	net18 = !a;
	net16 = !b;
	net14 = !c;
	net12 = !d;
	
	net28 = !(net16 && d && c);
	net24 = !(a && b && c);
	net32 = !(a && c && d);
	net36 = !(b && net14 && net18);
	net40 = !(a && net12 && net14);
	
	net45 = !(net28 && net24);
	net20 = !(net32 && net36 && net40);
	
	return !(net45 || net20);
	
}
unsigned char b3(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned char e)
{
	unsigned char net12, net18,net16,net10, net14;
	unsigned char net24, net33, net37, net41, net45, net49;
	unsigned char net25, net29;
	net12 = !a;
	net18 = !b;
	net16 = !c;
	net10 = !d;	
	net14 = !e;
	
	net24 = !(net12 && net14 && net18 && d);
	net33 = !(net12 && net16 && d);
	net37 = !(net10 && c && b);
	
	net41 = !(a && d && e);
	net45 = !(a && e && c);
	net49 = !(a && b && net10);
	
	net29 = !(net41 && net45 && net49);
	net25 = !(net24 && net33 && net37);
	
	return !(net25 || net29);
}


unsigned char random16(unsigned char initlow, unsigned char inithigh,int bitnumber, int status)
{
// status = 0, initial the random shifter
// status = 1, generate bitnumber random bit.
   unsigned char rangen1, rangen2,rangen3,temp;
   static unsigned char x[16];
   int i,j;
   unsigned short IniVal;
   unsigned char outbyte;
   bool r[15];
   
   IniVal = ((inithigh&0x00ff)<<8) | initlow;

   outbyte = 0;   
   if (status == 0)
   {
   	for(i=0;i<=15;i++)
		r[i] = ((IniVal>>i) & 0x01)?true:false;
	x[0] = r[1]^r[2]^r[4]^r[15];
	x[1] = r[0]^r[1]^r[3]^r[14];
    x[2] = r[0]^r[1]^r[4]^r[13]^r[15];
	x[3] = r[0]^r[1]^r[2]^r[3]^r[4]^r[12]^r[14]^r[15];
    x[4] = r[0]^r[3]^r[4]^r[11]^r[13]^r[14]^r[15];
	x[5] = r[1]^r[3]^r[4]^r[10]^r[12]^r[13]^r[14]^r[15];
    x[6] = r[0]^r[2]^r[3]^r[9]^r[11]^r[12]^r[13]^r[14];
	x[7] = r[4]^r[8]^r[10]^r[11]^r[12]^r[13]^r[15];
	x[8] = r[3]^r[7]^r[9]^r[10]^r[11]^r[12]^r[14];
    x[9] = r[2]^r[6]^r[8]^r[9]^r[10]^r[11]^r[13];
    x[10] = r[1]^r[5]^r[7]^r[8]^r[9]^r[10]^r[12];
    x[11] = r[0]^r[4]^r[6]^r[7]^r[8]^r[9]^r[11];
    x[12] = r[1]^r[2]^r[3]^r[4]^r[5]^r[6]^r[7]^r[8]^r[10]^r[15];
	x[13] = r[0]^r[1]^r[2]^r[3]^r[4]^r[5]^r[6]^r[7]^r[9]^r[14];
    x[14] = r[0]^r[3]^r[5]^r[6]^r[8]^r[13]^r[15];
	x[15] = r[0];

   }
   else {
   	  for (j=0; j<bitnumber; j++)
   	  {
       temp = x[15];
       temp = temp << 7;
       outbyte = outbyte >> 1  ;
       outbyte = outbyte + temp;
	   rangen1 = (x[14] != x[12]);
	   rangen2 = (rangen1 != x[11]);
	   rangen3 = (rangen2 != x[9]);
	   for (i=14; i>0; i--)
	   	   x[i]= x[i-1];
	   x[0] = x[15];
	   x[15] = rangen3;
   	  }
   	}
  // TRACE("randm=%x\n",outbyte);
   return outbyte;
}



int CrcCreate(unsigned char outbyte, int status, unsigned char *crcdata)
{
  // status = 0 , this byte is the first byte;
  // status = 1, this byte is the continus byte;
 // crcdata[1] store the first byte crc result, and should transfer
 // the least sign bit first. then the crcdata[0].
  unsigned char inidata,temp;
  static unsigned char r[16];
  int i,j;
  unsigned char xr0,xr5,xr12; 
 if(status == 0)
 {
  inidata = 0x63;
  for (i=0; i<8;i++)
   {
     temp = inidata << (7-i);
     temp = temp >> 7;
     r[i] = temp;
     r[i+8]= temp;
   }
   
  }  
  // initial the 16 bit shifter register.
 for (i=0; i<8; i++)
 {
    temp = outbyte << (7-i);
    temp = temp >> 7;
       // temp is the input bit.
    xr0 = (r[0] + temp ) % 2;
    xr5 = (r[11] + xr0 ) % 2;
    xr12 = (r[4] + xr0 ) % 2;

    for(j=0; j<15;j++)
     r[j] = r[j+1];

    r[15] = xr0;
    r[10] = xr5;
    r[3]  = xr12; 
 }
 for(i=7; i>=0; i--)
  {
    crcdata[1] = crcdata[1] << 1;
    crcdata[1]= crcdata[1] + r[i];
    crcdata[0] = crcdata[0] << 1;
    crcdata[0]= crcdata[0] + r[8+i];
  }
 return 0;
}
unsigned short outputcryptbyte(unsigned char InDat, int bitnumber,int cryptstatus)
{
// cryptstatus = 0 for RA
// cryptstatus = 1 for other
  
  unsigned short EncryptDat ;
  unsigned char curbit,i,temp=0,temp1=0,temp2=0;
  static unsigned char lastbit;
  unsigned char paritybit, cryptbit;
  paritybit = 1;

  cryptbit = crypt(0, 0, 0);
//  TRACE("indat=%x",InDat);
  for (i=0; i<bitnumber; i++)
  {
    cryptbit = crypt(0, 0, 0);
    curbit = InDat <<(7- i);
    curbit = curbit >> 7;
//	TRACE("InBit=%x",curbit);
    paritybit = (paritybit+curbit) % 2;
    if(cryptstatus == 1)
        crypt(curbit, 1, 1);
    else 
		crypt(curbit, 2, 1);
    curbit = (curbit != cryptbit);
//	TRACE("Enbit=%x",curbit);
	if(curbit==0x01)
		temp|= 0x01;
	else
        temp &= 0x0fe;
	if(i<(bitnumber-1))
        temp<<= 1;
	
  }
 // TRACE("\n");
 // TRACE("temp=%x",temp);
  if(bitnumber!=8)
	  temp<<=(8-bitnumber);
// TRACE("temp=%x",temp);
  for(i=0;i<8;i++)
  {
	  if(temp&0x01)
		  temp1 |=0x01;
	  else
		  temp1 &= 0x00fe;
	  temp>>=1;
	
      if(i<7)
        temp1<<= 1;
//	  TRACE("endata=%x",EncryptDat);
  }
 //  TRACE("temp1=%x",temp1);
  if ((bitnumber == 8))//parity bit crypt
  {
    cryptbit = crypt(0, 0, 0);
    curbit = paritybit;
    curbit = (curbit != cryptbit);
	if(curbit == 0x01)
		temp2=0x01; //(EncryptDat<<8)|0x01;
	else
		temp2=0x00;
  }
  EncryptDat=(temp1<<8)|temp2;
 // TRACE("endata=%x",temp1);
  return EncryptDat;
}
unsigned char parabity(unsigned char dat)
{
   unsigned char Bit_1_num=0,i;
   for(i= 0;i<8;i++)
   {
     if(dat%2)
		 Bit_1_num++;
	 dat=dat>>1;
   }
   if(Bit_1_num%2)
	     return 0x00;
	 else
		 return 0x01;

 
}

//send and receive data via COMM port
/*short protocol(HANDLE icdev,int len, unsigned char *send_cmd, unsigned char *receive_data)
{
    unsigned char i,TempBuffer[100];
	BOOL bResult;
	for(i=0;i<len;i++)
		TempBuffer[i] = send_cmd[i];
//	for(i=0;i<4;i++)
//		TRACE("dd=%x",send_cmd[i]);
	bResult=WriteCommPort(TempBuffer,len);
	if(!bResult)
		return -1;
	else
	{
		bResult = ReadCommPort(receive_data, 3);		
		if(!bResult)
	         return -1;
		else{
			if(receive_data[2]!=0x00){
			    bResult = ReadCommPort(receive_data+3,receive_data[2]);
           	    if(!bResult)
				   return -1;
			    else 
				   return 1;
			}
			else 
				return 1;
		}
	}
}
*/
short protocol(HANDLE icdev,int len, unsigned char *send_cmd, unsigned char *receive_data)
{
    unsigned char i,TempBuffer[100];
	CString		strLog,temp_str;
	BOOL bResult;
	for(i=0;i<len;i++)
		TempBuffer[i] = send_cmd[i];
	bResult=WriteCommPort(TempBuffer,len);
	if(!bResult)
	{
		return -1;
	}
	else
	{

		bResult = ReadCommPort(receive_data, 3);		
		if(!bResult)
	         return -1;
		else{
			if(receive_data[2]!=0x00)
			{
			    bResult = ReadCommPort(receive_data+3,receive_data[2]);
           	    if(!bResult)
				   return -1;
			}
			else 
			{
				return 1;
			}
		}
	}
	return 1;
}


BOOL ReadCommPort( unsigned char *buffer, unsigned int length)
{
	DWORD   n_received;
	if(!ReadFile(CommPort, buffer, length, &n_received, NULL)||n_received!=length) 
	   	return FALSE;
	return TRUE;
}
/*-------------------------------------------------------------------------
 * Transmit block of data
 *-------------------------------------------------------------------------*/
BOOL WriteCommPort(unsigned char *buffer, unsigned char length)
{
	DWORD dwBytesWritten;
		
	if (!WriteFile(CommPort, buffer, length, &dwBytesWritten, NULL) || (dwBytesWritten != length)) {
		return FALSE;
	}
	return TRUE;   
   
}
BOOL OpenComm(int CommNum,unsigned long BaudRate)
{  
	char *PortName;
    char *com[]={" ", "COM1", "COM2"};
    BOOL bOpenResult = TRUE;
	PortName = com[CommNum];
	COMMTIMEOUTS  CommTimeOuts;
    DCB				dcb ;
    printf("Opening com port \"%s\" at %ld baudrate\n",PortName,BaudRate);
    
	switch(BaudRate)
    {
        case 1200   :   BaudRate = CBR_1200     ;    break; 
        case 2400   :   BaudRate = CBR_2400     ;    break;            
        case 4800   :   BaudRate = CBR_4800     ;    break;
        case 9600   :   BaudRate = CBR_9600     ;    break;
        case 14400  :   BaudRate = CBR_14400    ;    break;
        case 19200  :   BaudRate = CBR_19200    ;    break;
        case 38400  :   BaudRate = CBR_38400    ;    break;
        case 57600  :   BaudRate = CBR_57600    ;    break;
        case 115200 :   BaudRate = CBR_115200   ;    break;
        default     :   BaudRate = CBR_57600     ; 
                        printf(" Baud rate not supported!!\n Opening com port at %ld baudrate\n",BaudRate);
                        break;
    }
	CommPort = CreateFile(PortName, GENERIC_READ | GENERIC_WRITE,
		0,                    
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, 
	    NULL);
	
	if (INVALID_HANDLE_VALUE == CommPort){
		printf(("Open %s... failed\n", PortName));
	    return FALSE;	
	}
     // Set the size of the input and output buffer.
	if (!SetupComm( CommPort, 4096,4096  )) {
		printf("SetupComm failed\n");
		return FALSE;
	}

	// purge any information in the buffer
	if (!PurgeComm( CommPort, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR)) {
		printf(" PurgeComm failed\n");
		return FALSE;
	}

	// set the time-out parameters for all read and write operations

	CommTimeOuts.ReadIntervalTimeout = 20 ;
	CommTimeOuts.ReadTotalTimeoutMultiplier =100 ;
	CommTimeOuts.ReadTotalTimeoutConstant =300 ;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0 ;
	CommTimeOuts.WriteTotalTimeoutConstant = 500 ;	

	if (!SetCommTimeouts( CommPort, &CommTimeOuts )) {
		printf("SetCommTimeouts failed\n");
		return FALSE;
	}
	
	dcb.DCBlength = sizeof( DCB ) ;

	if (!GetCommState(CommPort, &dcb)) {
		printf(" GetCommState failed\n");
		return FALSE;
	}

	dcb.BaudRate = BaudRate;
	dcb.Parity = FALSE ;
	dcb.fBinary = TRUE ;
	dcb.Parity = NOPARITY ;
	dcb.ByteSize = 8 ;
	dcb.StopBits = ONESTOPBIT ;
// Flow Control Settings
	dcb.fRtsControl = RTS_CONTROL_DISABLE;//RTS_CONTROL_HANDSHAKE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;//DTR_CONTROL_HANDSHAKE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fAbortOnError = FALSE;

	if (!SetCommState(CommPort, &dcb)) {
		printf(" SetCommState failed\n");
		return FALSE;
	}
	

	return TRUE;
}


void CRfCardDlg::OnButton3() 
{
// TODO: Add your control notification handler code here
	// TODO: Add your control notification handler code here
	static char BASED_CODE szFilter[] = "Text Files (*.txt) |*.txt|";
	CFileDialog cfgOpen(TRUE, NULL, NULL,  OFN_HIDEREADONLY | OFN_FILEMUSTEXIST , (LPCTSTR)szFilter, NULL);
			bool bResult = false;
			CString strContent,str_head;
			unsigned int  str_headnum=0;
			char TempStr[100];
			char *str_token;
			unsigned int num_token=0;
			char seps[]   = "|";
	if (cfgOpen.DoModal() != IDOK) return;
//	if (cfgOpen.DoModal())
	else
	{
		CString fileName = cfgOpen.GetFileName();
		if (fileName != "")
		{
			//m_lstCmd.ResetContent();
			
			HANDLE hFile = CreateFile(fileName.GetBuffer(fileName.GetLength()),
				GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,NULL);
			if (hFile != NULL)
			{
				OVERLAPPED ov;
				memset(&ov,0,sizeof(ov));
				for ( ; ; )
				{
					DWORD dwRead;
					char c;
					if (ReadFile(hFile,&c,1,&dwRead,&ov) != 0)
					{
						//有数据
						if (dwRead > 0)	
						{
							if ((c != '\n') && (c != '\r'))
							{
								strContent += c;
							}
							else
							{
								if (strContent.GetLength() != 0)
								{
								//	m_lstCmd.AddString(strContent);
									str_head=strContent.Mid(0,3); 
									if(str_head=="DTR")
									{
										str_ID[str_headnum]=strContent.Mid(13,8);
									//	m_lstCmd.AddString(str_ID[str_headnum]);
									}
									else if(str_head=="PRR")
									{

										strcpy(TempStr,(LPCTSTR)strContent);
									
										str_token = strtok(TempStr, seps);
										 while( str_token != NULL )
										 {
											 num_token++;
											str_token = strtok(NULL, seps);
											if(num_token==7)
											{
												str_x[str_headnum]=str_token;
											//		m_lstCmd.AddString(str_x[str_headnum]);
											}
												else if(num_token==8)
											{	
												str_y[str_headnum]=str_token;
											//		m_lstCmd.AddString(str_y[str_headnum]);
												break;
											}
										 }
										num_token=0;
										str_headnum++;
									}
								strContent.Empty();
								}
							}
						}
					}
					else		//read error
					{
						if (GetLastError() == ERROR_HANDLE_EOF)	
						{
							if (strContent.GetLength() != 0)
							{
								//m_lstCmd.AddString(strContent);
								strContent.Empty();
							}
							bResult = true;			
						}
						Num_strtotal=str_headnum;
						break;
					}
					ov.Offset += 1;
				}// end for read file
				if (!bResult)
				{
					MessageBox("Read File Error","Error",MB_OK);
				}
				CloseHandle(hFile);
			}
			fileName.ReleaseBuffer();
		}
	}

}

void CRfCardDlg::OnButton4() 
{
	// TODO: Add your control notification handler code here

	
//	str_x[0]="-30";str_y[0]="-80";
//	char Temp_head[10],*Temp_sx,*Temp_sy;
	int		coord_x,coord_y;
	bool	user_id_ok=0;
	CString str_h;
	user_ID= "";
	str_h= "";
	for(int k=0;k<4;k++)
	{
	//	str_h.Format("%s",user_IDdata[k]);
		CString temp;
		temp.Format("%02x",user_IDdata[k]);
		user_ID+=temp;
	}
		m_IDdata=user_ID;
//	coord_x = atoi( str_x[0] );
//	coord_y = atoi( str_y[0] );
	for(unsigned int Num_v=0;Num_v<Num_strtotal;Num_v++)
	{
		str_ID[Num_v].Remove(' ');
		str_ID[Num_v].MakeLower();
		str_ID[Num_v].TrimLeft();
		str_ID[Num_v].TrimRight();
		user_ID.Remove(' ');
		user_ID.MakeLower();
		user_ID.TrimLeft();
		user_ID.TrimRight();
		if(user_ID==str_ID[Num_v])
		{
		coord_x = atoi( str_x[Num_v] );
		coord_y = atoi( str_y[Num_v] );
		coord_x = coord_x%3-2;
		coord_y = coord_y%3+1;
			if((coord_x>=-2)&&(coord_y<=2)&&(coord_y>=0))
				m_version= coord_y*3+1+abs(coord_x);
			else if((coord_x>=-4)&&(coord_x<=-3)&&(coord_y<=2)&&(coord_y>=0))
				m_version= coord_y*3+1+abs(coord_x)-3;
			else if((coord_x>=-4)&&(coord_x<=-3)&&(coord_y==-1))
				m_version= 7+abs(coord_x)-3;
			else if((coord_x>=-2)&&(coord_y==-1))
				m_version= 7+abs(coord_x);
			else if((coord_x>=-4)&&(coord_x<=-3)&&(coord_y==3))
				m_version= 1+abs(coord_x)-3;
			else if((coord_x>=-2)&&(coord_y==3))
				m_version= 1+abs(coord_x);

		break;
		}
	
	}

    UpdateData(FALSE);
	
}

BOOL CRfCardDlg::CheckCommand(CString strCmdLine)
{
	if (strCmdLine.IsEmpty() == TRUE)
		return FALSE;
	if (strCmdLine.Mid(0, 2) == "//")
		return FALSE;
	if (strCmdLine.Mid(0, 2) == "\\\\")
		return FALSE;

	return TRUE;
}

BOOL CRfCardDlg::HandleCommand(CCommand *pCommand)
{
	if ((pCommand->uCmdType == 0) && (pCommand->uCmdId == 0xff)) //special for compare cmd
	{
		int nIndex = g_nResponseTimes - 1;
		if (nIndex < 0)
			nIndex = 0;
		if (pCommand->uDataLen != g_data[nIndex].nDataLen)
		{
			if(m_StopAuthen)
			{
				CDialog::KillTimer(m_nTimerID);
				SetDlgItemText(IDC_RUN, "Run");
				m_bRunning = FALSE;
			}
			m_lstFromMcu.AddString("...Error... \t comparison result is wrong");
			return FALSE;
		}
		for (int i = 0; i < pCommand->uDataLen; i++)
		{
			if (pCommand->pData[i] != g_data[nIndex].data[i])
			{
				if(m_StopAuthen)
				{
					CDialog::KillTimer(m_nTimerID);
					SetDlgItemText(IDC_RUN, "Run");
					m_bRunning = FALSE;
				}
				m_lstFromMcu.AddString("...Error... \t comparison result is wrong");
				return FALSE;
			}
		}

		m_lstFromMcu.AddString("...Ok... \t comparison result is ok");
		return TRUE;
	}
/*
	//send command
	if (g_pollThread == NULL)
	{
		ClearResponseData();
		g_bExit = FALSE;
		g_pollThread = AfxBeginThread(pollingThread, this, THREAD_PRIORITY_NORMAL, 0, 0, NULL); 
	}
	*/
	//AT command
	CString strKey = KEY_ATCMD;
	BOOL bResult = FALSE;
	if (pCommand->strCmdName.Find(strKey) == 0)
	{
		bResult = SendATCommand(g_port, pCommand);
	}else
	{
		bResult = SendCommand(g_port, pCommand);
	}
	
	if (FALSE == bResult)
	{
		m_lstFromMcu.AddString("Failed to write port!");
		return FALSE;
	}
	/*
	//end polling thread
	g_bExit = TRUE;
	g_pollThread = NULL;
	WaitForSingleObject(threadEndEvent, 5000);
	threadEndEvent.ResetEvent();
	if (g_data[g_nResponseTimes-1].responseId != 0)
	{
		if(m_StopAuthen)
		{
			CDialog::KillTimer(m_nTimerID);
			SetDlgItemText(IDC_RUN, "Run");
			m_bRunning = FALSE;
		}
	}
	*/
	//update GUI
	//UpdateGUI();
	return TRUE;
}

CCommand* CRfCardDlg::TranslateCmd(CString strCommand)
{
	CCommand* pCmd = NULL;
	BOOL bFindMatch = FALSE;
	BOOL bIsAtCmd = FALSE;
	strCommand.TrimLeft();
	strCommand.TrimRight();
	strCommand.Remove(' ');
	if (strCommand.IsEmpty() == TRUE)
		return pCmd;
	int nIndex = strCommand.Find(',');
	if (nIndex < 0)
		nIndex = strCommand.GetLength();

	//common command
	CString strKey = KEY_COMMONCMD;
	if (strCommand.Find(strKey) == 0)
	{
		//cmd name
		CString strCmdName = strCommand.Mid(0, nIndex);
		pCmd = m_pCommonCmds->GetHead ();
		POSITION pos = m_pCommonCmds->GetHeadPosition ();
		while(NULL != pos)
		{
			pCmd = m_pCommonCmds->GetNext (pos);
			if(pCmd != NULL)
			{
				if (strCmdName.CompareNoCase(pCmd->strCmdName) == 0)
				{
					bFindMatch = TRUE;
					break;
				}
			}
		}
	}
	//IIC command
	strKey = KEY_IICCMD;
	if (strCommand.Find(strKey) == 0)
	{
		//cmd name
		CString strCmdName = strCommand.Mid(0, nIndex);
		pCmd = m_pIICCmds->GetHead ();
		POSITION pos = m_pIICCmds->GetHeadPosition ();
		while(NULL != pos)
		{
			pCmd = m_pIICCmds->GetNext (pos);
			if(pCmd != NULL)
			{
				if (strCmdName.CompareNoCase(pCmd->strCmdName) == 0)
				{
					bFindMatch = TRUE;
					break;
				}
			}
		}
	}
	//SPI command
	strKey = KEY_SPICMD;
	if (strCommand.Find(strKey) == 0)
	{
		//cmd name
		CString strCmdName = strCommand.Mid(0, nIndex);
		pCmd = m_pSPICmds->GetHead ();
		POSITION pos = m_pSPICmds->GetHeadPosition ();
		while(NULL != pos)
		{
			pCmd = m_pSPICmds->GetNext (pos);
			if(pCmd != NULL)
			{
				if (strCmdName.CompareNoCase(pCmd->strCmdName) == 0)
				{
					bFindMatch = TRUE;
					break;
				}
			}
		}
	}
	//MW command
	strKey = KEY_MWCMD;
	if (strCommand.Find(strKey) == 0)
	{
		//cmd name
		CString strCmdName = strCommand.Mid(0, nIndex);
		pCmd = m_pMWCmds->GetHead ();
		POSITION pos = m_pMWCmds->GetHeadPosition ();
		while(NULL != pos)
		{
			pCmd = m_pMWCmds->GetNext (pos);
			if(pCmd != NULL)
			{
				if (strCmdName.CompareNoCase(pCmd->strCmdName) == 0)
				{
					bFindMatch = TRUE;
					break;
				}
			}
		}
	}

	//AT command
	strKey = KEY_ATCMD;
	if (strCommand.Find(strKey) == 0)
	{
		//cmd name
		CString strCmdName = strCommand.Mid(0, nIndex);
		pCmd = m_pATCmds->GetHead ();
		POSITION pos = m_pATCmds->GetHeadPosition ();
		while(NULL != pos)
		{
			pCmd = m_pATCmds->GetNext (pos);
			if(pCmd != NULL)
			{
				if (strCmdName.CompareNoCase(pCmd->strCmdName) == 0)
				{
					bFindMatch = bIsAtCmd = TRUE;
					break;
				}
			}
		}
	}

	if (bFindMatch == FALSE)
		return NULL;

	//cmd data
	CString strData;
	if (nIndex >= strCommand.GetLength())
		strData.Empty();
	else
		strData = strCommand.Mid(nIndex+1);
	int nLen = strData.GetLength();
	if(bIsAtCmd == FALSE)
	{
		if ((nLen%2) != 0)
			pCmd = NULL;
		nLen = nLen / 2;
		pCmd->uDataLen = nLen;
		pCmd->pData = new unsigned char[pCmd->uDataLen];
		int i = 0;
		while (i < pCmd->uDataLen)
		{
			char *stopstr;
			pCmd->pData[i++] = (unsigned char)strtoul(strData.Mid(i*2, 2), &stopstr, 16);
		}
	}else
	{
		if(strCommand.Find(NETWRITE_CMD) == 0)
		{
			int socketID = -1;
			int data_len = 0;
			char temp[20] = {0};
			sscanf(strData.GetBuffer(nLen), "=%d,%d", &socketID, &data_len);
			if(data_len == 0)
			{
				data_len = generateRandomArray(1,2048);
			}
			sprintf(temp, "=%d,%d",socketID, data_len);
			pCmd->uDataLen = strlen(temp);
			pCmd->pData = new unsigned char[pCmd->uDataLen];
			memset(pCmd->pData,0,pCmd->uDataLen);
			memcpy(pCmd->pData,(unsigned char *)temp,pCmd->uDataLen*sizeof(unsigned char));
		}else
		{
			pCmd->uDataLen = nLen;
			if(nLen > 0)
			{
				pCmd->pData = new unsigned char[pCmd->uDataLen];
				memcpy(pCmd->pData,(unsigned char *)strData.GetBuffer(nLen),nLen*sizeof(unsigned char));
			}
			//pCmd->pData = (unsigned char *)strData.GetBuffer(strData.GetLength());
		}
	}
	return pCmd;
}

#define ERROR_CHECKSUM     0x01
#define ERROR_PARAMETERS   0x02
#define ERROR_READCMP      0x03
#define ERROR_WRITEPOLLING 0x04
#define ERROR_WRITERESULT  0x05
#define ERROR_WRITECMD     0x06
#define ERROR_ERASE        0x07
#define ERROR_ERAL         0x08
//update GUI
void CRfCardDlg::UpdateGUI(void)
{
	CString strMessage;

	//two times of response is mandatory
	if ((g_nResponseTimes > 0) && (g_nResponseTimes < MAX_RESPONSE_TIMES))
	{
		strMessage += "...Error... \t no second response"; 
		m_lstFromMcu.AddString(strMessage);
		return;
	}
	if (g_nResponseTimes > MAX_RESPONSE_TIMES)
	{
		strMessage += "...Error... \t more than two times response"; 
		m_lstFromMcu.AddString(strMessage);
		return;
	}

	//show
	int nIndex = g_nResponseTimes - 1;
	if (nIndex < 0)
		nIndex = 0;

	if (g_data[nIndex].nDataLen != 0)
	{
		if (g_data[nIndex].data != NULL)
		{
			strMessage += " \t ";
			strMessage += g_data[nIndex].data;
		}
	}

	m_lstFromMcu.AddString(strMessage);
}

void CRfCardDlg::WaitForTermination()
{
	g_bPoll_mcu0 = TRUE;
	g_nPoolingTime = 0;
	WaitForSingleObject(endEvent_mcu0, 500);
	//g_bExit = TRUE;
	//g_bPoll_mcu0 = FALSE;
	endEvent_mcu0.ResetEvent();
}

void CRfCardDlg::OnStepandnext() 
{
//#ifdef	SUPPORT_2315_STS
//	OnStep();
//	if(m_lstCmd.GetCurSel()==(m_lstCmd.GetCount()-1))
//		m_lstCmd.SetCurSel(-1);
//	m_lstCmd.SetCurSel(m_lstCmd.GetCurSel()+1);//visible
//#else
	BeginWaitCursor();

	UpdateData(true);
	if(m_lstCmd.GetCurSel() == LB_ERR)	
		return;		//must have selection

	//get command
	CString strCmdLine;
	m_lstCmd.GetText(m_lstCmd.GetCurSel(),strCmdLine);
	strCmdLine.TrimLeft();
	strCmdLine.TrimRight();

	//check command
	BOOL bResult = CheckCommand(strCmdLine);
	if (FALSE == bResult)
	{
		m_lstFromMcu.AddString(strCmdLine);
		if(m_lstCmd.GetCurSel()==(m_lstCmd.GetCount()-1))
			m_lstCmd.SetCurSel(-1);
		m_lstCmd.SetCurSel(m_lstCmd.GetCurSel()+1);//visible	
		return;
	}

	//command translation
	CCommand *pCommand = TranslateCmd(strCmdLine);
	if (pCommand == NULL)
	{
		m_lstFromMcu.AddString("...Error... \t Command is invalid!");
		if(m_lstCmd.GetCurSel()==(m_lstCmd.GetCount()-1))
			m_lstCmd.SetCurSel(-1);
		m_lstCmd.SetCurSel(m_lstCmd.GetCurSel()+1);//visible	
		return;
	}

	//Handle Command
	bResult = HandleCommand(pCommand);
	if (FALSE == bResult)
		return;

	if(m_lstCmd.GetCurSel()==(m_lstCmd.GetCount()-1))
		m_lstCmd.SetCurSel(-1);
	m_lstCmd.SetCurSel(m_lstCmd.GetCurSel()+1);//visible	

	CString str_3;
	str_3.Format("%d",Error_sum);
	SetDlgItemText(IDC_ErrorSum,str_3);	
	str_3.Format("%d",Assert_sum);
	SetDlgItemText(IDC_ErrorSum2,str_3);

	EndWaitCursor();

//#endif
}


/*
void CRfCardDlg::OnTTT() 
{
	// TODO: Add your control notification handler code here
    unsigned long Snr;
	CString strvalue,strtmp;
	char redata[33]={0},wrdata[33]={0};

         unsigned char size;     
			unsigned __int16 TagType;
			st = rf_request(icdev,0,&TagType);
		st=rf_anticoll(icdev,0,&Snr);

        st=rf_select(icdev,Snr,&size);
		st=rf_authentication(icdev,0,0);
		st=rf_halt(icdev);
			st = rf_request(icdev,0,&TagType);

}
*/
void CRfCardDlg::AddMifareCommand()
{
    m_lstAllCmd.AddString("Reset");
    m_lstAllCmd.AddString("SetSector");
    m_lstAllCmd.AddString("SetBlock");	
    m_lstAllCmd.AddString("Request0");
	m_lstAllCmd.AddString("Request1");
	m_lstAllCmd.AddString("AntiColl");
    m_lstAllCmd.AddString("Select");
    m_lstAllCmd.AddString("SetKeyA");
	m_lstAllCmd.AddString("SetKeyB");	
	m_lstAllCmd.AddString("Authen");
    m_lstAllCmd.AddString("Read");
	m_lstAllCmd.AddString("Write16B");
    m_lstAllCmd.AddString("Write4B");
	m_lstAllCmd.AddString("Inc");
	m_lstAllCmd.AddString("Dec");
    m_lstAllCmd.AddString("Restore");
	m_lstAllCmd.AddString("Transfer");
	m_lstAllCmd.AddString("Compare4B");
	m_lstAllCmd.AddString("Compare16B");
	m_lstAllCmd.AddString("Halt0");
	m_lstAllCmd.AddString("Assert_Error");
	m_lstAllCmd.AddString("Assert_OK");
	m_lstAllCmd.AddString("TestMode");
	m_lstAllCmd.AddString("ExitTest");
	m_lstAllCmd.AddString("Note");

}

void CRfCardDlg::AddDemoCos()
{
    m_lstAllCmd.AddString("Reset");
    m_lstAllCmd.AddString("SetSector");
    m_lstAllCmd.AddString("SetBlock");	
    m_lstAllCmd.AddString("Request0");
	m_lstAllCmd.AddString("Request1");
	m_lstAllCmd.AddString("AntiColl");
    m_lstAllCmd.AddString("Select");
	m_lstAllCmd.AddString("DirectCmd");
	m_lstAllCmd.AddString("Note");
	m_lstAllCmd.AddString("Pro_RATS");	
	m_lstAllCmd.AddString("Pro_DESELECT");	
	m_lstAllCmd.AddString("Pro_WriteBlock");	
	m_lstAllCmd.AddString("Pro_ReadBlock");	
	m_lstAllCmd.AddString("Pro_WriteByte");	
	m_lstAllCmd.AddString("Pro_ReadByte");	
	m_lstAllCmd.AddString("Pro_WriteSecurity");	
	m_lstAllCmd.AddString("Pro_ReadSecurity");	
	m_lstAllCmd.AddString("Pro_Increase");	
	m_lstAllCmd.AddString("Pro_Decrease");	
	m_lstAllCmd.AddString("Pro_Restore");	
	m_lstAllCmd.AddString("Pro_Transfer");
	m_lstAllCmd.AddString("Pro_Write4B");
	m_lstAllCmd.AddString("Test_WriteBlock");
	m_lstAllCmd.AddString("Test_WriteByte");
}

void CRfCardDlg::Add2315STSCmd()
{
    m_lstAllCmd.AddString("Reset");
    m_lstAllCmd.AddString("Request0");
	m_lstAllCmd.AddString("Request1");
	m_lstAllCmd.AddString("AntiColl");
    m_lstAllCmd.AddString("Select");
	m_lstAllCmd.AddString("DirectCmd");
	m_lstAllCmd.AddString("Note");
	m_lstAllCmd.AddString("Pro_RATS");	
	m_lstAllCmd.AddString("Pro_DESELECT");	
	for(UINT i=0;i<m_nCommandNum;i++)
		m_lstAllCmd.AddString(m_pSTS_Command[i]->m_strCmdName);
}

void CRfCardDlg::OnSelchangeCombo1() 
{
	// TODO: Add your control notification handler code here
	UpdateData(true);

	m_lstAllCmd.ResetContent();

	if (m_strCombo == 0) //common command
	{
		if(m_pCommonCmds != NULL)
		{
			CCommand* pCmds = m_pCommonCmds->GetHead();
			POSITION pos = m_pCommonCmds->GetHeadPosition ();
			while (NULL != pos)
			{
				pCmds = m_pCommonCmds->GetNext (pos);
				if(pCmds != NULL)
					m_lstAllCmd.AddString(pCmds->strCmdName);
			}
		}
	}

	if (m_strCombo == 1) //IIC command
	{
		if(m_pIICCmds != NULL)
		{
			CCommand* pCmds = m_pIICCmds->GetHead();
			POSITION pos = m_pIICCmds->GetHeadPosition ();
			while (NULL != pos)
			{
				pCmds = m_pIICCmds->GetNext (pos);
				if(pCmds != NULL)
					m_lstAllCmd.AddString(pCmds->strCmdName);
			}
		}
	}

	if (m_strCombo == 2) //SPI command
	{
		if(m_pSPICmds != NULL)
		{
			CCommand* pCmds = m_pSPICmds->GetHead();
			POSITION pos = m_pSPICmds->GetHeadPosition ();
			while (NULL != pos)
			{
				pCmds = m_pSPICmds->GetNext (pos);
				if(pCmds != NULL)
					m_lstAllCmd.AddString(pCmds->strCmdName);
			}
		}
	}

	if (m_strCombo == 3) //MW command
	{
		if(m_pMWCmds != NULL)
		{
			CCommand* pCmds = m_pMWCmds->GetHead();
			POSITION pos = m_pMWCmds->GetHeadPosition ();
			while (NULL != pos)
			{
				pCmds = m_pMWCmds->GetNext (pos);
				if(pCmds != NULL)
					m_lstAllCmd.AddString(pCmds->strCmdName);
			}
		}
	}

	if (m_strCombo == 4) //AT command
	{
		if(m_pATCmds != NULL)
		{
			CCommand* pCmds = m_pATCmds->GetHead();
			POSITION pos = m_pATCmds->GetHeadPosition ();
			while (NULL != pos)
			{
				pCmds = m_pATCmds->GetNext (pos);
				if(pCmds != NULL)
					m_lstAllCmd.AddString(pCmds->strCmdName);
			}
		}
}
}

void CRfCardDlg::OnSelchangeListAllCmd() 
{
	UpdateData(true);
	CComboBox* p;
	p=(CComboBox*)GetDlgItem(IDC_COMBO1);
	CString str;	
	p->GetLBText(p->GetCurSel( ),str);
	if(str!="2315 STS Command")
		return;
	
//	GetDlgItem(IDC_STATIC_CMD_COMMENT)->SetWindowText("");
	GetDlgItem(IDC_INFO)->SetWindowText("");
	int index;
	index=m_lstAllCmd.GetCurSel();
	if(index != LB_ERR)
	{
		CString str;
		m_lstAllCmd.GetText(index,str);
		for(UINT i=0;i<m_nCommandNum;i++)
		{
			if(str==m_pSTS_Command[i]->m_strCmdName)
			{
//				GetDlgItem(IDC_STATIC_CMD_COMMENT)->SetWindowText(m_pSTS_Command[i]->m_strCmdComment);
				GetDlgItem(IDC_INFO)->SetWindowText(m_pSTS_Command[i]->m_strCmdParameter);
				if(m_pSTS_Command[i]->m_nMaxParaLength==0)
					GetDlgItem(IDC_INFO)->SetWindowText("");
				break;
			}
		}
	}	
}

#ifdef SUPPORT_2315_STS
void CRfCardDlg::Init_2315STS_Cmd()
{
	m_nCommandNum=0;
	#define REGISTER_CMD(a,b,c,d,e,f,g)	\
	{\
		m_pSTS_Command[m_nCommandNum++]=new CSTSCommand(a,b,c,d,e,f,g);\
		ASSERT(m_nCommandNum<=100);\
	}
#define MAX_PARAMETER_LENGTH 255
//	REGISTER_CMD(CMD_CATEGORY_OTHER,"Reset",
//		"Reset card",
//		"",0,0,
//		"");
//	REGISTER_CMD(CMD_CATEGORY_OTHER,"Warm Reset",
//		"Warm Reset card",
///		"",0,0,
//		"");

	REGISTER_CMD(CMD_CATEGORY_SFR,"Read SFR",		//0
		"Read out SFR’s value by its address.",
		"byte 0: SFR's address",
		1,1,
		"80ee00xx02");
	REGISTER_CMD(CMD_CATEGORY_SFR,"Write SFR",		//1
		"Write specified value to SFR by the address. If control byte is not \"0x00\", STS will write the SFR with timed access (write 0xAA, 0x55 to TA and then write the value to the SFR immediately), for other value, STS write the value to SFR directly.",
		"byte 0: SFR's address\tbyte 1: value",
		2,2,
		"80ee01xx02");

	REGISTER_CMD(CMD_CATEGORY_ROM,"Read ROM/EEPROM",		//2
		"Read data from ROM/EEPROM starting from specified address.",
		"byte 0-2: ROM/EEPROM address\tbyte 3: number of data to read",
		4,4,
		"80ee020004");
		
	REGISTER_CMD(CMD_CATEGORY_RAM,"Write RAM",		//3
		"Write data to RAM starting from specified address.",
		"byte 0: RAM's address\tbyte 1-...: value",
		2,MAX_PARAMETER_LENGTH,
		"80ee0300xx");
	REGISTER_CMD(CMD_CATEGORY_RAM,"Read RAM",		//4
		"Read data from RAM starting from specified address.",	
		"byte 0: RAM's address\tbyte 1: number of data to read",
		2,2,
		"80ee030102");
	REGISTER_CMD(CMD_CATEGORY_RAM,"Write_verify RAM",	//5
		"Write a constant to RAM (0x30-0xff), then read back and verify. First write 0x55 to RAM, and then write 0xAA to RAM. If no error occurs, send 0x99 to host, else send 0x66 to host.",
		"byte 0: data patten for test",
		1,1,
		"80ee030301");
	REGISTER_CMD(CMD_CATEGORY_RAM,"bank 0~3 test",		//6
		"Test bank 0~3 in RAM, First test 0x55 (write 0x55 to R0~R7 in all bank, read back by directory address and verify), then test “0xAA”, If no error occurs, send 0x99 to host, else send 0x66 to host.",
		"",
		0,0,
		"80ee030401");
	REGISTER_CMD(CMD_CATEGORY_RAM,"bit space test",		//7
		"Test bit space, If no error occurs, send 0x99 to host, else send 0x66 to host.",
		"",
		0,0,
		"80ee030501");

	REGISTER_CMD(CMD_CATEGORY_XRAM,"Write XRAM/EEPROM's buffer",		//8
		"Write data to XRAM starting from specified address.",
		"byte 0-2: XRAM's address\tbyte 3-...: Data to write",
		4,MAX_PARAMETER_LENGTH,
		"80ee0400xx");
	REGISTER_CMD(CMD_CATEGORY_RAM,"Read XRAM/EEPROM",	//9
		"Read data from XRAM starting from specified address.",
		"byte 0-2: XRAM's address\tbyte 3:number of data to read",
		4,4,
		"80ee040104");
	REGISTER_CMD(CMD_CATEGORY_RAM,"write_verify XRAM",	//10
		"Write a constant to XRAM, then read back and verify. First write 0x55 to XRAM, and then write 0xAA to XRAM. If no error occurs, send 0x99 to host, else send 0x66 to host.",
		"byte 0: data patten for test",
		1,1,
		"80ee040301");

	REGISTER_CMD(CMD_CATEGORY_EEPROM,"Write EEPROM",	//11
		"Write data to EEPROM starting from specified address.",
		"byte 0-2: EEPROM's address\tbyte 3-...: Data to write",
		4,MAX_PARAMETER_LENGTH,
		"80ee0500xx");

	REGISTER_CMD(CMD_CATEGORY_EEPROM,"write_read EEPROM",	//12
		"Write data to EEPROM starting from specified address and then read bytes from specified address.",
		"byte 0-2: EEPROM read address\tbyte 3:number of data to read\tbyte 4-6: EEPROM write address\tbyte 7-... Data to write",
		8,MAX_PARAMETER_LENGTH,
		"80ee0503xx");
	REGISTER_CMD(CMD_CATEGORY_EEPROM,"Chip write",			//13
		"Write 32 bytes to EEPROM in chip write mode.",
		"byte 0-3: address to write, 4-...: data to write",
		4,32+3,
		"80ee050523");
	REGISTER_CMD(CMD_CATEGORY_EEPROM,"Chip mode cycling",	//14
		"Cycling in test mode. Every time, write the inputted constant (Data) to EEPROM in EEPROM chip write cycle mode, after chip cycling for 1000 cycles, read EEPROM and verify them, if no error, clear IO and wait for 200 clocks and then set IO, then complement the constant and cycling again. If error, stop there using instruction \"AJMP $\". If the Test times (T) of command is not 0x00, it will only cycle for T times, only write the constant to EEPROM without verification.",
		"byte 0: times of chip cycling\tbyte 1: Data to chip cycling",
		2,2,
		"80ee050602");
	REGISTER_CMD(CMD_CATEGORY_EEPROM,"Page mode cycling",	//15
		"Cycling in test mode. Every time, write the inputted constant (Data) to EEPROM in EEPROM page mode, after chip cycling for specified check times, read EEPROM and verify them, if no error, clear IO and wait for 5000 clocks and then set IO, then complement the constant and cycling again. If error, stop there using instruction \"AJMP $\". If the Test times (T) of command is not 0x00, it will only cycle for T times, only write the constant to EEPROM without verification.",
		"byte 0: times of page cycling\tbyte 1: Data to page cycling",
		2,2,
		"80ee050702");
		
	REGISTER_CMD(CMD_CATEGORY_CRC,"CRC test",				//16
		"Input several bytes data to CRCD, and then output 2bytes CRCD result.",
		"byte 0-...: Data for test CRC",
		3,MAX_PARAMETER_LENGTH,
		"80ee0601xx");
	REGISTER_CMD(CMD_CATEGORY_CRC,"Memory CRC",				//17
		"Read Memory (OTP, EEPROM or ROM) and move them byte-by-byte to CRCD, then send back two bytes CRCD result.",
		"byte 0-2: start address for calculate, byte 3-5: end address for calculate",
		6,6,
		"80ee060306");

	REGISTER_CMD(CMD_CATEGORY_DES,"DES test",				//18
		"Input 24 bytes (8 bytes KeyA + 8 bytes KeyB + 8 bytes Data) for 3DES encryption, then output 8bytes encrypted data.",
		"byte 0: 01: Single DES decrypt\t        02: Triple DES encrypt\t        03: Triple DES decrypt\tbyte 2-8: Key A\tbyte 9-16:Key B\tbyte 17-24: Data",
		25,25,
		"80ee070119");

	REGISTER_CMD(CMD_CATEGORY_RNG,"RNG test",				//19
		"Generate specified bytes of random numbers.",
		"byte 0: number to generate",
		1,1,
		"80ee0800xx");
	REGISTER_CMD(CMD_CATEGORY_RNG,"RN to XRAM",				//20
		"Generate several random numbers and put them to XRAM.",
		"",
		0,0,
		"80ee080100");
#if 1//2006-08-11_11
	REGISTER_CMD(CMD_CATEGORY_FIFO,"fifo test",					//21
		"Test FIFODAT.",				
		"byte 0...: any data",
		0,MAX_PARAMETER_LENGTH,
		"80ee090000");
#endif
	REGISTER_CMD(CMD_CATEGORY_JMP,"jmp",					//21
		"Set PC to Address[2:0] and execute the code at Address[2:0].",				
		"byte 0-2: address to jump",
		3,3,
		"80ee0a0003");
}

/*
int CRfCardDlg::STS_CommandHandler(UINT nCmdID, UCHAR *pPara, UINT nParaLen, UCHAR *pExpect,UINT nExpectLen)
{
	if(m_EquSel == RF100){
		m_lstFromMcu.AddString("STS command now not supported on RF100 reader.");
		return -1;
	}
	if(m_EquSel != RF35){
		m_lstFromMcu.AddString("STS command now supported only on RF35 reader.");
		return -1;
	}
	CString strDisplay;
	
#define STR_CMD_NAME	(CString)(m_pSTS_Command[nCmdID]->m_strCmdName+"                  ").Left(18)
#define STR_CMD_OK		"...OK... "
#define STR_CMD_ER		"...Error... "

//#define RF35_rf_pro_trn_Header	4
//#define LENGTH_SEND_IN_BUF		3
//#define LENGTH_RCV_IN_BUF		3

#define SEND_LEN		(problock[3])
#define SEND_BYTE(x)	(problock[4+x])
#define RCV_LEN			(recv[3])
#define RCV_DATA_START	4
#define RCV_BYTE(x)		(recv[RCV_DATA_START+x])
	int st;
	unsigned char problock[256];
	unsigned char recv[256];
	problock[0]=0;
	problock[1]=0;
	problock[2]=0;
//	problock[3]=nParaLen;
//	memcpy(&problock[4],pPara,nParaLen);
	memset(recv,0,256);
	switch(nCmdID)
	{
		case 0://Read SFR
		{
			SEND_LEN=5;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			SEND_BYTE(3)=pPara[0];
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if( 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK+"SFR(%02x)=%02x",RCV_BYTE(0),RCV_BYTE(1));
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 4://read ram
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if( 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK+"RAM(%02x)=",RCV_BYTE(0),RCV_BYTE(1));
				strDisplay+=bufToString(&RCV_BYTE(0),RCV_LEN-2);
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 1://Write SFR
		case 3://write ram
		case 8://Write XRAM/EEPROM's buffer
		case 11://Write EEPROM
		case 13://Chip write
		case 14://Chip mode cycling
		case 15://Page mode cycling
		case 20://RN to XRAM
		case 22://JMP	//2006-08-11_11
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			SEND_BYTE(4)=nParaLen;
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if( 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 2://read rom/EEPROM
		case 9://read xram/EEPROM
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if( 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
				if(nCmdID==3)
					strDisplay=strDisplay+	"ROM/EEPROM(0x";
				else //if(nCmdID==9)
					strDisplay=strDisplay+	"XRAM/EEPROM(0x";
				CString strTmp;
				strTmp.Format("%02x%02x%02x)=",pPara[0],pPara[1],pPara[2]);
				strDisplay=strDisplay+	strTmp +	bufToString(&RCV_BYTE(0),RCV_LEN);
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 5://write_verify ram
		case 6://bank 0~3 test
		case 7://bit space test
		case 10://write_verify XRAM
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if(RCV_LEN==3 && 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 12://write_read EEPROM
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			SEND_BYTE(4)=nParaLen;
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if( 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
				strDisplay=strDisplay+	"EEPROM(0x";
				CString strTmp;
				strTmp.Format("%02x%02x%02x)=",pPara[0],pPara[1],pPara[2]);
				strDisplay=strDisplay+	strTmp +	bufToString(&RCV_BYTE(0),RCV_LEN);
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 16://CRC test
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			SEND_BYTE(4)=nParaLen;
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if(RCV_LEN==4 && 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				WORD	crc_result;
extern WORD crc_function(UCHAR b_reverse,UCHAR *pData,UINT length);
				crc_result=crc_function(0,pPara,nParaLen);
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
				CString str;
				if(RCV_BYTE(1)==(UCHAR)(crc_result>>8) && RCV_BYTE(0)==(UCHAR)(crc_result&0xff) )
				{
					str.Format("CRC Result corrcet= %02x %02x",RCV_BYTE(0),RCV_BYTE(1));
				}
				else
				{
					str.Format("CRC Result incorrect, Correct result= %02x %02x, result from card= %02x %02x",
						(UCHAR)(crc_result&0xff),(UCHAR)(crc_result>>8),RCV_BYTE(0),RCV_BYTE(1));
					Error_sum++;
				}
				strDisplay+=str;
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 17://Memory CRC
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			SEND_BYTE(4)=nParaLen;
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if(RCV_LEN==2 && 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				problock[0]=0;
				problock[1]=0;
				problock[2]=0;
			//	problock[3]=nParaLen;
			//	memcpy(&problock[4],pPara,nParaLen);
				memset(recv,0,256);
				SEND_BYTE(0)=0x80;	SEND_BYTE(1)=0xee;	SEND_BYTE(2)=0x06;	SEND_BYTE(3)=0x04;	SEND_BYTE(4)=0x02;
				SEND_LEN=5;
				Sleep(500);
				st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
				if(st)		return st;
				if(RCV_LEN==4 && 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
				{
					CString str;
					strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
					CString strTmp;
					strTmp.Format("%02x%02x%02x)=",pPara[0],pPara[1],pPara[2]);
					strDisplay=strDisplay+ "CRC from 0x" + strTmp + " to 0x";
					strTmp.Format("%02x%02x%02x)=",pPara[3],pPara[4],pPara[5]);
					strDisplay=strDisplay+strTmp+ " is: ";
					str.Format("%02x %02x",RCV_BYTE(0),RCV_BYTE(1));				
					strDisplay+=str;
					break;
				}				
				strDisplay=STR_CMD_NAME+STR_CMD_ER;
				Error_sum++;
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 18://DES test
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if(RCV_LEN==10 && 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
#define uint8 unsigned char
				uint8	KeyA[8],KeyB[8],Buf[8],	Use3Des,Encrypt;
				memcpy(KeyA,pPara+1,8);
				memcpy(KeyB,pPara+1+8,8);
				memcpy(Buf,pPara+1+8+8,8);
				if(pPara[0]&0x02)	{
					Use3Des=1;
					if(pPara[0]&0x01)
						Encrypt=0;
					else
						Encrypt=1;
				}else{	
					if(pPara[0]&0x04)
						memcpy(KeyA,KeyB,8);
					Use3Des=0;
					Encrypt=0;
				}
extern void des_function(uint8 Use3Des,uint8 Encrypt,uint8 KeyA[8],uint8 KeyB[8],uint8 Buf[8]);
				des_function(Use3Des,Encrypt,KeyA,KeyB,Buf);

				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
				if(0==memcmp(Buf,&RCV_BYTE(0),8)){
					strDisplay=strDisplay+"DES result correct, Card DES Result=" + bufToString(Buf,8);
				}else{
					strDisplay=strDisplay+"DES result incorrect! Correct Result=" + bufToString(Buf,8);
					Error_sum++;
					strDisplay=strDisplay+" Card DES Result=" + bufToString(&RCV_BYTE(0),8);
				}
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 19://RNG test
		{
			SEND_LEN=5;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			SEND_BYTE(4)=pPara[0];
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if(RCV_LEN>2 && 0x90==recv[RCV_DATA_START+RCV_LEN-2] && 0x00==recv[RCV_DATA_START+RCV_LEN-1])
			{
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
				strDisplay=strDisplay+",random number:" + bufToString(&RCV_BYTE(0),RCV_LEN);
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 21://fifo	//2006-08-11_11
		{
			SEND_LEN=5+nParaLen;
			memcpy(&SEND_BYTE(0),m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			SEND_BYTE(4)=nParaLen;
			memcpy(&SEND_BYTE(5),pPara,nParaLen);
			st=STS_SendCommand(icdev,problock,recv,pExpect,nExpectLen);
			if(st)		return st;
			if(RCV_LEN==nParaLen)
			{
				strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		default:
			ASSERT(0);
	}
	m_lstFromMcu.AddString(strDisplay);
	m_lstFromMcu.SetCurSel(m_lstFromMcu.GetCount()-1);
//	AdjustHorizontalScroll(&m_lstFromMcu);
	return 0;
#if 0

	UCHAR	Command[256];
	DWORD	nCommandLen;
	UCHAR	Response[256];
	DWORD	nResponseLen=0;
	
	TRACE(STR_CMD_NAME);
	ASSERT( (INT)nParaLen>=(m_pSTS_Command[nCmdID]->m_nMinParaLength) &&
			(INT)nParaLen<=(m_pSTS_Command[nCmdID]->m_nMaxParaLength) );
	switch(nCmdID)
	{
		case 5://write ram
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			Command[4]=nParaLen;
			memcpy(Command+5,pPara,nParaLen);
			nCommandLen=5+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
			{
				if(nResponseLen==2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
						break;
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 6://read ram
		{
#if 1
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(Command+5,pPara,1);
#else
			Command[2]=0x04;
			Command[3]=0x00;
			Command[4]=0x02;
			Command[5]=pPara[0];
			Command[6]=pPara[1];
#endif
			nCommandLen=5+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==(DWORD)2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
#if 1
						Command[2]=0x03;
						Command[3]=0x02;
#else
						Command[2]=0x00;
						Command[3]=0x00;
#endif
						Command[4]=pPara[1];
						nCommandLen=5;//+Command[4];
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
									CString str;
									str.Format("RAM(0x%02x)=",pPara[0]);
									strDisplay+=str;
									for(DWORD i=0;i<nResponseLen-2;i++)
									{
										str.Format(" %02x",Response[i]);
										strDisplay+=str;
									}
									break;
								}
							}
						}
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 7://Write&verify RAM
		case 8://bank 0~3 test
		case 0x0b://write & verify xram
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(Command+5,pPara,nParaLen);
			nCommandLen=5;//+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
			{
				if(nResponseLen==(DWORD)2+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
						if(Response[0]==0x99)
						{
							strDisplay+=" 99 90 00";
							break;
						}
						else if(Response[0]==0x66)
						{
			if( nCmdID==0x07 || nCmdID==0x08 )
								strDisplay+="Find RAM error!!!";
			else if(nCmdID==0x0b)
								strDisplay+="Find XRAM error!!!";
							break;
						}
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 9://write xram
		case 0x0c://write eeprom
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			Command[4]=nParaLen;
			memcpy(Command+5,pPara,nParaLen);
			nCommandLen=5+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
			{
				if(nResponseLen==2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
						break;
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x0a://read xram
		case 0x0d://read eeprom
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(Command+5,pPara,3);
			nCommandLen=5+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==(DWORD)2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
						Command[2]=0x04;
						Command[3]=0x02;
						Command[4]=pPara[3];
						nCommandLen=5;//+Command[4];
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
									CString str;
						if( nCmdID==0x0a )
									str.Format("XRAM(0x%02x%02x%02x)=",pPara[0],pPara[1],pPara[2]);
						else if( nCmdID==0x0d )
									str.Format("EEPROM(0x%02x%02x%02x)=",pPara[0],pPara[1],pPara[2]);
									strDisplay+=str;
									for(DWORD i=0;i<nResponseLen-2;i++)
									{
										str.Format(" %02x",Response[i]);
										strDisplay+=str;
									}
									break;
								}
							}
						}
					}
				}
			}
/*			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x0e://write& read eeprom
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(Command+5,pPara,nParaLen-1);
			Command[4]=nParaLen-1;
			nCommandLen=5+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==(DWORD)2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
						Command[2]=0x05;
						Command[3]=0x04;
						Command[4]=pPara[nParaLen-1];
						nCommandLen=5;//+Command[4];
//						Sleep(500);
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
									CString str;
									str.Format("EEPROM(0x%02x%02x%02x)=",pPara[0],pPara[1],pPara[2]);
									strDisplay+=str;
									for(DWORD i=0;i<nResponseLen-2;i++)
									{
										str.Format(" %02x",Response[i]);
										strDisplay+=str;
									}
									break;
								}
							}
						}
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x0f://chip write
		case 0x10://chip mode cycling
		case 0x11://page mode cycling
		case 0x17://hw/sw 7816
		case 0x18://jmp
		case 0x1b://write chip id
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(Command+5,pPara,nParaLen);
			nCommandLen=5+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
			{
				if(nResponseLen==2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
						break;
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x12://crc test
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			Command[4]=nParaLen;
			memcpy(Command+5,pPara,nParaLen);
			nCommandLen=5+Command[4];
			WORD	crc_result;
extern WORD crc_function(UCHAR b_reverse,UCHAR *pData,UINT length);
			crc_result=crc_function(0,pPara,nParaLen);
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
						Command[2]=0x06;
						Command[3]=0x00;
						Command[4]=0x02;
						nCommandLen=5;//+Command[4];						
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
									CString str;
									if(Response[1]==(UCHAR)(crc_result>>8) && Response[0]==(UCHAR)(crc_result&0xff) )
									{
										str.Format("CRC Result corrcet= %02x %02x",Response[0],Response[1]);
									}
									else
									{
										str.Format("CRC Result incorrect, Correct result= %02x %02x, result from card= %02x %02x",
											(UCHAR)(crc_result&0xff),(UCHAR)(crc_result>>8),Response[0],Response[1]);
										Error_sum++;
									}
									strDisplay+=str;
									break;
								}
							}
						}
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x13://eeprom crc
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			nCommandLen=5;//+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
						Command[2]=0x06;
						Command[3]=0x00;
						Command[4]=0x02;
						nCommandLen=5;//+Command[4];
						Sleep(1000);
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									strDisplay=STR_CMD_NAME+STR_CMD_OK;
									CString str;
									str.Format("CRC Result= %02x %02x",Response[0],Response[1]);
									strDisplay+=str;
									break;
								}
							}
						}
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x14://memory crc
		{			
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(Command+5,pPara,nParaLen);
			nCommandLen=5+Command[4];
			nCommandLen=5+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
						Command[2]=0x06;
						Command[3]=0x00;
						Command[4]=0x02;
						nCommandLen=5;//+Command[4];
				if(nCmdID==0x13)
						Sleep(1000);
				if(nCmdID==0x14)
						Sleep(1000);
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									strDisplay=STR_CMD_NAME+STR_CMD_OK;
									CString str;
									str.Format("CRC Result= %02x %02x",Response[0],Response[1]);
									strDisplay+=str;
									break;
								}
							}
						}
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x15://des
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			Command[4]=nParaLen;
			memcpy(Command+5,pPara,nParaLen);
			nCommandLen=5+Command[4];

#define uint8 unsigned char
			uint8	KeyA[8];
			uint8	KeyB[8];
			uint8	Buf[8];
			uint8	Use3Des;
			uint8	Encrypt;
			if(pPara[0]&0x02)
			{
				Use3Des=1;
				if(pPara[0]&0x01)
					Encrypt=0;
				else
					Encrypt=1;
			}
			else
			{
				Use3Des=0;
				Encrypt=0;
			}
			memcpy(KeyA,pPara+1,8);
			memcpy(KeyB,pPara+1+8,8);
			memcpy(Buf,pPara+1+8+8,8);
extern void des_function(uint8 Use3Des,uint8 Encrypt,uint8 KeyA[8],uint8 KeyB[8],uint8 Buf[8]);
			des_function(Use3Des,Encrypt,KeyA,KeyB,Buf);

			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
						Command[2]=0x07;
						Command[3]=0x00;
						Command[4]=0x08;
						nCommandLen=5;//+Command[4];
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									CString str;
									strDisplay=STR_CMD_NAME+STR_CMD_OK;
									if(0==memcmp(Buf,Response,8))
									{
										strDisplay+="DES result correct, Card DES Result=";
									}
									else
									{
										strDisplay+="DES result incorrect! Correct Result=";
										for(UINT i=0;i<8;i++)
										{
											str.Format(" %02x",Buf[i]);
											strDisplay+=str;
										}					
										Error_sum++;
										strDisplay+=" Card DES Result=";
									}									
									for(UINT i=0;i<nResponseLen-2;i++)
									{
										str.Format(" %02x",Response[i]);
										strDisplay+=str;
									}
									break;
								}
							}
						}
					}
				}
			}
			Error_sum++;
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			break;
		}
		case 0x16://rng
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			Command[4]=pPara[0];
			nCommandLen=5;//+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==(DWORD)2+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						strDisplay=STR_CMD_NAME+STR_CMD_OK+"Random numbers:";
						CString str;
						for(UINT i=0;i<nResponseLen-2;i++)
						{
							str.Format(" %02x",Response[i]);
							strDisplay+=str;
						}
						break;
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x19://write otp
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			Command[4]=nParaLen;
			memcpy(Command+5,pPara,nParaLen);
			nCommandLen=5+Command[4];

			{
				UCHAR	temp=0x00;
				UINT	t;
				for(t=0;t<nParaLen-1;t++)
				{
					TRACE("temp=0x%02x, pPara[%d]=0x%02x\n",temp,t,pPara[t]);
					temp^=pPara[t];
					TRACE("temp=0x%02x\n",temp);
				}
				if(temp!=pPara[t])
				{
					strDisplay.Format("Check sum error, correct check sum is 0x%02x",temp);
					Error_sum++;
					break;
				}
			}

			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
						Command[2]=0x0b;
						Command[3]=0x01;
						Command[4]=0x02;
						nCommandLen=5;//+Command[4];
						Sleep(20);
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									strDisplay=STR_CMD_NAME+STR_CMD_OK;
									if(Response[0]==0)
										strDisplay+="no error during writing OPT";
									else if(Response[0]==1)
									{
										strDisplay+="OTP write is forbidden!";
									}
									else if(Response[0]==2)
									{
										strDisplay+="Check sum failed!";
									}
									else if(Response[0]==3)
									{
										CString str;
										str.Format("find error while writing byte[%02x]",Response[1]);
										strDisplay+=str;
									}
									else
									{
										strDisplay+="transfer error or sts error!";
									}
									break;
								}
							}
						}
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x1a://read otp
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			memcpy(Command+5,pPara,3);
			nCommandLen=5+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen) )
			{
				if(nResponseLen==(DWORD)2)//+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						Command[0]=0x80;
						Command[1]=0xc0;
						Command[2]=0x0b;
						Command[3]=0x03;
						Command[4]=pPara[3];
						nCommandLen=5;//+Command[4];
						if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
						{
							if(nResponseLen==(DWORD)2+Command[4])
							{
								if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
								{
									strDisplay.Format(STR_CMD_NAME+STR_CMD_OK);
									CString str;
									str.Format("OTP(0x%02x%02x%02x)=",pPara[0],pPara[1],pPara[2]);
									strDisplay+=str;
									for(DWORD i=0;i<nResponseLen-2;i++)
									{
										str.Format(" %02x",Response[i]);
										strDisplay+=str;
									}
									break;
								}
							}
						}
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x1c://read chip id
		{
			memcpy(Command,m_pSTS_Command[nCmdID]->m_pCmdHeder,5);
			nCommandLen=5;//+Command[4];
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
			{
				if(nResponseLen==(DWORD)2+Command[4])
				{
					if( 0x90==Response[nResponseLen-2] && 0x00==Response[nResponseLen-1] )
					{
						strDisplay=STR_CMD_NAME+STR_CMD_OK+"Chip ID=";
						for(UINT i=0;i<nResponseLen;i++)
						{
							CString str;
							str.Format(" %02x",Response[i]);
							strDisplay+=str;
						}
						break;
					}
				}
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x1d://direct command
		{
			memcpy(Command,pPara,nParaLen);
			nCommandLen=nParaLen;
			if( 0==SendCommand(hCardHandle,Command,nCommandLen,Response,&nResponseLen,pResp,nRespLen) )
			{
				strDisplay=STR_CMD_NAME+"Response:";
				CString str;
				for(DWORD i=0;i<nResponseLen;i++)
				{
					str.Format(" %02x",Response[i]);
					strDisplay+=str;
				}
				break;
			}
			strDisplay=STR_CMD_NAME+STR_CMD_ER;
			Error_sum++;
			break;
		}
		case 0x1e://delay ms
		{
			DWORD i;
			i=pPara[0];
			for(UINT j=1;j<nParaLen;j++)
				i=i*256+pPara[j];
#if 0
			Sleep(i);
#else
			DWORD dwStart = GetTickCount();
			DWORD dwEnd   = dwStart;
			do
			{
				MSG   msg;
				GetMessage(&msg,NULL,0,0);
				TranslateMessage(&msg); 
				DispatchMessage(&msg);
				dwEnd = GetTickCount()-dwStart;
			}while(dwEnd <i);
#endif
			break;
		}
		case 0x1f://comment
		{
/*
			break;
		}
		default:
			ASSERT(0);
	}
	m_lstFromMcu.AddString(strDisplay);
	m_lstFromMcu.SetCurSel(m_lstFromMcu.GetCount()-1);
	AdjustHorizontalScroll(&m_lstFromMcu);
	return;

    if(m_EquSel == RF35)
	{
		int st;
		unsigned char problock[256];
		unsigned char recv[256];
		problock[0]=0;
		problock[1]=0;
		problock[2]=0;
		problock[3]=nParaLen;
		memcpy(&problock[4],pPara,nParaLen);
		memset(recv,0,256);
		{
			CString str="RF out: [PCB] + ";
			for(UINT i=0;i<nParaLen;i++)
			{
				CString strTmp;
				strTmp.Format(" %02x",pPara[i]);
				str+=strTmp;
			}
			m_lstFromMcu.AddString(str+"+ [CRC]");
		}
		st=rf_pro_trn(icdev,problock,recv);		//返 回：成功则返回 0
		if(st){
			m_lstFromMcu.AddString("Command execute error. (while call function:rf_pro_trn)");
			return st;
		}

		{
			CString str="RF in: [PCB] + ";
			for(int i=4;i<recv[3]+4;i++)
			{
				CString strTmp;
				strTmp.Format(" %02x",recv[i]);
				str+=strTmp;
			}
			m_lstFromMcu.AddString(str+"+ [CRC]");
		}
	}
	else if(m_EquSel == RF100)
	{
		m_lstFromMcu.AddString("STS command now not supported on RF100 reader.");
		return -1;
	}
#endif
	return 0;
}*/

CString CRfCardDlg::bufToString(UCHAR *pbuf, UINT len)
{
	CString str;
	for(UINT i=0;i<len;i++)
	{
		CString strTmp;
		strTmp.Format(" %02x",pbuf[i]);
		str+=strTmp;
	}
	return str;
}
/*
int CRfCardDlg::STS_SendCommand(HANDLE icdev, unsigned char *problock, unsigned char *recv, UCHAR *pExpect, UINT nExpectLen)
{
	m_lstFromMcu.AddString("RF Out: [PCB]" + bufToString(&problock[4],problock[3])+ " [CRC]");
	st=rf_pro_trn(icdev,problock,recv);		//返 回：成功则返回 0
	if(st){
		m_lstFromMcu.AddString("Command execute error. (while call function:rf_pro_trn)");
		return st;
	}
	m_lstFromMcu.AddString("RF In: [PCB]" + bufToString(&recv[4],recv[3])+ " [CRC]");
	if(nExpectLen!=0)
	{
		if(memcmp(pExpect,&recv[4],nExpectLen))
		{
			m_lstFromMcu.AddString("Response not as expected!");
			return -1;
		}
	}
	return 0;
}

BOOL CRfCardDlg::Is_STS_Command(CString str)
{
	CString strCmdLine;
	strCmdLine=str;
	strCmdLine.Replace("\t","");		strCmdLine.TrimLeft();
	if(strCmdLine.GetLength()==0)	return	FALSE;
	{
		int i;
		i=strCmdLine.Find("//");
		if(i>=0)	strCmdLine=strCmdLine.Left(i);
		i=strCmdLine.Find(";");
		if(i>=0)	strCmdLine=strCmdLine.Left(i);
	}
	strCmdLine.MakeLower();
	if(strCmdLine==strCmdLine.SpanIncluding("0123456789abcdef <="))
		strCmdLine="directcommand,"+strCmdLine;
	int index;
	index=strCmdLine.Find(",",0);
	CString strCmdName;
	if(index>0)
	{
		strCmdName=strCmdLine.Left(index);
	}
	else
	{
		strCmdName=strCmdLine;
	}
	strCmdName.MakeLower();		strCmdName.TrimLeft();		strCmdName.TrimRight();
	for(UINT nCmdID=0;nCmdID<m_nCommandNum;nCmdID++)
	{
		CString strTemp=m_pSTS_Command[nCmdID]->m_strCmdName;
		strTemp.MakeLower();
		if(strCmdName==strTemp)
			return TRUE;
	}
	return FALSE;
}*/

#endif	//SUPPORT_2315_STS

void CRfCardDlg::OnDestroy() 
{
	CDialog::OnDestroy();
	
	//delete response data
	ClearResponseData();

	// delete cmds list
	if (m_pCommonCmds != NULL)
	{
		while (!m_pCommonCmds->IsEmpty())
		{
			CCommand* pCmd = m_pCommonCmds->GetHead();
			if (pCmd != NULL)
			{
				if (pCmd->pData != NULL)
					delete pCmd->pData;
				pCmd->pData = NULL;
				delete pCmd;
				pCmd = NULL;
			}
			m_pCommonCmds->RemoveHead();
		}
		m_pCommonCmds->RemoveAll();
		delete m_pCommonCmds;
		m_pCommonCmds = NULL;
	}
	if (m_pIICCmds != NULL)
	{
		while (!m_pIICCmds->IsEmpty())
		{
			CCommand* pCmd = m_pIICCmds->GetHead();
			if (pCmd != NULL)
			{
				if (pCmd->pData != NULL)
					delete pCmd->pData;
				pCmd->pData = NULL;
				delete pCmd;
				pCmd = NULL;
			}
			m_pIICCmds->RemoveHead();
		}
		m_pIICCmds->RemoveAll();
		delete m_pIICCmds;
		m_pIICCmds = NULL;
	}
	if (m_pSPICmds != NULL)
	{
		while (!m_pSPICmds->IsEmpty())
		{
			CCommand* pCmd = m_pSPICmds->GetHead();
			if (pCmd != NULL)
			{
				if (pCmd->pData != NULL)
					delete pCmd->pData;
				pCmd->pData = NULL;
				delete pCmd;
				pCmd = NULL;
			}
			m_pSPICmds->RemoveHead();
		}
		m_pSPICmds->RemoveAll();
		delete m_pSPICmds;
		m_pSPICmds = NULL;
	}
	if (m_pMWCmds != NULL)
	{
		while (!m_pMWCmds->IsEmpty())
		{
			CCommand* pCmd = m_pMWCmds->GetHead();
			if (pCmd != NULL)
			{
				if (pCmd->pData != NULL)
					delete pCmd->pData;
				pCmd->pData = NULL;
				delete pCmd;
				pCmd = NULL;
			}
			m_pMWCmds->RemoveHead();
		}
		m_pMWCmds->RemoveAll();
		delete m_pMWCmds;
		m_pMWCmds = NULL;
	}

	if (m_pATCmds != NULL)
	{
		while (!m_pATCmds->IsEmpty())
		{
			CCommand* pCmd = m_pATCmds->GetHead();
			if (pCmd != NULL)
			{
				if (pCmd->pData != NULL)
					delete pCmd->pData;
				pCmd->pData = NULL;
				delete pCmd;
				pCmd = NULL;
			}
			m_pATCmds->RemoveHead();
		}
		m_pATCmds->RemoveAll();
		delete m_pATCmds;
		m_pATCmds = NULL;
	}
	if(g_netSendData.data != NULL)
	{
		delete g_netSendData.data;
		g_netSendData.data = NULL;
	}
}

void CRfCardDlg::OnBtnAddcomment() 
{
	UpdateData(TRUE);

	CString strComment = "//";
	strComment += m_strPara;
	//add to command window
	if (m_lstCmd.GetCurSel() == LB_ERR)//no selection
        m_lstCmd.InsertString(m_lstCmd.GetCount(),strComment);//just add string at end of string list
	else
        m_lstCmd.InsertString(m_lstCmd.GetCurSel()+1,strComment);
	
	AdjustHorizontalScroll(&m_lstCmd);
}

void CRfCardDlg::OnSelchangeListDevice() 
{
	char strDevice[256];
	int nCurSel = m_listDevice.GetCurSel();
	m_listDevice.GetText(nCurSel, strDevice);
	//update ORG radio buttons
	BOOL bEnable = strstr(strDevice, "IS93") ? TRUE:FALSE;
	GetDlgItem(IDC_STATIC_ORG)->ShowWindow(bEnable);
	GetDlgItem(IDC_RADIO_0)->ShowWindow(bEnable);
	GetDlgItem(IDC_RADIO_1)->ShowWindow(bEnable);
	CheckRadioButton(IDC_RADIO_0, IDC_RADIO_1, IDC_RADIO_0);
}

BOOL CRfCardDlg::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::OnCommand(wParam, lParam);
}
