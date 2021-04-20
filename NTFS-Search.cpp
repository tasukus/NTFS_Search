// NTFSearch.cpp : Defines the entry point for the application.
//

/* BugFixes
    - limit Results // more or less done 0xfff0
    - extend searchstrings
    - include sort // nearly done
    - include typeinformation // done
    - progressbar completion / fixed by cheating
*/
//-----------------------------------------------------------------------------------------------------------------------
#ifdef _DEBUG
   #define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)
    #define _CRTDBG_MAP_ALLOC
#endif // _DEBUG
//-----------------------------------------------------------------------------------------------------------------------
#include "stdafx.h"

#include <stdlib.h>
#include <crtdbg.h>
#include "NTFS_STRUCT.h"
#include "SimplePattern.h"
#include "NTFS-Search.h"
#include "utl/mytchar.h"
#include <string_view>

enum
{
    MAX_LOADSTRING = 100
};

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define PACKVERSION(major,minor) MAKELONG(minor,major)


CMyHeap::HEAPBLOCK* g_FileStrings=nullptr;
CMyHeap::HEAPBLOCK* g_PathStrings=nullptr;

struct SearchResult
{
    USHORT icon;
    wchar_t* extra;
    wchar_t* filename;
    wchar_t* path;

    ULONGLONG dataSize;
    ULONGLONG allocatedSize;

};
//vectorにしてみる
SearchResult *m_pResults = nullptr;
int results_cnt = 0;
int g_buffer_num = 0;
int glSensitive = FALSE;


// Global Variables:
TCHAR szDeletedFile[ MAX_PATH ];
TCHAR szFNF[ MAX_PATH ];
TCHAR szDiskError[ MAX_LOADSTRING ];


DISKHANDLE* disks[ 32 ] = { nullptr };

//Controls

int filecompare( const void* arg1 , const void* arg2 );
int pathcompare( const void* arg1 , const void* arg2 );
int extcompare( const void* arg1 , const void* arg2 );

#define FILES 0
#define FILENAMES 1
#define DELETENOW 1
#define DELETEONREBOOT 42


