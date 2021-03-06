//=================================================================================================
//
//  Copyright (C) 2012-2016, DigiStar Studio
//
//  This file is part of nMatrix client app (http://feather1231.myweb.hinet.net/)
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//
//  Above are some template claims copied from somewhere on the Internet that don't matter.
//  The code is written by c++ language and I know you have no fear of it. :D
//  Hope that you use the code to do good things like google's slogan, don't be evil.
//  Any feedback is welcomed.
//
//  Author: Pointer Huang <digistarstudio@google.com>
//
//
//=================================================================================================


#include "stdafx.h"
#include "VPN Client.h"
#include "SetupDialog.h"
#include "DriverAPI.h"
#include "CnMatrixCore.h"


// Data for language cache file.
DWORD GLanID = 0;
HANDLE hGLangCacheFile = INVALID_HANDLE_VALUE;
CString GCacheFilePath, csGColon;
TCHAR *GLanguageName[LLI_TOTAL] = { _T("Auto"), _T("English"), _T("繁體中文"), _T("简体中文")};

#define LanEventFnSize 1
LanEventCallback GLanEventFn[LanEventFnSize];
void *GLanEventData[LanEventFnSize];
UINT GTotalLanEventFn = 0;

void RegLanEventCB(LanEventCallback pFn, void *pData)
{
	GLanEventFn[GTotalLanEventFn] = pFn;
	GLanEventData[GTotalLanEventFn] = pData;
	++GTotalLanEventFn;
	ASSERT(GTotalLanEventFn <= LanEventFnSize);
}

BOOL SetGUILanguage(UINT LanListID)
{
	BOOL bInit = !GLanID;
	if(hGLangCacheFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hGLangCacheFile);
		hGLangCacheFile = INVALID_HANDLE_VALUE;
	}

	WORD ResID = 0;
	if(LanListID == LLI_AUTO)
	{
		GLanID = GetUserDefaultUILanguage();
		switch(GLanID)
		{
			case 0x0404: // Chinese Traditional.
				ResID = IDR_LAN_CHT;
				break;

			case 0x0804: // Chinese Simplified.
				ResID = IDR_LAN_CHS;
				break;
		}
	}
	else
	{
		switch(LanListID)
		{
			case LLI_ENGLISH:
				GLanID = DEFAULT_LANGUAGE_ID;
				break;

			case LLI_CHT: // Chinese Traditional.
				GLanID = 0x0404;
				ResID = IDR_LAN_CHT;
				break;

			case LLI_CHS: // Chinese Simplified.
				GLanID = 0x0804;
				ResID = IDR_LAN_CHS;
				break;
		}
	}

	if(!ResID)
		GLanID =  DEFAULT_LANGUAGE_ID;
	else
	{
		TCHAR path[MAX_PATH + 1];
		GetTempPath(MAX_PATH + 1, path);
		GCacheFilePath = path;
		ASSERT(GCacheFilePath[GCacheFilePath.GetLength() - 1] == '\\');
		GCacheFilePath += _T("lcache.tmp");

		if(SaveResourceToFile(GCacheFilePath, ResID))
			hGLangCacheFile = CreateFile(GCacheFilePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY, 0);
	}

	if(!bInit)
		for(UINT i = 0; i < GTotalLanEventFn; ++i)
			GLanEventFn[i](GLanEventData[i], GLanID);

	return TRUE;
}

DWORD GetUILanguage()
{
	return GLanID;
}

CString GUILoadString(DWORD StringID)
{
	TCHAR buffer[512];
	CString cs;
	if(hGLangCacheFile == INVALID_HANDLE_VALUE || GLanID == DEFAULT_LANGUAGE_ID)
		cs.LoadString(StringID);
	else
	{
		cs.Format(_T("%d"), StringID);
		GetPrivateProfileString(_T("GUI"), cs, 0, buffer, sizeof(buffer) / sizeof(TCHAR), GCacheFilePath);
		cs = buffer;
		if(cs == _T(""))
			cs.LoadString(StringID);
	}

	return cs;
}

