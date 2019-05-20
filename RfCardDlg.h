// RfCardDlg.h : header file
//

#if !defined(AFX_RFCARDDLG_H__56B304A2_970F_4F10_B923_90C7564FAD44__INCLUDED_)
#define AFX_RFCARDDLG_H__56B304A2_970F_4F10_B923_90C7564FAD44__INCLUDED_

#include "ColorListBox.h"
#include "HScrollListBox.h"
#include "TextProgressCtrl.h"
#include "wdu_lib.h"
#include "utils.h"
#include "windrvr.h"
#include <afxtempl.h>


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WM_DISPLAY_CHANGE WM_USER+1
#define WM_CONTINUE_TIMER WM_USER+2


///////////////////////////////////////////////For USB device/////////////////////////
// change the following definitions to match your device.
#define DEFAULT_VENDOR_ID         0x0471
#define DEFAULT_PRODUCT_ID        0x0666
#define DEFAULT_LICENSE_STRING    "6f1ea7e369bc92835210e676875e40a1c2bb48dc8817d1.ISSI"
#define DEFAULT_INTERFACE_NUM     0
#define DEFAULT_ALTERNATE_SETTING 0
#define USB_PIPENUMBER 2


#define USE_DEFAULT 0xffff
#define ATTACH_EVENT_TIMEOUT 20 // in seconds
#define TRANSFER_TIMEOUT 2000 // in msecs

typedef struct _UartData
{
	char *pUartData;
	int dataLen;
}UartData;

typedef struct _UartDataNode
{
	UartData uartData;
	struct _UartDataNode *pNext;
	//struct _UartDataNode *pPre;
}UartDataNode;

typedef struct _UartDataHead
{
	int dataNum;
	UartDataNode *pHeadNode;
	UartDataNode *pRearNode;	
}UartDataHead;


typedef struct DEVICE_CONTEXT
{
    struct DEVICE_CONTEXT *pNext;
    WDU_DEVICE_HANDLE hDevice;
    DWORD dwVendorId;
    DWORD dwProductId;
    DWORD dwInterfaceNum;
    DWORD dwAlternateSetting;
} DEVICE_CONTEXT;

typedef struct DRIVER_CONTEXT
{
    HANDLE hEvent;
    HANDLE hMutex;
    DWORD dwDeviceCount;
    DEVICE_CONTEXT *deviceContextList;
    DEVICE_CONTEXT *pActiveDev;
    HANDLE hDeviceUnusedEvent;
} DRIVER_CONTEXT;
//////////////////////////////////////End for USB device///////////////////////////


///////////////////////////////////For communication//////////////////////////
///communication para
#define INPUT_BUFFERSIZE  4096
#define OUTPUT_BUFFERSIZE 4096
#define WAIT_TIMEOUT_MS   10000
#define WAIT_WRITEBYTE_MS   2000
#define MAX_RESPONSE_TIMES 1

#define ERROR_TIMEOUT1    0x00
#define ERROR_CHECKSUM    0x01
#define ERROR_PARAMETERS  0x02
#define ERROR_OTHER       0x03

//port config data
typedef struct Port
{
	HANDLE hPort;
	unsigned int nType; //0:USB, 1:UART
	int nNum;
	BOOL bEnable;
	//if UART
	unsigned long BaudRate;
	//if usb
	//HANDLE hDriver;
	//DRIVER_CONTEXT DrvCtx;
}Port;


typedef struct ReceiveData
{
	int responseId;
	int nDataLen;
	int position;
	unsigned char *data;
}ReceiveData;

typedef enum _EXT_AT_TOKEN_
{
	EXT_AT_TOKEN_BEGIN = 1000,
	EXT_AT_TOKEN_MYNETWRITE,
	EXT_AT_TOKEN_URC,
	EXT_AT_TOKEN_END	
}EXT_AT_TOKEN;

typedef void (* EXT_AT_DATA_RES_CB)(ReceiveData *pSrcRes);


