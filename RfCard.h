// RfCard.h : main header file for the RFCARD application
//

#if !defined(AFX_RFCARD_H__8D4C05F6_7BF4_411B_8BB9_936E2560A0D6__INCLUDED_)
#define AFX_RFCARD_H__8D4C05F6_7BF4_411B_8BB9_936E2560A0D6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols



/////////////////////////////////////////////////////////////////////////////
// CRfCardApp:
// See RfCard.cpp for the implementation of this class
//

class CRfCardApp : public CWinApp
{
public:
	CRfCardApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRfCardApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CRfCardApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RFCARD_H__8D4C05F6_7BF4_411B_8BB9_936E2560A0D6__INCLUDED_)