BOOL GetServiceFileDirectory(CString &csPath)
{
	TCHAR szPath[MAX_PATH];

	if(!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, 0, szPath)))
		return FALSE;

	csPath = szPath;
	csPath += _T("\\DigiStar Studio");

	return TRUE;
}

BOOL SetupServiceFile(BOOL &bVersionOK, CString &csPath)
{
	bVersionOK = FALSE;

	if(!GetServiceFileDirectory(csPath))
		return FALSE;
	if(GetFileAttributes(csPath) == INVALID_FILE_ATTRIBUTES)
		if(!CreateDirectory(csPath, 0))
		{
			printx("CreateDirectory failed! ec: %d\n", GetLastError());
			return FALSE;
		}

	UINT size = 0;
	bVersionOK = TRUE;
	csPath += _T("\\nMatrix.exe");
	DWORD VerSize = GetFileVersionInfoSize(csPath, 0);
	if(VerSize)
	{
		void *pBlock = malloc(VerSize);
		if(pBlock)
		{
			if(GetFileVersionInfo(csPath, 0, VerSize, pBlock))
			{
				VS_FIXEDFILEINFO *verInfo;
				if(VerQueryValue(pBlock, _T("\\"), (void**)&verInfo, &size) && size && verInfo->dwSignature == 0xfeef04bd)
				{
					INT major, minor, build/*, nouse*/;
					major = HIWORD(verInfo->dwFileVersionMS);
					minor = LOWORD(verInfo->dwFileVersionMS);
					build = HIWORD(verInfo->dwFileVersionLS);
				//	nouse = LOWORD(verInfo->dwFileVersionLS);

					if(AppGetVersion() < APP_VERSION(major, minor, build))
						bVersionOK = FALSE;

					printx("File version %d.%d.%d\n", major, minor, build);
				}
			}
			free(pBlock);
		}
	}

	if(bVersionOK)
	{
		TCHAR szPath[MAX_PATH];
		GetModuleFileName(0, szPath, MAX_PATH);
		UINT nTryCount = 100;
		while(nTryCount--)
			if(!CopyFile(szPath, csPath, FALSE))
			{
				DWORD dwError = GetLastError();
				if(dwError == ERROR_SHARING_VIOLATION)
				{
					Sleep(100);
					continue;
				}
				printx(_T("CopyFile failed! ec:%d\n"), dwError);
				return FALSE;
			}
			else
				return TRUE;
	}

	return FALSE;
}


IMPLEMENT_DYNAMIC(CInfoDialog, CDialog)
BEGIN_MESSAGE_MAP(CInfoDialog, CDialog)
	ON_WM_TIMER()
END_MESSAGE_MAP()


void RenameNetworkConnection(CString NewName)
{
//	printx("---> RenameNetworkConnection\n");

	if(!SetNetworkConnectionName(_T(ADAPTER_DESC), NewName))
	{
		//printx("Change network connection name failed!\n");
	}

	DWORD dwVersion = GetVersion();
	if(LOBYTE(LOWORD(dwVersion)) >= 6) // Major version number of Windows Vista.
	{
		SetNT6NetworkName(NewName);
	}
}

void CInfoDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CInfoDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText(_T(NMATRIX_SETUP_INFO_DIALOG_NAME));
	SetTimer(1, 100, 0);

	if(m_NewMsg != _T(""))
		UpdateMessage(0);
	else
		UpdateMessage(_T(""));

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CInfoDialog::OnTimer(UINT_PTR nIDEvent)
{
	if(m_NewMsg != _T(""))
	{
		UpdateMessage(0);
		m_NewMsg.Empty();
	}

	if(m_bEndDialog)
	{
		KillTimer(1);
		EndDialog(m_bResult);
	}

	CDialog::OnTimer(nIDEvent);
}

void CInfoDialog::OnCancel()
{
	if(GetKeyState(VK_F2) < 0)
		CDialog::OnCancel();
}