typedef struct _AT_RESPONSE_TAB_
{
	int token;
	const char *WriteAtCmd;
	const char *ReponseCmd;
	EXT_AT_DATA_RES_CB resCbfun;
}AT_RESPONSE_TAB;


typedef struct NetSendData
{
	int len;
	int pos;
	int partCount;
	char *data;
}NetSendData;


////////////////////////////////////////For commands/////////////////////
#define KEY_COMMONCMD "Common_"
#define KEY_IICCMD "IIC_"
#define KEY_SPICMD "SPI_"
#define KEY_MWCMD "MW_"
#define KEY_ATCMD "AT"
#define END_CHAR  "\r\n"

#define SEMICOLON_CHAR       (';')
#define COLON_CHAR           (':')
#define COMMA_CHAR           (',')
#define DOT_CHAR             ('.')
#define QUOTES_CHAR          ('"')
#define QUERY_CHAR           ('?')
#define EQUALS_CHAR          ('=')
#define STAR_CHAR            ('*')
#define HASH_CHAR            ('#')
#define SPACE_CHAR           (' ')
#define AMPERSAND_CHAR       ('&')
#define HAT_CHAR             ('^')
#define BACK_SLASH_CHAR      (92)
#define PLUS_CHAR            ('+')
#define NULL_CHAR            ('\0')
#define ESC_CHAR             (27)
#define CTRL_Z_CHAR          (26)
#define INTERNATIONAL_PREFIX ('+')
#define S_REGISTER_CHAR      ('S')
#define FWD_SLASH_CHAR       ('/')
#define ZERO_CHAR            ('0')
#define NINE_CHAR            ('9')
#define NETWRITE_RESPONSE    "\r\n$MYNETWRITE:"
#define NETWRITE_CMD		"AT_MYNETWRITE"
#define MYCOMMON_CMD		"AT_COMMON"


#define COMMON_COMMAND 0
#define IIC_COMMAND 1
#define SPI_COMMAND 2
#define MW_COMMAND  3
#define AT_COMMAND  4

//command struct
class CCommand
{
public:
	CCommand() {pData = NULL;};
	CCommand(const CCommand& copy)
	{
		uCmdType = copy.uCmdType;
		uCmdId = copy.uCmdId;
		uDataLen = copy.uDataLen;
		strCmdName = copy.strCmdName;
		strCmdData = copy.strCmdData;   //add by jinjiyuan
		strCmdComment = copy.strCmdComment;
		strParaComment = copy.strParaComment;
	};
	virtual ~CCommand(){};
public:
	unsigned char uCmdType;
	unsigned char uCmdId;
	unsigned char uDataLen;
	CString strCmdName;
	CString strCmdData;  //add jinjiyuan
	unsigned char *pData;
	CString strCmdComment;
	CString strParaComment;
};
typedef CTypedPtrList <CPtrList, CCommand*> COMMANDLIST;
typedef COMMANDLIST* LPCMDLIST;
////////////////////////////////////////////End for commands/////////////


/////////////////////////////////////////////////////////////////////////////
// CRfCardDlg dialog