MyApp theApp;
//-----------------------------------------------------------------------------------------------------------------------
MyApp::MyApp() :SxApp()
{
    m_pResults = nullptr;
    _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

    INITCOMMONCONTROLSEX init = { 0 };
    init.dwSize = sizeof( init );
    init.dwICC = ICC_WIN95_CLASSES | ICC_USEREX_CLASSES;//ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS |ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx( &init );

    //Beep(2500,150);
    return;
}
//-----------------------------------------------------------------------------------------------------------------------
BOOL MyApp::InitInstance()
{
    HINSTANCE hInstance = theApp.GetInstance();
    LoadString( hInstance , IDS_DELETEDFILE , szDeletedFile , MAX_PATH );
    LoadString( hInstance , IDS_FILENOTFOUND , szFNF , MAX_PATH );
    LoadString( hInstance , IDS_DISKERROR , szDiskError , MAX_LOADSTRING );

    return TRUE;
}
//-----------------------------------------------------------------------------------------------------------------------
int SxMain( int argc , TCHAR* argv[] )
{
    //----------------------------------------
#ifdef _DEBUG
    {
        CMyHeap::HEAPBLOCK* ptest = CMyHeap::CreateHeap( 1 );
        CMyHeap::ReUseBlocks( ptest , true );
        CMyHeap::FreeHeap( ptest );
    }
#endif


    HINSTANCE hInstance = theApp.GetInstance();

    theApp.InitInstance();
    g_FileStrings = CMyHeap::CreateHeap( 0xffff * sizeof( SearchResult ) );
    g_PathStrings = CMyHeap::CreateHeap( 0xfff * MAX_PATH );
    MyDialog dialog( nullptr , hInstance );

    dialog.DoModal( nullptr );


    CMyHeap::FreeHeap( g_PathStrings );
    CMyHeap::FreeHeap( g_FileStrings );
    //-----------
    if ( m_pResults != nullptr)
    {
        free( m_pResults );
        m_pResults = nullptr;
    }
    //-----------
    return 0;
}
//------------------------------------------------------------------------------------------------------
BOOL MyDialog::_OnInitDialog( HWND hDlg , HWND hwndFocus , LPARAM lParam )
{
    //------------------------------------------
    HICON icon;
    HINSTANCE shell = LoadLibrary( TEXT( "shell32.dll" ) );

    theApp.list = ImageList_Create( GetSystemMetrics( SM_CXSMICON ) , GetSystemMetrics( SM_CYSMICON ) , ILC_MASK | ILC_COLOR32 , 8 , 8 );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 8 ) );
    ImageList_AddIcon( theApp.list , icon );
    DestroyIcon( icon );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 9 ) );
    ImageList_AddIcon( theApp.list , icon );
    DestroyIcon( icon );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 10 ) );
    ImageList_AddIcon( theApp.list , icon );
    DestroyIcon( icon );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 5 ) );
    ImageList_AddIcon( theApp.list , icon );
    DestroyIcon( icon );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 11 ) );
    ImageList_AddIcon( theApp.list , icon );
    DestroyIcon( icon );

    m_pSxDriveListCombBox.reset( new SxDriveListCombBox( this , m_hInst ) );
    m_pSxDriveListCombBox->SetAttatch( ::GetDlgItem( hDlg , IDC_DRIVES ) );
    m_pSxDriveListCombBox->SetImageList( theApp.list );

    //------------------------------------------
    // List2
    theApp.list2 = ImageList_Create( GetSystemMetrics( SM_CXSMICON ) , GetSystemMetrics( SM_CYSMICON ) , ILC_MASK | ILC_COLOR32 , 8 , 8 );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 1 ) );
    ImageList_AddIcon( theApp.list2 , icon );
    DestroyIcon( icon );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 2 ) );
    ImageList_AddIcon( theApp.list2 , icon );
    DestroyIcon( icon );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 42 ) );
    ImageList_AddIcon( theApp.list2 , icon );
    DestroyIcon( icon );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 5 ) );
    ImageList_AddIcon( theApp.list2 , icon );
    DestroyIcon( icon );
    icon = LoadIcon( shell , MAKEINTRESOURCE( 10 ) );
    ImageList_AddIcon( theApp.list2 , icon );
    DestroyIcon( icon );

    FreeLibrary( shell );


    m_hMenuPopUp = LoadMenu( m_hInst , ( LPTSTR )IDR_MENU1 );

    m_hhm = GetSubMenu( m_hMenuPopUp , 0 );
    SetMenuDefaultItem( m_hhm , 0 , TRUE );

    //------------------------------------------
    //ここから、画面系を初期化する
    {
        m_pSxStatusBarCtr.reset( new SxStatusBarCtr( this , m_hInst ) );
        m_pSxStatusBarCtr->CreateEx( WS_EX_COMPOSITED , WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP , nullptr , RECT{ 20 , 20 , 100 , 100 } , nullptr );
        int parts[ 2 ] = { 100 , 100 };
        m_pSxStatusBarCtr->SetParts( sizeof( parts ) / sizeof( int ) , parts );
    }
    //------------------------------------------
    m_pSxList.reset( new SxList( this , m_hInst ) );
    m_pSxList->SetAttatch( ::GetDlgItem( hDlg , IDC_RESULT ) );

    m_pSxList->SetExtendedListViewStyle( LVS_EX_DOUBLEBUFFER );
    m_pSxList->SetImageList( theApp.list2 , LVSIL_SMALL );
    //----------
    LVCOLUMN col;
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;// | LVCF_IMAGE | LVCF_FMT;
    col.fmt = HDF_BITMAP_ON_RIGHT;
    col.pszText = TEXT( "Filename" );
    col.cx = 220;
    col.iSubItem = 0;

    m_pSxList->InsertColumn( 0 , &col );
    //----------
    col.pszText = TEXT( "Path" );
    col.cx = 320;
    col.iSubItem = 2;
    m_pSxList->InsertColumn( 2 , &col );
    //----------
    col.pszText = TEXT( "Extension" );
    col.cx = 60;
    col.iSubItem = 1;

    m_pSxList->InsertColumn( 1 , &col );
    //----------
    col.pszText = _T( "Size" );
    col.cx = 60;
    col.iSubItem = 3;

    m_pSxList->InsertColumn( 3 , &col );
    //----------
    col.pszText = _T( "Allocated Size" );
    col.cx = 60;
    col.iSubItem = 4;

    m_pSxList->InsertColumn( 4 , &col );
    //----------
    COMBOBOXEXITEM item = { 0 };
    item.iItem = -1;
    item.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_LPARAM | CBEIF_SELECTEDIMAGE;
    item.iImage = 0;
    item.iSelectedImage = 1;

    item.pszText = TEXT( "Use all loaded disk" );
    item.lParam = 0xff;
    m_pSxDriveListCombBox->InsertItem( &item );


    DWORD drives = GetLogicalDrives();
    for ( int index = 0; index < 32; index++ )
    {
        if ( ( ( drives >> ( index ) ) & 0x1 ) != 0u )
        {

            TCHAR str[ 5 ];
            wsprintf( str , TEXT( "%C:\\" ) , 0x41 + index );
            item.pszText = str;
            item.lParam = index;
            const UINT type = GetDriveType( str );
            if ( type == DRIVE_FIXED )
            {
                m_pSxDriveListCombBox->InsertItem( &item );
            }
        }

    }
    item.pszText = TEXT( "Load all disks" );
    item.lParam = 0xfe;
    m_pSxDriveListCombBox->InsertItem( &item );


    m_pSxDriveListCombBox->SetCurSel( 0 );

    return TRUE;
}
//-----------------------------------------------------------
LRESULT MyDialog::_OnNotify( HWND hWnd , int idFrom , NMHDR FAR* pnmhdr )
{
    if ( pnmhdr->hwndFrom != m_pSxList->m_hWnd )
    {
        return NOERROR;
    }
    //------------------------------------------
    LPNMLISTVIEW listitem;
    switch ( pnmhdr->code )
    {
        case NM_DBLCLK:
            {
                const int index = m_pSxList->GetSelectionMark();
                if ( index >= 0 )
                {
                    MyDialog::ProcessPopupMenu( hWnd , index , IDM_OPEN );
                }
                break;
            }
        case NM_RCLICK:

            break;
        case LVN_DELETEALLITEMS:
            SetLastError( 0 );
            SetWindowLongPtr( hWnd , DWLP_MSGRESULT , TRUE );
            return TRUE;
        case LVN_DELETEITEM:
            Beep( 2500 , 5 );
            break;
        case NM_RETURN:
            Beep( 2500 , 50 );
            break;
        case LVN_ODFINDITEM:
            {
                NMLVFINDITEM* finditem = ( NMLVFINDITEM* )pnmhdr;
                if ( ( finditem->lvfi.flags & LVFI_STRING ) != 0u )
                {
                    wchar_t stmp[ 2 ] , stmp2[ 2 ];
                    stmp[ 0 ] = finditem->lvfi.psz[ 0 ];
                    stmp[ 1 ] = 0;
                    stmp2[ 1 ] = 0;
                    CharLowerW( stmp );
                    int j = 0;
                    for ( int i = finditem->iStart; i != finditem->iStart - 1; i++ )
                    {
                        if ( j >= results_cnt )
                        {
                            break;
                        }
                        if ( i >= results_cnt )
                        {
                            i = 0;
                        }
                        stmp2[ 0 ] = m_pResults[ i ].filename[ 0 ];
                        CharLowerW( stmp2 );
                        if ( stmp[ 0 ] == stmp2[ 0 ] )
                        {
                            SetWindowLongPtr( hWnd , DWLP_MSGRESULT , static_cast< LONG_PTR >( i ) );
                            return TRUE;
                        }
                        j++;
                    }
                }
                SetWindowLongPtr( hWnd , DWLP_MSGRESULT , static_cast< LONG_PTR >( -1 ) );
                return TRUE;
                break;

            }
        case LVN_GETDISPINFO:
            {
                NMLVDISPINFO* info = ( NMLVDISPINFO* )pnmhdr;

                if ( info->item.iItem > -1 && info->item.iItem < results_cnt )
                {
                    SearchResult* res = &m_pResults[ info->item.iItem ];

                    if ( ( info->item.mask & LVIF_TEXT ) != 0u )
                    {
                        switch ( info->item.iSubItem )
                        {
                            case 0:
                                {
                                    const std::wstring wfilename = { res->filename };
                                    const tstring tfilename = convert_string( wfilename );
                                    _tcsncpy( info->item.pszText , tfilename.c_str() , info->item.cchTextMax );
                                    break;
                                }
                            case 1:
                                {
                                    const std::wstring wextra = { res->extra };
                                    const tstring textra = convert_string( wextra );
                                    _tcsncpy( info->item.pszText , textra.c_str() , info->item.cchTextMax );
                                    break;
                                }
                            case 2:
                                {
                                    const std::wstring wpath = { res->path };
                                    const tstring tpath = convert_string( wpath );
                                    _tcsncpy( info->item.pszText , tpath.c_str() , info->item.cchTextMax );
                                    break;
                                }
                            case 3:
                                _sntprintf( info->item.pszText , info->item.cchTextMax , TEXT( "%llu" ) , res->dataSize );
                                break;
                            case 4:
                                _sntprintf( info->item.pszText , info->item.cchTextMax , TEXT( "%llu" ) , res->allocatedSize );
                                break;
                        }
                    }

                    if ( ( info->item.mask & LVIF_IMAGE ) != 0u )
                    {
                        info->item.iImage = res->icon;
                    }
                }
                break;
            }
        case LVN_BEGINDRAG:
            listitem = ( LPNMLISTVIEW )pnmhdr;
            break;
        case LVN_COLUMNCLICK:
            listitem = ( LPNMLISTVIEW )pnmhdr;
            switch ( listitem->iSubItem )
            {
                case 0: qsort( ( void* )&m_pResults[ 0 ] , results_cnt , sizeof( SearchResult ) , filecompare ); break;
                case 1: qsort( ( void* )&m_pResults[ 0 ] , results_cnt , sizeof( SearchResult ) , extcompare ); break;
                case 2: qsort( ( void* )&m_pResults[ 0 ] , results_cnt , sizeof( SearchResult ) , pathcompare ); break;
                case 3: std::sort( m_pResults, m_pResults + results_cnt ,
                    [ ] ( const SearchResult& left , const SearchResult& right )
                    {
                        return left.dataSize > right.dataSize;
                    } );
                    break;
                case 4: std::sort( m_pResults , m_pResults + results_cnt ,
                    [ ] ( const SearchResult& left , const SearchResult& right )
                    {
                        return left.allocatedSize > right.allocatedSize;
                    } );
                    break;
            }
            InvalidateRect( listitem->hdr.hwndFrom , nullptr , TRUE );
            break;
    }
    return NOERROR;
}
//------------------------------------------------------------------------------------------------------
void MyDialog::_On_WmUser2()
{
    TCHAR tmp[ MAX_PATH ];
    COMBOBOXEXITEM item = { 0 };

    const int count = m_pSxDriveListCombBox->GetCount();

    item.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
    item.iImage = 2;
    item.iSelectedImage = 2;
    DWORD loaded = 0;
    for ( int index = 0; index < count; index++ )
    {
        const TCHAR data = ( TCHAR )m_pSxDriveListCombBox->GetItemData( index );
        if ( ( 0 < data && data < 32 ) == false )
        {
            continue;
        }
        if ( disks[ data ] != nullptr )
        {
            if ( disks[ data ]->filesCount != 0 )
            {
                wsprintf( tmp , TEXT( "%C:\\ -{loaded with %u entries}" ) , disks[ data ]->DosDevice , disks[ data ]->filesCount );
                item.iItem = index;
                item.pszText = tmp;
                m_pSxDriveListCombBox->SetItem( &item );
            }
            else
            {
                wsprintf( tmp , TEXT( "%C:\\ - {UNSUPPORTED}" ) , disks[ data ]->DosDevice );
                item.iItem = index;
                item.iImage = 4;
                item.iSelectedImage = 4;
                item.pszText = tmp;
                m_pSxDriveListCombBox->SetItem( &item );
                item.iImage = 2;
                item.iSelectedImage = 2;
            }
            loaded += disks[ data ]->realFiles;
        }
        else
        {
            wsprintf( tmp , TEXT( "%C:\\" ) , 0x41 + data );
            item.iItem = index;
            item.pszText = tmp;
            item.iImage = 0;
            item.iSelectedImage = 1;
            m_pSxDriveListCombBox->SetItem( &item );
            item.iImage = 2;
            item.iSelectedImage = 2;
        }
    }
    ::InvalidateRect( m_pSxDriveListCombBox->m_hWnd , nullptr , TRUE );
    wsprintf( tmp , TEXT( "%u entries total" ) , loaded );
    m_pSxStatusBarCtr->SetText( 1 , tmp , 0 );
    return;
}
//-----------------------------------------------------------------------------------------------------------------------
void MyDialog::_OnSize( HWND hWnd , UINT state , int cx , int cy )
{
    //--------------------------------------------
    RECT rt , rt2;
    const int width = cx;
    const int height = cy;

    ::MoveWindow( m_pSxStatusBarCtr->m_hWnd , 0 , height , width , 20 , TRUE );
    ::GetWindowRect( m_pSxStatusBarCtr->m_hWnd , &rt );
    ::GetWindowRect( m_pSxDriveListCombBox->m_hWnd , &rt2 );
    rt2.bottom += 12;
    ::MapWindowPoints( nullptr , m_hWnd , reinterpret_cast< LPPOINT >( &rt2 ) , 2 );
    ::MoveWindow( m_pSxList->m_hWnd , 0 , ( rt2.bottom ) + 20 , width , height - ( rt.bottom - rt.top ) - ( rt2.bottom + 20 ) , TRUE );
    //--------------------------------------------
    return;
}
//-----------------------------------------------------------------------------------------------------------------------
void MyDialog::_OnContextMenu( HWND hwnd , HWND hwndContext , UINT xPos , UINT yPos )
{
    if ( m_pSxList->m_hWnd != hwndContext )
    {
        return;
    }
    //--------------------------------------------
    const int index = m_pSxList->GetSelectionMark();
    if ( index < 0 )
    {
        return;
    }
    //--------------------------------------------
    DWORD dw;
    if ( xPos == -1 && yPos == -1 )
    {
        RECT rt , rt2;
        m_pSxList->GetItemRect( index , &rt , LVIR_SELECTBOUNDS );
        GetWindowRect( m_pSxList->m_hWnd , &rt2 );
        dw = m_pSxList->GetTopIndex();
        dw = index - dw;

        dw = TrackPopupMenu( m_hhm , TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD , rt2.left + rt.right , rt2.top + ( rt.bottom - rt.top ) * ( dw + 1 ) , 0 , m_hWnd , nullptr );
    }
    else
    {
        dw = TrackPopupMenu( m_hhm , TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD , xPos , yPos , 0 , m_hWnd , nullptr );
    }
    MyDialog::ProcessPopupMenu( m_hWnd , index , dw );
    return;
}
//-----------------------------------------------------------------------------------------------------------------------
void MyDialog::_OnCommand( HWND hWnd , int id , HWND hWndCtl , UINT codeNotify )
{

    if ( codeNotify == EN_SETFOCUS )
    {
        EnableWindow( GetDlgItem( hWnd , IDOK ) , TRUE );
    }
    //---------------------------
    else if ( id == IDM_EXIT )
    {
        DestroyWindow( hWnd );
        return;
    }
    //---------------------------
    if ( id == IDM_CLEAR )
    {
        m_pSxList->DeleteAllItems();
        EnableWindow( GetDlgItem( hWnd , IDOK ) , TRUE );
    }
    else if ( id == IDM_SAVE )
    {
        TCHAR tmp[ MAX_PATH ];
        OPENFILENAME of = { 0 };
        wsprintf( tmp , TEXT( "results_%d.txt" ) , results_cnt );
        of.lStructSize = sizeof( OPENFILENAME );
        of.lpstrFile = tmp;
        of.nMaxFile = MAX_PATH;
        of.lpstrFilter = TEXT( "Textfile\0*.txt\0All files\0*.*\0\0" );
        of.nFileExtension = 1;
        of.hwndOwner = hWnd;
        of.Flags = OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT;
        of.lpstrTitle = TEXT( "Save results" );
        if ( GetSaveFileName( &of ) != 0 )
        {
            if ( MyDialog::SaveResults( of.lpstrFile ) == false )
            {
                MessageBox( hWnd , TEXT( "The results couldn't be saved." ) , nullptr , MB_ICONERROR );
            }
        }
    }
    else if ( id == IDOK || id == IDOK2 )
    {
        if ( m_pSxList->m_hWnd == GetFocus() )
        {
            int index = m_pSxList->GetSelectionMark();
            if ( index >= 0 )
            {
                MyDialog::ProcessPopupMenu( hWnd , index , IDM_OPEN );
            }
        }
        else
        {
            m_pSxStatusBarCtr->SetText( 0 , TEXT( "Searching ..." ) , 0 );

            const int index = m_pSxDriveListCombBox->GetCurSel();
            TCHAR data = m_pSxDriveListCombBox->GetItemData( index );
            //--------------------------------
            std::wstring wstr;
            {
                tstring tmp( MAX_PATH + 1 , L'\0' );
                GetDlgItemText( hWnd , IDC_EDIT , &tmp.at( 0 ) , MAX_PATH );
                //--------------------------------
                wstr = convert_wstring( tmp );
            }
            //--------------------------------
            DWORD res = MyDialog::Search( hWnd , data , &wstr.at( 0 ) , false );

            TCHAR fff[ 128 ];
            wsprintf( fff , TEXT( "%u files found" ) , res );
            m_pSxStatusBarCtr->SetText( 0 , fff , 0 );
            //--------------------------------
            UINT b = IsDlgButtonChecked( hWnd , IDC_LIVE );

            if ( res > 0 && b != BST_CHECKED )
            {
                ::SetFocus( m_pSxList->m_hWnd );
                m_pSxList->SetSelectionMark( 0 );
            }
        }
        return;
    }
    else if ( id == IDC_UNLOAD && codeNotify == BN_CLICKED )
    {
        const int indexCurSel = m_pSxDriveListCombBox->GetCurSel();
        const TCHAR data = m_pSxDriveListCombBox->GetItemData( indexCurSel );
        if ( data > 0 && data < 32 )
        {
            m_pSxList->DeleteAllItems();
            CMyHeap::ReUseBlocks( g_PathStrings , false );
            MyDialog::UnloadDisk( hWnd , data );
        }
        else
        {
            m_pSxList->DeleteAllItems();
            CMyHeap::ReUseBlocks( g_PathStrings , false );

            for ( int index = 0; index < 32; index++ )
            {
                MyDialog::UnloadDisk( hWnd , index );
            }
        }
    }
    else if ( id == IDC_REFRESH )
    {
        MyDialog::ProcessLoading( hWnd , m_pSxDriveListCombBox->GetWnd() , TRUE );
    }
    else if ( id == IDC_CASE )
    {
        const UINT b = IsDlgButtonChecked( hWnd , IDC_CASE );
        if ( b == BST_CHECKED )
        {
            glSensitive = TRUE;
        }
        else
        {
            glSensitive = FALSE;
        }
    }
    else if ( codeNotify == EN_CHANGE )
    {
        const int len = SendDlgItemMessage( hWnd , id , WM_GETTEXTLENGTH , 0 , 0 );
        UINT b = IsDlgButtonChecked( hWnd , IDC_LIVE );
        EnableWindow( GetDlgItem( hWnd , IDOK ) , TRUE );

        if ( len > 2 && b == BST_CHECKED )
        {
            SendMessage( hWnd , WM_COMMAND , MAKEWPARAM( IDOK , BN_CLICKED ) , 0 );
        }
    }
    else if ( codeNotify == CBN_SELCHANGE )
    {
        MyDialog::ProcessLoading( hWnd , hWndCtl , FALSE );
    }
    if ( id == IDCANCEL )
    {
        EndDialog( hWnd , IDCANCEL );
        return;
    }
    return;
}
//-----------------------------------------------------------------------------------------------------------------------
LRESULT CALLBACK MyDialog::WindowProc( HWND hwnd , UINT uMsg , WPARAM wParam , LPARAM lParam )
{
    switch ( uMsg )
    {
        HANDLE_MSG( hwnd , WM_SIZE , _OnSize );
        HANDLE_MSG( hwnd , WM_INITDIALOG , _OnInitDialog );
        HANDLE_MSG( hwnd , WM_COMMAND , _OnCommand );
        HANDLE_MSG( hwnd , WM_NOTIFY , _OnNotify );
        HANDLE_MSG( hwnd , WM_CONTEXTMENU , _OnContextMenu );
        case WM_DESTROY:
            ImageList_Destroy( theApp.list ); theApp.list = nullptr;
            ImageList_Destroy( theApp.list2 ); theApp.list2 = nullptr;
            DestroyMenu( m_hMenuPopUp );
            ReleaseAllDisks();
            PostQuitMessage( 0 );
            break;
        case WM_USER + 2:
            _On_WmUser2();
            break;
    }
    return 0;
}
//-----------------------------------------------------------------------------------------------------------------------
int CSearchThread::Worker()
{
    STATUSINFO status = { 0 };
    status.Value = PBM_SETPOS;
    status.hWnd = GetDlgItem( m_info.hWnd , IDC_PROGRESS );
    if ( m_info.disk->filesCount == 0 )
    {
        const DWORD res = static_cast< DWORD >( static_cast< unsigned int >( CNtfsHelper::LoadMFT( m_info.disk , FALSE ) != 0 ) != 0u );
        if ( res != 0u )
        {
            SendDlgItemMessage( m_info.hWnd , IDC_PROGRESS , PBM_SETRANGE32 , 0 , m_info.disk->NTFS.entryCount );
            CNtfsHelper::ParseMFT( m_info.disk , SEARCHINFO , &status );
        }
        else if ( res == 3 )
        {
            MessageBox( m_info.hWnd , TEXT( "The filesystem is badly fragmented and can't be processed." ) , nullptr , MB_ICONERROR );
        }
    }
    else
    {
        SendDlgItemMessage( m_info.hWnd , IDC_PROGRESS , PBM_SETRANGE32 , 0 , m_info.disk->NTFS.entryCount );
        CNtfsHelper::ReparseDisk( m_info.disk , SEARCHINFO , &status );
    }
    Sleep( 800 );
    SendMessage( m_info.hWnd , WM_USER + 1 , 0 , 0 );
    return 0;
}
//-----------------------------------------------------------------------------------------------------------------------
void MyDialog::StartLoading( DISKHANDLE* disk , HWND hWnd )
{
    CWaitDialog waitDlg( this , theApp.GetInstance() );
    waitDlg.m_pPDiskHandle = disk;

    waitDlg.DoModal( this );
    return;
}
//-----------------------------------------------------------------------------------------------------------------------
LRESULT CALLBACK CWaitDialog::WindowProc( HWND hDlg , UINT message , WPARAM wParam , LPARAM lParam )
{
    UNREFERENCED_PARAMETER( lParam );
    switch ( message )
    {
        case WM_INITDIALOG:
            if ( lParam != NULL )
            {
                m_pThread.reset( new CSearchThread() );
                m_pThread->m_info.disk = m_pPDiskHandle;
                m_pThread->m_info.hWnd = m_hWnd;

                TCHAR tmp[ 256 ];
                wsprintf( tmp , TEXT( "Loading %C:\\ ... - Please wait" ) , m_pThread->m_info.disk->DosDevice );
                SetWindowText( hDlg , tmp );
                SendDlgItemMessage( hDlg , IDC_PROGRESS , PBM_GETRANGE , TRUE , NULL );

                m_pThread->Start();
            }
            return static_cast< INT_PTR >( TRUE );

        case WM_COMMAND:
            if ( LOWORD( wParam ) == IDOK || LOWORD( wParam ) == IDCANCEL )
            {
                EndDialog( hDlg , LOWORD( wParam ) );
                return static_cast< INT_PTR >( TRUE );
            }
            break;
        case WM_USER + 1:
            HWND hWnd = ::GetParent( hDlg );
            if ( hWnd != nullptr )
            {
                PostMessage( hWnd , WM_USER + 2 , 0 , 0 );
            }
            EndDialog( hDlg , 0 );
            return TRUE;
            break;
    }
    return static_cast< INT_PTR >( FALSE );
}
//-----------------------------------------------------------------------------------------------------------------------
VOID MyDialog::ReleaseAllDisks()
{
    for ( auto& disk : disks )
    {
        if ( disk != nullptr )
        {
            CNtfsHelper::CloseDisk( disk );
        }
    }
}
//-----------------------------------------------------------------------------------------------------------------------
int MyDialog::Search( HWND hWnd , int diskNo , wchar_t* filename , const bool deleted )
{
    SendMessage( m_pSxList->m_hWnd , WM_SETREDRAW , FALSE , 0 );

    m_pSxList->DeleteAllItems();
    CMyHeap::ReUseBlocks( g_PathStrings , false );
    CMyHeap::ReUseBlocks( g_FileStrings , false );

    SEARCHP* pat = StartSearch( filename , wcslen( filename ) );
    if ( pat == nullptr )
    {
        return 0;
    }
    //---------------------
    //バッファを初期化
    results_cnt = 0;
    free( m_pResults );
    g_buffer_num = 0;
    //---------------------

    int ret = 0;

    if ( diskNo > 0 && diskNo < 32 )
    {
        if ( disks[ diskNo ] != nullptr )
        {
            const int new_len = disks[ diskNo ]->filesCount; //実際のファイル数は、これよりも少ない
            m_pResults = (SearchResult*)malloc( sizeof( SearchResult ) * new_len );
            g_buffer_num = new_len;

            ret = SearchFiles( hWnd , disks[ diskNo ] , filename , deleted , pat );
        }
    }
    else
    {
        for ( auto& disk : disks )
        {
            if ( disk != nullptr )
            {
                ret += SearchFiles( hWnd , disk , filename , deleted , pat );
            }
        }
    }
    if ( ret != results_cnt )
    {
        _ASSERT( ret == results_cnt );
        DebugBreak();
    }
    results_cnt = ret;
    m_pSxList->SetItemCountEx( results_cnt , 0 );
    SendMessage( m_pSxList->m_hWnd , WM_SETREDRAW , TRUE , 0 );
    EndSearch( pat );

    return ret;
}
//-----------------------------------------------------------------------------------------------------------------------
int MyDialog::SearchFiles( HWND hWnd , DISKHANDLE* disk , wchar_t* filename , const bool deleted , SEARCHP* pat )
{
    LVITEM item = { 0 };
    if ( glSensitive == FALSE )
    {
        _wcslwr( filename );
    }
    const SEARCHFILEINFO* info = disk->fFiles;

    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;

    int hit = 0;
    bool bcontinue = false;
    for ( DWORD i = 0; i < disk->filesCount; i++ )
    {
        //削除済みファイルの場合。deleted=trueの場合は表示するのだけど、Pathがうまく取れていないので、使わないほうが良い
        if ( deleted != true && ( ( info[ i ].Flags & 0x1 ) != 0x1 ) )
        {
            continue;
        }
        if ( info[ i ].FileName == nullptr )
        {
            continue;
        }
        //----------------------------
        bool res = false;
        if ( glSensitive == FALSE )
        {
            WCHAR tmp[ 1024 * 100 ];
            memcpy( tmp , info[ i ].FileName , info[ i ].FileNameLength * sizeof( WCHAR ) + 2 );
            _wcslwr( tmp );
            res = SearchStr( pat , info[ i ].FileName , info[ i ].FileNameLength );
        }
        else
        {
            res = SearchStr( pat , const_cast< wchar_t* >( info[ i ].FileName ) , info[ i ].FileNameLength );
        }
        //----------------------------
        if ( res == false )
        {
            continue;
        }
        //----------------------------
        if ( g_buffer_num <= results_cnt )
        {
            int new_len = results_cnt + 0xffff;
            SearchResult * pNewBuffer = (SearchResult*)malloc( sizeof( SearchResult ) * new_len );
            RtlCopyMemory( pNewBuffer , m_pResults , sizeof( SearchResult ) * results_cnt );
            free( m_pResults );
            m_pResults = pNewBuffer;
            g_buffer_num = new_len;
        }
        SearchResult* pSearchResult = &m_pResults[ results_cnt++ ];
        STRSAFE_LPWSTR t = CNtfsHelper::GetPath( disk , i );
        int s = wcslen( t );
        pSearchResult->filename = const_cast< wchar_t* >( info[ i ].FileName );
        pSearchResult->path = CMyHeap::AllocAndCopyString( g_PathStrings , t , s );
        pSearchResult->icon = info[ i ].Flags;

        pSearchResult->dataSize = info[ i ].DataSize;
        pSearchResult->allocatedSize = info[ i ].AllocatedSize;

        if ( ( info[ i ].Flags & 0x002 ) == 0 )
        {
            wchar_t* ret = wcsrchr( pSearchResult->filename , L'.' );
            if ( ret != nullptr )
            {
                pSearchResult->extra = ret + 1;
            }
            else
            {
                pSearchResult->extra = L" ";
            }
        }
        else
        {
            pSearchResult->extra = L" ";
        }
        auto last = m_pSxList->InsertItem( &item );

        hit++;
#ifdef _DEBUG
        if ( bcontinue == false && results_cnt > 0xfff9 )
        {
            const int ret = MessageBox( hWnd , TEXT( "Your search produces too many results!" ) , nullptr , MB_ICONWARNING | MB_OKCANCEL );
            if ( ret == IDCANCEL )
            {
                break;
            }
            bcontinue = true;
        }
#endif
    }
    return hit;
}
//-----------------------------------------------------------------------------------------------------------------------
UINT MyDialog::ExecuteFile( HWND hWnd , STRSAFE_LPWSTR str , const USHORT flags )
{
    if ( ( flags & 0x001 ) == 0 )
    {
        MessageBox( hWnd , szDeletedFile , nullptr , MB_ICONWARNING );
        return 0;
    }
    SHELLEXECUTEINFOW shell = { 0 };
    shell.cbSize = sizeof( SHELLEXECUTEINFO );
    shell.lpFile = str;
    shell.fMask = SEE_MASK_INVOKEIDLIST;
    shell.lpVerb = nullptr;
    shell.nShow = SW_SHOWDEFAULT;

    ::ShellExecuteExW( &shell );
    UINT res = ( UINT )shell.hInstApp;
    switch ( res )
    {
        case SE_ERR_NOASSOC:
            ShellExecuteW( nullptr , L"openas" , str , nullptr , nullptr , SW_SHOWDEFAULT );
            break;
        case SE_ERR_ASSOCINCOMPLETE:

            break;
        case SE_ERR_ACCESSDENIED:
            MessageBox( hWnd , TEXT( "Access denied!" ) , nullptr , MB_ICONERROR );
            break;
        case ERROR_PATH_NOT_FOUND:
            //MessageBox(hWnd, TEXT("The path coulnd't be found.\nPropably the path is hidden."), 0, MB_ICONWARNING);
            break;
        case ERROR_FILE_NOT_FOUND:
            //MessageBox(hWnd, szFNF/*TEXT("The file coulnd't be found.\nThe file is propably hidden or a metafile.")*/, 0, MB_ICONERROR);
            break;
        case ERROR_BAD_FORMAT:
            //MessageBox(hWnd, TEXT("This is not a valid Win32 Executable File."), 0, MB_ICONINFORMATION);
            break;
        case SE_ERR_DLLNOTFOUND:

            break;
        default:
            //if (res>32)

            break;
    }

    return res;
}
//-----------------------------------------------------------------------------------------------------------------------
bool MyDialog::UnloadDisk( HWND hWnd , int index )
{
    if ( disks[ index ] != nullptr )
    {
        CNtfsHelper::CloseDisk( disks[ index ] );
        disks[ index ] = nullptr;
        SendMessage( hWnd , WM_USER + 2 , 0 , 0 );
        return true;
    }
    return false;
}
//-----------------------------------------------------------------------------------------------------------------------
UINT MyDialog::ExecuteFileEx( HWND hWnd , const wchar_t* command , LPCWSTR str , LPCWSTR dir , const UINT show , const USHORT flags )
{
    if ( ( flags & 0x001 ) == 0 )
    {
        MessageBox( hWnd , szDeletedFile/*TEXT("The file is deleted and cannot be accessed throught the filesystem driver.\nUse a recover program to get access to the stored data.")*/ , nullptr , MB_ICONWARNING );
        return 0;
    }

    SHELLEXECUTEINFOW shell = { 0 };
    shell.cbSize = sizeof( SHELLEXECUTEINFO );
    shell.lpFile = str;
    shell.fMask = SEE_MASK_INVOKEIDLIST;
    shell.lpVerb = command;
    shell.nShow = show;
    shell.lpDirectory = dir;

    ShellExecuteExW( &shell );
    UINT res = ( UINT )shell.hInstApp;
    switch ( res )
    {
        case SE_ERR_NOASSOC:
            ShellExecuteW( nullptr , L"openas" , str , nullptr , nullptr , SW_SHOWDEFAULT );
            break;
        case SE_ERR_ASSOCINCOMPLETE:

            break;
        case SE_ERR_ACCESSDENIED:
            MessageBox( hWnd , TEXT( "Access denied!" ) , nullptr , MB_ICONERROR );
            break;
        case ERROR_PATH_NOT_FOUND:
            //MessageBox(hWnd, TEXT("The path coulnd't be found.\nPropably the path is hidden."), 0, MB_ICONWARNING);
            break;
        case ERROR_FILE_NOT_FOUND:
            //MessageBox(hWnd, TEXT("The file coulnd't be found.\nThe file is propably hidden or a metafile."), 0, MB_ICONERROR);
            break;
        case ERROR_BAD_FORMAT:
            //MessageBox(hWnd, TEXT("This is not a valid Win32 Executable File."), 0, MB_ICONINFORMATION);
            break;
        case SE_ERR_DLLNOTFOUND:

            break;
        default:
            //if (res>32)

            break;
    }

    return res;
}
//-----------------------------------------------------------------------------------------------------------------------
int filecompare( const void* arg1 , const void* arg2 )
{
    const SearchResult* pLeft = ( SearchResult* )arg1;
    const SearchResult* pRight = ( SearchResult* )arg2;

    // Compare all of both strings:
    return _wcsicmp( pLeft->filename , pRight->filename );
}
//-----------------------------------------------------------------------------------------------------------------------
int pathcompare( const void* arg1 , const void* arg2 )
{
    const SearchResult* pLeft = ( SearchResult* )arg1;
    const SearchResult* pRight = ( SearchResult* )arg2;

    // Compare all of both strings:
    return _wcsicmp( pLeft->path , pRight->path );
}
//-----------------------------------------------------------------------------------------------------------------------
int extcompare( const void* arg1 , const void* arg2 )
{
    const SearchResult* pLeft = ( SearchResult* )arg1;
    const SearchResult* pRight = ( SearchResult* )arg2;

    // Compare all of both strings:
    return _wcsicmp( pLeft->extra , pRight->extra );
}
//-----------------------------------------------------------------------------------------------------------------------
bool MyDialog::SaveResults( const TCHAR* filename )
{
    HANDLE file = CreateFile( filename , GENERIC_WRITE , 0 , nullptr , CREATE_ALWAYS , FILE_FLAG_SEQUENTIAL_SCAN , nullptr );

    if ( file == INVALID_HANDLE_VALUE )
    {
        return FALSE;
    }

    bool error = false;
    for ( int i = 0; i < results_cnt; i++ )
    {
        std::wstring buff;
        buff.reserve( 2048 );
        buff = m_pResults[ i ].path;
        buff = buff + m_pResults[ i ].filename + L"\r\n";
        DWORD written;
        if ( WriteFile( file , buff.c_str() , buff.size() * sizeof( TCHAR ) , &written , nullptr ) != TRUE )
        {
            error = true;
        }
    }

    CloseHandle( file );
    if ( error )
    {
        return false;
    }
    return true;
}
//-----------------------------------------------------------------------------------------------------------------------
BOOL MyDialog::ProcessPopupMenu( HWND hWnd , int index , DWORD item )
{
    wchar_t path[ 0xffff ];
    wcscpy( path , m_pResults[ index ].path );
    size_t len = wcslen( path );
    path[ len ] = 0;

    switch ( item )
    {
        case IDM_OPEN:
            wcscpy( &path[ len ] , m_pResults[ index ].filename );
            len += wcslen( path );
            path[ len ] = 0;
            MyDialog::ExecuteFile( hWnd , path , m_pResults[ index ].icon );
            break;
        case IDM_OPENMIN:
            wcscpy( &path[ len ] , m_pResults[ index ].filename );
            len += wcslen( path );
            path[ len ] = 0;
            MyDialog::ExecuteFileEx( hWnd , nullptr , path , m_pResults[ index ].path , SW_MINIMIZE , m_pResults[ index ].icon );

            break;
        case IDM_OPENWITH:
            wcscpy( &path[ len ] , m_pResults[ index ].filename );
            len += wcslen( path );
            path[ len ] = 0;
            MyDialog::ExecuteFileEx( hWnd , L"openas" , path , m_pResults[ index ].path , SW_SHOWDEFAULT , m_pResults[ index ].icon );

            break;
        case IDM_OPENDIR:
            MyDialog::ExecuteFileEx( hWnd , L"explore" , path , m_pResults[ index ].path , SW_SHOWDEFAULT , m_pResults[ index ].icon );
            break;
        case ID_CMDPROMPT:

            MyDialog::ExecuteFileEx( hWnd , L"open" , L"cmd.exe" , m_pResults[ index ].path , SW_SHOWDEFAULT , m_pResults[ index ].icon );
            break;
        case IDM_PROPERTIES:
            wcscpy( &path[ len ] , m_pResults[ index ].filename );
            len += wcslen( path );
            path[ len ] = 0;
            MyDialog::ExecuteFileEx( hWnd , L"properties" , path , m_pResults[ index ].path , SW_SHOWDEFAULT , m_pResults[ index ].icon );

            break;
        case ID_COPY:
            PrepareCopy( hWnd , FILES );
            break;
        case ID_COPY_NAMES:
            PrepareCopy( hWnd , FILENAMES );
            break;
        case ID_DELETE:
            DeleteFiles( hWnd , DELETENOW );
            break;
        case ID_DELETE_ON_REBOOT:
            DeleteFiles( hWnd , DELETEONREBOOT );
            break;
        default:

            break;
    }

    return TRUE;
}
//-----------------------------------------------------------------------------------------------------------------------
void MyDialog::PrepareCopy( HWND hWnd , UINT flags )
{
    int newline;
    int structsize;

    switch ( flags )
    {
        case FILES:
            newline = 0;
            structsize = sizeof( DROPFILES );
            break;
        case FILENAMES:
        default:
            newline = L'\n';
            structsize = 0;
            break;
    }
    //-----------------------------
    DROPFILES files = { 0 };
    files.fNC = TRUE;
    files.pt.x = 0;
    files.pt.y = 0;
    files.pFiles = sizeof( files );
    files.fWide = TRUE;

    DWORD datasize = 0;
    wchar_t* buff = static_cast< wchar_t* >( malloc( 0x100000 ) );
    int len;
    for ( int i = 0; i < results_cnt; i++ )
    {
        UINT mask = m_pSxList->GetItemState( i , LVIS_SELECTED );
        if ( ( ( mask & LVIS_SELECTED ) != 0u ) && ( ( m_pResults[ i ].icon & 0x001 ) != 0 ) )
        {
            wcscpy( &buff[ datasize ] , m_pResults[ i ].path );
            len = wcslen( &buff[ datasize ] );
            wcscpy( &buff[ datasize + len ] , m_pResults[ i ].filename );
            len += wcslen( &buff[ datasize + len ] );

            if ( flags == FILES )
            {
                buff[ datasize + len ] = 0;
            }
            else
            {
                buff[ datasize + len ] = L'\r';
                buff[ datasize + len + 1 ] = L'\n';
                datasize += 1;
            }
            datasize += len + 1;
            if ( datasize > 0x7fff0 )
            {
                break;
            }
        }
    }
    buff[ datasize ] = 0;
    buff[ datasize++ ] = 0;

    HANDLE hdrop = GlobalAlloc( 0 , structsize + datasize * sizeof( TCHAR ) );

    PVOID ptr = GlobalLock( hdrop );
    CopyMemory( ptr , &files , structsize );
    CopyMemory( PUCHAR( ptr ) + structsize , buff , datasize * sizeof( TCHAR ) );
    GlobalUnlock( hdrop );

    if ( OpenClipboard( hWnd ) != 0 )
    {
        if ( EmptyClipboard() != 0 )
        {
            if ( flags == FILES )
            {
                if ( SetClipboardData( CF_HDROP , hdrop ) == nullptr )
                {
                    GlobalFree( hdrop );
                }
            }
            else
            {
                if ( SetClipboardData( CF_UNICODETEXT , hdrop ) == nullptr )
                {
                    GlobalFree( hdrop );
                }
            }
        }
        else
        {
            GlobalFree( hdrop );
        }

        CloseClipboard();
    }
    else
    {
        GlobalFree( hdrop );
    }

    free( &buff[ 0 ] );
}
//-----------------------------------------------------------------------------------------------------------------------
void MyDialog::DeleteFiles( HWND hWnd , UINT flags )
{
    wchar_t buff[ 1024 ];

    if ( flags == DELETENOW )
    {
        for ( int index = 0; index < results_cnt; index++ )
        {
            const UINT mask = m_pSxList->GetItemState( index , LVIS_SELECTED );
            if ( ( mask & LVIS_SELECTED ) != LVIS_SELECTED )
            {
                continue;
            }
            if ( ( m_pResults[ index ].icon & 0x001 ) == 0x001 )
            {
                wchar_t wpath[ 1024 ] = { 0 };
                wcscpy( wpath , m_pResults[ index ].path );
                int len = wcslen( wpath );
                wcscpy( &wpath[ len ] , m_pResults[ index ].filename );
                len += wcslen( &wpath[ len ] );
                wpath[ len ] = 0;

                wsprintfW( buff , L"Are you sure you want to delete\n\n%.1024s\n\nfrom disk?\nYou can't restore this file!" , wpath );
                DWORD res = MessageBoxW( hWnd , buff , L"WARNING" , MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON2 );
                if ( res == IDYES )
                {
                    if ( !DeleteFileW( wpath ) )
                    {
                        MyDialog::ShowError();
                    }
                }
                else if ( res == IDCANCEL )
                {
                    break;
                }
            }
        }
        return;
    }
    else if ( flags == DELETEONREBOOT )
    {
        for ( int index = 0; index < results_cnt; index++ )
        {
            const UINT mask = m_pSxList->GetItemState( index , LVIS_SELECTED );
            if ( ( mask & LVIS_SELECTED ) != LVIS_SELECTED )
            {
                continue;
            }
            if ( ( m_pResults[ index ].icon & 0x001 ) == 0x001 )
            {
                wchar_t wpath[ 1024 ] = { 0 };
                wcscpy( wpath , m_pResults[ index ].path );
                int len = wcslen( wpath );
                wcscpy( &wpath[ len ] , m_pResults[ index ].filename );
                len += wcslen( &wpath[ len ] );
                wpath[ len ] = 0;

                wsprintfW( buff , L"Warning!!!\nThis is very dangerous - you try to delete a file on reboot - this may cause damage to the system.\nYour system might be unaccessable afterwards.\n\nAre you sure you want to delete\n\n%.1024s\n\nfrom disk?\nYou can't restore this file!" , wpath );
                DWORD res = MessageBoxW( hWnd , buff , L"WARNING" , MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON2 );
                if ( res == IDYES )
                {
                    if ( !MoveFileExW( wpath , nullptr , MOVEFILE_DELAY_UNTIL_REBOOT ) )
                    {
                        MyDialog::ShowError();
                    }
                }
                else if ( res == IDCANCEL )
                {
                    break;
                }
            }
        }
    }
    return;
}