void CInfoDialog::UpdateMessage(TCHAR *pMsg)
{
	CStatic *pStatic = (CStatic*)GetDlgItem(IDC_MSG);

	if(pMsg)
		pStatic->SetWindowText(pMsg);
	else
		pStatic->SetWindowText(m_NewMsg);
}


CSetupDialog::CSetupDialog(CWnd* pParent /*=NULL*/)
: CDialog(CSetupDialog::IDD, pParent)
, m_Mode(MODE_INSTALL), m_bComplete(FALSE), m_bResult(0)
{
	m_bDebug = FALSE;
	m_bSilentUpdate = FALSE;
}

CSetupDialog::~CSetupDialog()
{
}

void CSetupDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


IMPLEMENT_DYNAMIC(CSetupDialog, CDialog)
BEGIN_MESSAGE_MAP(CSetupDialog, CDialog)
	ON_BN_CLICKED(ID_NEXT, &CSetupDialog::OnBnClickedNext)
	ON_BN_CLICKED(IDC_DEBUG, &CSetupDialog::OnBnClickedDebug)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_TIMER()
	ON_WM_WINDOWPOSCHANGING()
	ON_BN_CLICKED(IDC_COPY_ERR, &CSetupDialog::OnBnClickedCopyErr)
END_MESSAGE_MAP()


void CSetupDialog::OnCancel()
{
	if(m_bComplete)
	{
		if(m_Mode == MODE_INSTALL || MODE_UPDATE == MODE_UPDATE)
		{
			DeleteFile(_T("c:\\DevInsLog.txt")); // Log file generated by DevIns.
		}

		EndDialog(m_bResult);
		return;
	}

	CString csMsg(_T("This will exit setup wizard, continue?")), csCaption(_T("Exit setup"));

	if(MessageBox(csMsg, csCaption, MB_YESNO | MB_ICONQUESTION) == IDYES)
		CDialog::OnCancel();
}

BOOL CSetupDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_BoldFont.CreateFont(
		22,                        // nHeight
		0,                         // nWidth
		0,                         // nEscapement
		0,                         // nOrientation
		FW_BOLD,                   // nWeight
		FALSE,                     // bItalic
		FALSE,                     // bUnderline
		0,                         // cStrikeOut
		ANSI_CHARSET,              // nCharSet
		OUT_DEFAULT_PRECIS,        // nOutPrecision
		CLIP_DEFAULT_PRECIS,       // nClipPrecision
		DEFAULT_QUALITY,           // nQuality
		DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
	   _T("Arial")
	   );

	GetDlgItem(IDC_TITLE)->SetFont(&m_BoldFont);

	CString csVersion, csInfo;
	csVersion.Format(_T("Build: %s"), _T(__DATE__));
	GetDlgItem(IDC_VERSION)->SetWindowText(csVersion);

	if(m_Mode == MODE_INSTALL)
		csInfo = _T("Before using the software, you must install nMatrix VPN adapter driver. Click Next to Continue.");
	else if(m_Mode == MODE_UPDATE)
		csInfo = _T("Please click next to update nMatrix VPN adapter driver.");
	else if(m_Mode == MODE_UNINSTALL)
		csInfo = _T("Thank you for using nMatrix. Click Next to uninstall nMatrix adapter driver.");

	SetInfoText(csInfo);

	HICON hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_SETUP));
	SetIcon(hIcon, TRUE);

	CWnd *pWndCheck = GetDlgItem(IDC_NDIS6);
	DWORD dwVersion = GetVersion();
	if(LOBYTE(LOWORD(dwVersion)) >= 6 && m_Mode != MODE_UNINSTALL) // Major version number of Windows Vista.
	{
		pWndCheck->EnableWindow(TRUE);
		pWndCheck->ShowWindow(SW_SHOW);
		((CButton*)pWndCheck)->SetCheck(BST_CHECKED);
	}

	m_b64Bit = Is64BitsOs();
	if(m_b64Bit)
		printx("Running in 64-bit os.\n");

	if(m_bSilentUpdate)
		SetTimer(1, 100, 0);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