class CRfCardDlg : public CDialog
{
// Construction
public:
	void Add2315STSCmd();
	void AddDemoCos();
	void AddMifareCommand();
	CRfCardDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CRfCardDlg)
	enum { IDD = IDD_RFCARD_DIALOG };
	CListBox	m_listDevice;
	CSpinButtonCtrl	m_Spin_Times;
	CProgressCtrl	m_CmdFinishedProgress;
	CSpinButtonCtrl	m_Spin_Blk;
	CSpinButtonCtrl	m_Spin_Sector;
	CHScrollListBox	m_lstFromMcu;
    CColorListBox m_lstCmd;
	CListBox	m_lstAllCmd;
	CString	m_lstAllCmdValue;
	CString	m_write_hex_begin;
	CString	m_Key_Value;
	CString	m_Inc_Value;
	CString	m_Dec_Value;
	CString	m_Trans_Value;
	CString	m_Sector_Value;
	CString	m_Blk_Value;
	CString	m_Write2_Value;
	int		m_WinSel;
	BOOL	m_CrcErr;
	BOOL	m_ParaErr;
	int		m_Comport;
	int		m_BaudRate;
	BOOL	m_stop;
	BOOL	m_Mcm200_On;
	BOOL	m_ShowResp;
	BOOL    m_bUpdateTimes;
	long	m_RunTimes;
	UINT	m_version;
	CString	m_IDdata;
	BYTE	m_cgmode;
	CString	m_modeedit;
	BOOL	m_block00;
	BOOL	m_block01;
	BOOL	m_block02;
	BOOL	m_block10;
	BOOL	m_block11;
	BOOL	m_block12;
	BOOL	m_block20;
	BOOL	m_block21;
	BOOL	m_block22;
	BOOL	m_block30;
	BOOL	m_block31;
	BOOL	m_block32;
	CString	m_keya;
	CString	m_keyb;
	CString	m_ErrorSum;
	BOOL	m_StopAuthen;
	BOOL	m_StopAuthen2;
	int		m_EquSel;
	int 	m_strCombo;
	CString	m_strPara;
	long	m_delayTime;
	//}}AFX_DATA
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRfCardDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL
    
// Implementation
protected:
	HICON m_hIcon;
	

	// Generated message map functions
	//{{AFX_MSG(CRfCardDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	
	afx_msg  LRESULT  OnDisplayChange(WPARAM wParam, LPARAM lParam);
	
	afx_msg LRESULT OnContinueTimer(WPARAM wParam,LPARAM lParam);
	afx_msg void OnAdd();
	afx_msg void OnOpen();
	afx_msg void OnSave();
	afx_msg void OnBtnDeleteAll();
	afx_msg void OnBtnDelete();
	afx_msg void OnRun();
	afx_msg void OnStep();
	afx_msg void OnLink();
	afx_msg void OnDblclkListCmd();
	afx_msg void OnDblclkListAllCmd();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnStop();
	afx_msg void OnButton3();
	afx_msg void OnButton4();
	afx_msg void OnStepandnext();
	afx_msg void OnSelchangeCombo1();
	afx_msg void OnSelchangeListAllCmd();
	afx_msg void OnDestroy();
	afx_msg void OnBtnAddcomment();
	afx_msg void OnSelchangeListDevice();
	//}}AFX_MSG
	//afx_msg void OnRunStep();
	DECLARE_MESSAGE_MAP()

public:

private:
	BOOL m_bConnect;
	BOOL m_bUsb;
	LPCMDLIST m_pCommonCmds;
	LPCMDLIST m_pIICCmds;
	LPCMDLIST m_pSPICmds;
	LPCMDLIST m_pMWCmds;
	LPCMDLIST m_pATCmds;

	unsigned int m_nTimerID;
	unsigned int m_SendAtCmdTimeID;
	BOOL m_bRunning;
	BOOL m_bBreakOn;
	BOOL m_bTestMode;
	BOOL m_bKeyB_On;
    int m_nCurBrkpt;
	int m_nPreBrkpt;
	int m_nCurrentLine;
	int m_nEnd;
	int m_nTotallLine;
	long m_nTotallTimes;
	long m_nDelayTimes;
	//void CmdHandler(unsigned char *Cmd);
	//#ifdef	SUPPORT_2315_STS

public:
	//port functions
	void InitPorts();
	void SetPorts();
	BOOL ClosePorts();
	BOOL LinkPorts();
	
	BOOL CRfCardDlg::SendATCommand(Port port, CCommand *pCmd);
	BOOL LinkOnePort();

	//command functions
	BOOL CheckCommand(CString strCmdLine);
	BOOL HandleCommand(CCommand *pCommand);
	BOOL ReadInfoFile(CString strFile);
	CCommand* TranslateCmd(CString strCommand);
	void WaitForTermination();

	//GUI
	void AdjustHorizontalScroll(CListBox *pListBox);
	void UpdateGUI(void);
	void EnableButtons();

