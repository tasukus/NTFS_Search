// Simple- really

struct SEARCHP
{
    unsigned int mode;
    wchar_t* string;
    int len;
    wchar_t* extra;
    int extralen;
    int totallen;
};

SEARCHP* StartSearch( wchar_t* string , int len );
bool SearchStr( const SEARCHP* pattern , const wchar_t* string , const int len );
void EndSearch( SEARCHP* pattern );

