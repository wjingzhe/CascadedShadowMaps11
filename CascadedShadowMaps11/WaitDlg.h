#pragma once

// File: WaitDlg.h
//
// wait dialog for shader compilation
//
#include <windows.h>
#include <process.h>
#include <WinUser.h>
#include <CommCtrl.h>
#include "DXUT.h"
#include "Resource.h"

//----------------------------------------
INT_PTR CALLBACK WaitDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

unsigned int __stdcall UpdateBarDialog(void* pArg);

//----------------------------------------------------------
class CWaitDlg
{
public:
	CWaitDlg():
		m_hDialogWnd(nullptr),
		m_hThread(nullptr),
		m_hProcessWnd(nullptr),
		m_iProcess(0),
		m_bDone(FALSE)
	{

	}
	~CWaitDlg()
	{
		DestroyDialog();
	}

	bool IsRunning()
	{
		return !m_bDone;
	}

	void UpdateProcessBar()
	{
		m_iProcess++;
		if (m_iProcess > 110)
		{
			m_iProcess = 0;
		}

		SendMessage(m_hProcessWnd, PBM_SETPOS, m_iProcess, 0);
		InvalidateRect(m_hDialogWnd, nullptr, FALSE);
		UpdateWindow(m_hDialogWnd);
	}

	bool GetDialogControls()
	{
		m_bDone = FALSE;

		m_hDialogWnd = CreateDialog(DXUTGetHINSTANCE(), MAKEINTRESOURCE(IDD_COMPILING_SHADERS), nullptr, WaitDialogProc);
		if (!m_hDialogWnd)
		{
			return false;
		}

		SetWindowLongPtr(m_hDialogWnd, GWLP_USERDATA, (LONG_PTR)this);

		//Set the position

		int appMiddleX = (m_AppRect.left + m_AppRect.right) / 2;
		int appMiddleY = (m_AppRect.top + m_AppRect.bottom) / 2;

		SetWindowPos(m_hDialogWnd, nullptr, appMiddleX, appMiddleY, 0, 0, SWP_NOSIZE);
		ShowWindow(m_hDialogWnd, SW_SHOW);

		//Get the process bar
		m_hProcessWnd = GetDlgItem(m_hDialogWnd, IDC_PROGRESS_BAR);
		SendMessage(m_hProcessWnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

		// Update the static text
		HWND hMessage = GetDlgItem(m_hDialogWnd, IDC_MESSAGE);
		SetWindowText(hMessage, m_szText);

		return true;
	}

	bool ShowDialog(WCHAR* pszInputText)
	{
		//Get the window rect
		GetWindowRect(DXUTGetHWND(), &m_AppRect);
		wcscpy_s(m_szText, MAX_PATH, pszInputText);

		// spawn a thread that does nothing but update the progress bar
		unsigned int threadAddr;
		m_hThread = (HANDLE)_beginthreadex(nullptr, 0, UpdateBarDialog, this, 0, &threadAddr);

		return true;
	}

	void DestroyDialog()
	{
		m_bDone = TRUE;
		WaitForSingleObject(m_hThread, INFINITE);

		if (m_hDialogWnd)
		{
			DestroyWindow(m_hDialogWnd);
		}
		m_hDialogWnd = nullptr;
	}

private:
	HWND m_hDialogWnd;
	HANDLE m_hThread;
	HWND m_hProcessWnd;
	int m_iProcess;
	BOOL m_bDone;
	RECT m_AppRect;
	WCHAR m_szText[MAX_PATH];
};