public:
	//BOOL Is_STS_Command(CString str);
	//int STS_SendCommand(HANDLE icdev,unsigned char *problock,unsigned char *recv,UCHAR* pExpect,UINT nExpectLen);
	CString bufToString(UCHAR* pbuf,UINT len);
	int STS_CommandHandler(UINT nCmdID,UCHAR *pPara,UINT nParaLen,UCHAR *pExpect,UINT nExpectLen);
	void Init_2315STS_Cmd();
	class CSTSCommand
	{
	public:
		CSTSCommand(UINT nCmdCategory,CString strName,
			CString StrCmdComment,CString strParameter,
			UINT nMinParaLength,UINT nMaxParaLength,
			CString strCmdHeader)
		{
			m_strCmdName=strName;
			m_strCmdComment=StrCmdComment;
			m_strCmdParameter=strParameter;
			m_nMinParaLength=nMinParaLength;
			m_nMaxParaLength=nMaxParaLength;
			if(strCmdHeader!="")
				for(UINT i=0;i<5;i++)
				{
					CHAR tmp[3];
					memcpy(tmp,strCmdHeader,2);
					tmp[2]='\0';
					m_pCmdHeder[i]=(UCHAR)strtol(tmp,NULL,16);
					strCmdHeader=strCmdHeader.Mid(2);
				}
		};
		CString m_strCmdName;
		CString m_strCmdComment;
		CString m_strCmdParameter;
		INT	m_nMinParaLength,m_nMaxParaLength;
		UCHAR	m_pCmdHeder[5];
	};
	CSTSCommand	*m_pSTS_Command[100];
	UINT m_nCommandNum;
//#endif
};


#ifdef	SUPPORT_2315_STS
	#define CMD_CATEGORY_SFR		0
	#define CMD_CATEGORY_ROM		1
	#define CMD_CATEGORY_RAM		2
	#define CMD_CATEGORY_XRAM		3
	#define CMD_CATEGORY_EEPROM		4
	#define CMD_CATEGORY_CRC		5
	#define CMD_CATEGORY_DES		6
	#define CMD_CATEGORY_RNG		7
	#define CMD_CATEGORY_FIFO		8	//CMD_CATEGORY_FIFO
	#define CMD_CATEGORY_JMP		9
	#define CMD_CATEGORY_OTP		10
	#define CMD_CATEGORY_CHIPID		11
	#define CMD_CATEGORY_OTHER		12
#endif
unsigned short  CmdConvert(CString );
unsigned char CmdTranslation(CString,unsigned char* CmdPt);
void AsciiConvert(char *AsciiChar,unsigned char *RetBuffer, unsigned char DataCount);

int CrcCreate(unsigned char outbyte, int status, unsigned char *crcdata);
unsigned char crypt(unsigned char inbyte, int status, int shiftnumber);
unsigned char random16(unsigned char initlow, unsigned char inithigh,int bitnumber, int status);
unsigned short outputcryptbyte(unsigned char InDat, int bitnumber,int cryptstatus);
BOOL OpenComm(int CommNum,unsigned long BaudRate);
BOOL ReadCommPort( unsigned char *buffer, unsigned int length);
BOOL WriteCommPort(unsigned char *buffer, unsigned char length);
short protocol(HANDLE icdev,int len, unsigned char *send_cmd, unsigned char *receive_data);
void app_extAt_myNetWrite_res(ReceiveData *repsonse_buff);
void app_extAt_myURC_res(ReceiveData *repsonse_buff);

unsigned char parabity(unsigned char dat);
bool getExtendedParameter (ReceiveData *commandBuffer_p,
                               int *value_p,
                                int inValue);
bool getExtendedString (ReceiveData *commandBuffer_p,
                            unsigned char *outString,
                             unsigned int maxStringLength,
                             unsigned int *outStringLength);

CString GetExePath();


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RFCARDDLG_H__56B304A2_970F_4F10_B923_90C7564FAD44__INCLUDED_)