DWORD WINAPI SetupThread(LPVOID lpParameter)
{
	CInfoDialog *pInfoDialog = (CInfoDialog*)lpParameter;
	BOOL bDebug = pInfoDialog->IsDebug();
	// Wait info dialog first.
	Sleep(THREAD_INIT_WAIT_TIME);
	TCHAR path[MAX_PATH + 1];
	GetTempPath(MAX_PATH + 1, path);
	CString csDevInsPath(path), csInfPath(path), csDriverPath(path), csCatFilePath(path), csParam, csErrorString;
	USHORT DevInsResID, InfResID, DriverBinResID, CatFileResID;
	ASSERT(csDevInsPath[csDevInsPath.GetLength() - 1] == '\\');

	if(pInfoDialog->IsNdis6())
	{
		InfResID = IDR_INF_FILE_N6;
		if(pInfoDialog->Is64BitsOS())
		{
			DevInsResID = IDR_DEVINS64;
			DriverBinResID = IDR_DRIVER64_N6;
			CatFileResID = IDR_CAT_X64N6;
		}
		else
		{
			DevInsResID = IDR_DEVINS32;
			DriverBinResID = IDR_DRIVER32_N6;
			CatFileResID = IDR_CAT_X32N6;
		}
	}
	else
	{
		InfResID = IDR_INF_FILE;
		if(pInfoDialog->Is64BitsOS())
		{
			DevInsResID = IDR_DEVINS64;
			DriverBinResID = bDebug ? IDR_DRIVER64D : IDR_DRIVER64;
			CatFileResID = bDebug ? IDR_CAT_X64D : IDR_CAT_X64;
		}
		else
		{
			DevInsResID = IDR_DEVINS32;
			DriverBinResID = bDebug ? IDR_DRIVER32D : IDR_DRIVER32;
			CatFileResID = bDebug ? IDR_CAT_X32D : IDR_CAT_X32;
		}
	}

	BOOL bResult = FALSE;
	csDevInsPath += _T(DEVINS_FILE_NAME);
	if(!SaveResourceToFile(csDevInsPath, DevInsResID))
	{
		bResult = 2;
		csErrorString.Format(_T("Failed to save DevIns file! Path: %s"), csDevInsPath);
		printx(_T("%s\n"), csErrorString);
	}
	csInfPath += _T(INF_FILE_NAME);
	if(!SaveResourceToFile(csInfPath, InfResID))
	{
		bResult = 2;
		csErrorString.Format(_T("Failed to save inf file! Path: %s"), csInfPath);
		printx(_T("%s\n"), csErrorString);
	}
	csCatFilePath += _T(CAT_FILE_NAME);
	if(!SaveResourceToFile(csCatFilePath, CatFileResID))
	{
		bResult = 2;
		csErrorString.Format(_T("Failed to save catalog file! Path: %s"), csCatFilePath);
		printx(_T("%s\n"), csErrorString);
	}
	csDriverPath += _T(DRIVER_FILE_NAME);
	if(!SaveResourceToFile(csDriverPath, DriverBinResID))
	{
		bResult = 2;
		csErrorString.Format(_T("Failed to save driver file! Path: %s"), csDriverPath);
		printx(_T("%s\n"), csErrorString);
	}

//	BOOL bResult = InstallDriver(_T("root\\NetVMini"), csInfPath.GetBuffer(), 0);
	if(!bResult)
	{
		STARTUPINFO si = {0};
		PROCESS_INFORMATION pi = {0};
		si.cb = sizeof(si);

		if(pInfoDialog->IsUpdate())
		{
			// Can't update windows driver directly after version 6.2 (win8).
			DWORD dwVersion = GetVersion();

			printx("windows version: %d.%d\n\n", LOBYTE(LOWORD(dwVersion)), HIBYTE(LOWORD(dwVersion)));

			if(LOBYTE(LOWORD(dwVersion)) == 6 && HIBYTE(LOWORD(dwVersion)) >= 2)
			{
				csParam.Format(_T("-u %s"), _T(ADAPTER_DESC));
				if( !CreateProcess( csDevInsPath,			// Module name.
									csParam.GetBuffer(),	// Command line.
									NULL,					// Process handle not inheritable.
									NULL,					// Thread handle not inheritable.
									FALSE,					// Set handle inheritance to FALSE.
									0,						// No creation flags.
									NULL,					// Use parent's environment block.
									NULL,					// Use parent's starting directory.
									&si,					// Pointer to STARTUPINFO structure.
									&pi ))					// Pointer to PROCESS_INFORMATION structure.
				{
					printx("CreateProcess() failed! Error code: %d.\n", GetLastError());
				}
				else
				{
					WaitForSingleObject(pi.hProcess, INFINITE);
					bResult = 0;
					if(!GetExitCodeProcess(pi.hProcess, (DWORD*)&bResult))
						printf("GetExitCodeProcess() failed! Error code: %d.\n", GetLastError());

					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
				}

				csParam.Format(_T("-i %s %s %s"), _T(ADAPTER_HARDWARE_ID), csInfPath, _T(NMATRIX_SETUP_INFO_DIALOG_NAME));
			}
			else
				csParam.Format(_T("-U %s %s %s"), _T(ADAPTER_HARDWARE_ID), csInfPath, _T(NMATRIX_SETUP_INFO_DIALOG_NAME));
		}
		else
			csParam.Format(_T("-i %s %s %s"), _T(ADAPTER_HARDWARE_ID), csInfPath, _T(NMATRIX_SETUP_INFO_DIALOG_NAME));

		if( !CreateProcess( csDevInsPath,			// Module name.
							csParam.GetBuffer(),	// Command line.
							NULL,					// Process handle not inheritable.
							NULL,					// Thread handle not inheritable.
							FALSE,					// Set handle inheritance to FALSE.
							0,						// No creation flags.
							NULL,					// Use parent's environment block.
							NULL,					// Use parent's starting directory.
							&si,					// Pointer to STARTUPINFO structure.
							&pi ))					// Pointer to PROCESS_INFORMATION structure.
		{
			bResult = 2;
			csErrorString.Format(_T("CreateProcess() failed! ec: %d.\n"), GetLastError());
			printx(_T("%s\n"), csErrorString);
		}
		else
		{
			WaitForSingleObject(pi.hProcess, INFINITE);
			if(!GetExitCodeProcess(pi.hProcess, (DWORD*)&bResult))
			{
				bResult = 2;
				csErrorString.Format(_T("GetExitCodeProcess() failed! ec: %d."), GetLastError());
				printx(_T("%s\n"), csErrorString);
			}

			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}

	INT nTryCount = 10;
	while(nTryCount--)
		if(DeleteFile(csDevInsPath))
			break;
		else if(GetLastError() == ERROR_ACCESS_DENIED)
		{
			printx("Failed to delete DevIns file(ERROR_ACCESS_DENIED)!\n");
			Sleep(500);
		}
		else
		{
			printx("Failed to delete DevIns file! Error coe: %d.\n", GetLastError());
			break;
		}

	//DeleteFile(csDevInsPath);
	DeleteFile(csInfPath);
	DeleteFile(csCatFilePath);
	DeleteFile(csDriverPath);

	if(!bResult || bResult == 1)
		RenameNetworkConnection(_T(NETWORK_CONNECTION_NAME));

	printx("Install completed with code: %d.\n", bResult);

	if(csErrorString != _T(""))
		pInfoDialog->SetErrorMsg(csErrorString);
	pInfoDialog->CloseDialog(bResult);

	return 0;
}

DWORD WINAPI UninstallThread(LPVOID lpParameter)
{
	CInfoDialog *pInfoDialog = (CInfoDialog*)lpParameter;

	// Wait info dialog first.
	Sleep(THREAD_INIT_WAIT_TIME);

	TCHAR path[MAX_PATH];
	GetTempPath(MAX_PATH, path);
	CString csDevInsPath(path), csParam;
	ASSERT(csDevInsPath[csDevInsPath.GetLength() - 1] == '\\');
	csDevInsPath += _T(DEVINS_FILE_NAME);
	WORD DevInsResID = pInfoDialog->Is64BitsOS() ? IDR_DEVINS64 : IDR_DEVINS32;
	BOOL bResult = FALSE;

	if(!SaveResourceToFile(csDevInsPath, DevInsResID))
	{
		printx(_T("Failed to save DevIns file! Path: %s\n"), csDevInsPath);
	}
	else
	{
		STARTUPINFO si = {0};
		PROCESS_INFORMATION pi = {0};
		si.cb = sizeof(si);

		csParam.Format(_T("-u %s"), _T(ADAPTER_DESC));
		if( !CreateProcess( csDevInsPath,			// Module name.
							csParam.GetBuffer(),	// Command line.
							NULL,					// Process handle not inheritable.
							NULL,					// Thread handle not inheritable.
							FALSE,					// Set handle inheritance to FALSE.
							0,						// No creation flags.
							NULL,					// Use parent's environment block.
							NULL,					// Use parent's starting directory.
							&si,					// Pointer to STARTUPINFO structure.
							&pi ))					// Pointer to PROCESS_INFORMATION structure.
		{
			printx("CreateProcess() failed! Error code: %d.\n", GetLastError());
		}
		else
		{
			WaitForSingleObject(pi.hProcess, INFINITE);
			bResult = 0;
			if(!GetExitCodeProcess(pi.hProcess, (DWORD*)&bResult))
				printf("GetExitCodeProcess() failed! Error code: %d.\n", GetLastError());

			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}

		DeleteFile(csDevInsPath);
	}

//	BOOL bResult = UninstallDriver(bNeedReboot);
//	pInfoDialog->SetReboot(bNeedReboot);
	pInfoDialog->CloseDialog(bResult);

	return 0;
}

void CSetupDialog::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if(m_Mode == MODE_UNINSTALL)
		return;

	CRect rect;
	CWnd *pWnd = GetDlgItem(IDC_DEBUG);

	pWnd->GetWindowRect(rect);
	ScreenToClient(rect);

	if(rect.left <= point.x && point.x <= rect.right)
		if(rect.top <= point.y && point.y <= rect.bottom)
		{
			pWnd->ShowWindow(SW_NORMAL);
			pWnd->EnableWindow(TRUE);
		}

//	CDialog::OnLButtonDblClk(nFlags, point);
}

