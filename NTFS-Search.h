#pragma once

#include "resource.h"
//-----------------------------------------------------------------------------------------------------------------------
#include "SxLib/SxWnd.h"
#include "SxLib/SxApp.h"
#include "SxLib/SxDialog.h"
#include "SxLib/SxThread.h"
#include <memory>
#include "SxLib/SxDriveListCombBox.h"
#include "SxLib/SxListView.h"
//-----------------------------------------------------------------------------------------------------------------------\]
class MyApp : public SxApp
{
public:
    MyApp();
    BOOL InitInstance();
public:
    HIMAGELIST list2 = nullptr;
    HIMAGELIST list = nullptr;
};
//-----------------------------------------------------------------------------------------------------------------------
class CSearchThread :public SxThread
{
public:
    CSearchThread() :SxThread( nullptr )
    {
        return;
    }
protected:
    int Worker();

public:
    struct ThreadInfo
    {
        HWND hWnd;
        DISKHANDLE* disk;
    } m_info = { nullptr };

};
//-----------------------------------------------------------------------------------------------------------------------
class CWaitDialog : public SxDialog
{
public:
    CWaitDialog( SxWnd* Parent , HINSTANCE hInst ) :SxDialog( Parent , hInst )
    {
        m_lpctTemplate = MAKEINTRESOURCE( IDD_WAIT );
    }
    ~CWaitDialog()
    {
        return;
    }
    DISKHANDLE* m_pPDiskHandle = nullptr;
protected:
    LRESULT CALLBACK WindowProc( HWND hwnd , UINT uMsg , WPARAM wParam , LPARAM lParam );

    std::unique_ptr<CSearchThread> m_pThread;
};
//-----------------------------------------------------------------------------------------------------------------------
class MyDialog : public SxDialog
{
public:
    MyDialog( SxWnd* Parent , HINSTANCE hInst ) :SxDialog( Parent , hInst )
    {
        m_lpctTemplate = MAKEINTRESOURCE( IDD_SEARCH );
        return;
    }
    virtual ~MyDialog()
    {
        return;
    }
    //------------------------
    //メッセージ関係
    //------------------------
protected:
    BOOL _OnInitDialog( HWND hDlg , HWND hwndFocus , LPARAM lParam );
    LRESULT _OnNotify( HWND hWnd , int idFrom , NMHDR FAR* pnmhdr );
    void _OnCommand( HWND hwnd , int id , HWND hwndCtl , UINT codeNotify );
    void _OnContextMenu( HWND hwnd , HWND hwndContext , UINT xPos , UINT yPos );
    void _OnSize( HWND hWnd , UINT state , int cx , int cy );
    void _On_WmUser2();
    LRESULT CALLBACK WindowProc( HWND hwnd , UINT uMsg , WPARAM wParam , LPARAM lParam );
protected:
    std::unique_ptr<SxDriveListCombBox> m_pSxDriveListCombBox = nullptr;
    std::unique_ptr<SxStatusBarCtr> m_pSxStatusBarCtr = nullptr;
    std::unique_ptr<SxList> m_pSxList = nullptr;
    //------------------------
    //いろいろ
    //------------------------
protected:
    int ProcessLoading( HWND hWnd , HWND hCombo , int reload );
    void StartLoading( DISKHANDLE* disk , HWND hWnd );
    BOOL ProcessPopupMenu( HWND hWnd , int index , DWORD item );
    void ReleaseAllDisks();
    UINT ExecuteFile( HWND hWnd , STRSAFE_LPWSTR str , const USHORT flags );
    UINT ExecuteFileEx( HWND hWnd , const wchar_t* command , LPCWSTR str , LPCWSTR dir , const UINT show , const USHORT flags );
    bool UnloadDisk( HWND hWnd , int index );
    DWORD ShowError();
    int SearchFiles( HWND hWnd , DISKHANDLE* disk , wchar_t* filename , const bool deleted , SEARCHP* pat );
    int Search( HWND hWnd , int disk , wchar_t* filename , const bool deleted );
    bool SaveResults( const TCHAR* filename );
    void PrepareCopy( HWND hWnd , UINT flags );
    void DeleteFiles( HWND hWnd , UINT flags );

    HMENU m_hMenuPopUp = nullptr;
    HMENU m_hhm = nullptr;

};