DWORD MyDialog::ShowError()
{
    // Retrieve the system error message for the last-error code

    DWORD dw = GetLastError();

    LPVOID lpMsgBuf=NULL;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS ,
        nullptr ,
        dw ,
        MAKELANGID( LANG_NEUTRAL , SUBLANG_DEFAULT ) ,
        reinterpret_cast< LPTSTR >( &lpMsgBuf ) ,
        0 , nullptr );
    MessageBox( nullptr , static_cast< LPCTSTR >( lpMsgBuf ) , nullptr , MB_OK | MB_ICONINFORMATION );

    LocalFree( lpMsgBuf );
    return dw;
}

int MyDialog::ProcessLoading( HWND hWnd , HWND hCombo , int reload )
{
    EnableWindow( GetDlgItem( hWnd , IDOK ) , TRUE );
    int data = SendMessage( hCombo , CB_GETITEMDATA , SendMessage( hCombo , CB_GETCURSEL , 0 , 0 ) , 0 );

    if ( 0 < data && data < 32 )
    {
        int b = IsDlgButtonChecked( hWnd , IDC_LOADALWAYS );

        if ( disks[ data ] == nullptr )
        {
            disks[ data ] = CNtfsHelper::OpenDisk( 0x41 + data );
            if ( disks[ data ] != nullptr )
            {
                StartLoading( disks[ data ] , hWnd );
            }
            else
            {
                MessageBox( hWnd , szDiskError , nullptr , MB_ICONINFORMATION );
                // set different icon
            }
        }
        else if ( b == BST_CHECKED || reload > 0 )
        {
            StartLoading( disks[ data ] , hWnd );
        }

    }
    else if ( data == 0xfe )
    {
        const DWORD drives = GetLogicalDrives();

        for ( int index = 0; index < 32; index++ )
        {
            if ( ( ( drives >> ( index ) ) & 0x1 ) != 0u )
            {
                TCHAR str[ 5 ];
                wsprintf( str , TEXT( "%C:\\" ) , 0x41 + index );
                const UINT type = GetDriveType( str );
                if ( type != DRIVE_FIXED )
                {
                    continue;
                }
                //---------------------------
                //
                if ( disks[ index ] != nullptr )
                {
                    continue;
                }
                //---------------------------
                disks[ index ] = CNtfsHelper::OpenDisk( 0x41 + index );
                if ( disks[ index ] != nullptr )
                {
                    StartLoading( disks[ index ] , hWnd );
                }
                else
                {
                    // set different icon
                }
            }

        }

        SendMessage( hCombo , CB_SETCURSEL , 0 , 0 );

    }
    else if ( reload > 0 )
    {
        for ( auto& disk : disks )
        {
            if ( disk != nullptr )
            {
                MyDialog::StartLoading( disk , hWnd );
            }
        }
    }
    SetFocus( GetDlgItem( hWnd , IDC_EDIT ) );
    return 0;
}
//-----------------------------------------------------------------------------------------------------------------------