void CSetupDialog::OnBnClickedDebug()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_DEBUG);

	if(pButton->GetCheck() == BST_CHECKED)
		m_bDebug = TRUE;
	else
		m_bDebug = FALSE;
}

void CSetupDialog::OnBnClickedCopyErr()
{
	CString ErrorMsg;
	GetDlgItemText(IDC_INFO, ErrorMsg);
	SetClipboardContent(GetSafeHwnd(), ErrorMsg);
}

void CSetupDialog::OnBnClickedNext()
{
	CInfoDialog InfoDialog;
	InfoDialog.SetOSVersion(m_b64Bit);
	InfoDialog.SetDebug(m_bDebug);
	HANDLE hHandle;

	BOOL bNdis6 = FALSE;
	bNdis6 = (((CButton*)GetDlgItem(IDC_NDIS6))->GetCheck() == BST_CHECKED);
	InfoDialog.SetNdis6(bNdis6);

	if(m_Mode == MODE_INSTALL)
	{
		hHandle = CreateThread(0, 0, SetupThread, &InfoDialog, 0, 0);
		CloseHandle(hHandle);

		CString csInfo(_T("Setup nMatrix network adapter driver..."));
		InfoDialog.SetMsg(csInfo);
		m_bResult = InfoDialog.DoModal();

		switch(m_bResult)
		{
			case 0:
			case 1: // Need reboot.
				SetInfoText(_T("Setup successfully!"));
				break;

			default:
				if(InfoDialog.GetErrorMsg() != _T(""))
				{
					CString msg;
					msg.Format(_T("Setup failed!\n\n%s"), InfoDialog.GetErrorMsg());
					SetInfoText(msg);
				}
				else
					SetInfoText(_T("Setup failed!\n"));
				GetDlgItem(IDC_COPY_ERR)->ShowWindow(SW_NORMAL);
				break;
		}
	}
	if(m_Mode == MODE_UPDATE)
	{
		InfoDialog.SetUpdate();
		hHandle = CreateThread(0, 0, SetupThread, &InfoDialog, 0, 0);
		CloseHandle(hHandle);

		InfoDialog.SetMsg(GUILoadString(IDS_STRING1502));
		m_bResult = InfoDialog.DoModal();

		switch(m_bResult)
		{
			case 0:
			case 1: // Need reboot.
				SetInfoText(_T("Update successfully!"));
				break;

			default:
				SetInfoText(_T("Update failed! Please run setup next time."));
				break;
		}
	}
	else if(m_Mode == MODE_UNINSTALL)
	{
		hHandle = CreateThread(0, 0, UninstallThread, &InfoDialog, 0, 0);
		CloseHandle(hHandle);

		CString csInfo(_T("Uninstall nMatrix network adapter driver..."));
		InfoDialog.SetMsg(csInfo);
		m_bResult = InfoDialog.DoModal();

		switch(m_bResult)
		{
			case 0:
				SetInfoText(_T("Uninstall successfully!"));
				break;

			case 1:
				break;

			default:
				SetInfoText(_T(":(\n\nUninstall failed! Please run setup next time."));
				break;
		}
	}

	GetDlgItem(ID_NEXT)->ShowWindow(SW_HIDE);
	GetDlgItem(IDCANCEL)->SetWindowText(_T("&Finish"));
	m_bComplete = TRUE;

	if(m_bSilentUpdate)
		OnCancel();
}

