#include "WaitDlg.h"


INT_PTR WaitDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CWaitDlg* pDialog = (CWaitDlg*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (uMsg)
	{
	case WM_INITDIALOG:
		return true;
		break;
	case WM_CLOSE:
		pDialog->DestroyDialog();
		return TRUE;
		break;
	}

	return FALSE;
}

unsigned int UpdateBarDialog(void * pArg)
{
	CWaitDlg* pDialog = (CWaitDlg*)pArg;

	// We create the dialog in this thread,so we can call SendMessage without blocking on the
	// main thread's message pump
	pDialog->GetDialogControls();

	while (pDialog->IsRunning())
	{

		pDialog->UpdateProcessBar();
		Sleep(100);
	}

	return 0;
}