void CSetupDialog::OnTimer(UINT_PTR nIDEvent)
{
	KillTimer(nIDEvent);
	OnBnClickedNext();

//	CDialog::OnTimer(nIDEvent);
}

void CSetupDialog::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
	CDialog::OnWindowPosChanging(lpwndpos);

	if(m_bSilentUpdate)
		lpwndpos->flags &= ~SWP_SHOWWINDOW;
}

void CSetupDialog::SetInfoText(const TCHAR *pText)
{
	GetDlgItem(IDC_INFO)->SetWindowText(pText);
}

BOOL CSetupDialog::CheckDriverVersion()
{
	INT main, sub, build, nmain, nsub, nbuild;
	HANDLE hAdapter = OpenAdapter();

	ASSERT(AppGetVersion() >= AppGetDriverResourceVersion());

	if(hAdapter != INVALID_HANDLE_VALUE)
	{
		stDriverVerInfo DriVerInfo = { 0 };
		GetAdapterDriverVersion(hAdapter, &DriVerInfo);
		CloseAdapter(hAdapter);

	//	AppGetVersionDetail(DriVerInfo.MinAppVersion, main, sub, build);
	//	printx("Driver requires min app version: %d.%d.%d\n", main, sub, build);

		if(AppGetVersion() < DriVerInfo.MinAppVersion)
			return FALSE;

		DWORD DriverResVersion = AppGetDriverResourceVersion();
		if(DriverResVersion > DriVerInfo.DriVersion)
		{
			AppGetVersionDetail(DriVerInfo.DriVersion, main, sub, build);
			AppGetVersionDetail(DriverResVersion, nmain, nsub, nbuild);
			printx("Update driver. Ver: %d.%d.%d -> %d.%d.%d.\n", main, sub, build, nmain, nsub, nbuild);

			// Before updating must stop core service if it's running.
			BOOL bServiceIsRunning = ((CVPNClientApp*)AfxGetApp())->StopService();

			SetMode(MODE_UPDATE);
			if(GetKeyState(VK_F2) >= 0)
				EnableSilentUpdate();
			if(DoModal() == IDCANCEL /*&& !(GetKeyState(VK_F2) < 0)*/)
			{
			}

			BOOL bVersionOK;
			CString csPath;
			if(SetupServiceFile(bVersionOK, csPath) && bServiceIsRunning) // Must update service file too.
				((CVPNClientApp*)AfxGetApp())->StartService();
		}
	}

	return TRUE;
}


