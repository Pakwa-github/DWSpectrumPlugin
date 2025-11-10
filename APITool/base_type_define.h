
#ifndef __BASE_TYPE_DEFINE_
#define __BASE_TYPE_DEFINE_

#ifndef WINVER                          // Specifies that the minimum required platform is Windows Vista.
#define WINVER 0x0600           // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT            // Specifies that the minimum required platform is Windows Vista.
#define _WIN32_WINNT 0x0600     // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINDOWS          // Specifies that the minimum required platform is Windows 98.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE                       // Specifies that the minimum required platform is Internet Explorer 7.0.
#define _WIN32_IE 0x0700        // Change this to the appropriate value to target other versions of IE.
#endif


#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <gdiplus.h>
#include <MMSystem.h>
#include <winsock2.h>
#include <mmreg.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(min)
#undef min 
#endif

#if defined(max)
#undef max 
#endif

#ifndef _DEBUG
#define DEBUG_NEW  new
#endif

#define  snprintf _snprintf 

typedef char			tchar, tint8;/////8bit signed				//命名前缀:c
typedef unsigned char	tbyte, tuchar, tuint8; ///////8bit unsigned	//命名前缀:uc
typedef short			tint16; ////16bit  signed					//命名前缀:s
typedef unsigned short  tuint16; ////16bit  unsigned				//命名前缀:us
typedef int				tint32; /////32 bit  signed					//命名前缀:n
typedef unsigned int	tuint32; /////32 bit unsigned				//命名前缀:dw
typedef __int64			tint64; //////64bit  signed					//命名前缀:ll
typedef unsigned __int64   tuint64; ////64bit  unsigned				//命名前缀:ull

/*
char szName[128](以0结束的字符串)	//命名前缀:sz
FILETIME ftCurTime;	//命名前缀:ft

按位定义的表达:  前缀是类型, 后缀是bit数, xb, 如: tuint32 dwXXX6b:6; 

std::string strName;	//命名前缀:str
HAND hXXX;				//命名前缀:h
std::list ChannelNodeList; 	//后缀加List
std::map ChNodeMap;		//后缀加Map
std::vector ChNodeVec;	//后缀加Vec
std::set ChNodeSet;		//后缀加Set
*/


#define TUINT32_MAX 0xFFFFFFFF
#define TUINT64_MAX 0xFFFFFFFFFFFFFFFF

/************************************************************************************
 *基本类型的扩展类型
************************************************************************************/
#if defined(OS_IS_64BIT)
    typedef tuint64     tuint_ptr;		//命名前缀:ptr
    typedef tint64      tint_ptr;		//命名前缀:ptr
#else
    typedef tuint32     tuint_ptr;		//命名前缀:ptr
    typedef tint32      tint_ptr;		//命名前缀:ptr
#endif /////

__inline bool operator < (const GUID& guidOne, const GUID& guidOther)
{
	return (memcmp(&guidOne, &guidOther, sizeof(GUID)) < 0);
}

__inline bool operator > (const GUID& guidOne, const GUID& guidOther)
{
	return (memcmp(&guidOne, &guidOther, sizeof(GUID)) > 0);
}


typedef DWORD TRHEAD_RETVALUE;

const FILETIME ZERO_FILE_TIME={0,0};
__inline bool operator < (const FILETIME &leftTime, const FILETIME &rightTime)
{
	const ULONGLONG *pLeftTime = (const ULONGLONG*)&leftTime;
	const ULONGLONG *pRightTime = (const ULONGLONG*)&rightTime;
	return ((*pLeftTime) < (*pRightTime));
}

__inline bool operator > (const FILETIME &leftTime, const FILETIME &rightTime)
{
	const ULONGLONG *pLeftTime = (const ULONGLONG*)&leftTime;
	const ULONGLONG *pRightTime = (const ULONGLONG*)&rightTime;
	return ((*pLeftTime) > (*pRightTime));
}

__inline bool operator == (const FILETIME &leftTime, const FILETIME &rightTime)
{
	return ((leftTime.dwHighDateTime== rightTime.dwHighDateTime) && 
		(leftTime.dwLowDateTime==rightTime.dwLowDateTime));
}

__inline bool operator != (const FILETIME &leftTime, const FILETIME &rightTime)
{
	return !(leftTime == rightTime);
}

__inline FILETIME & operator += (FILETIME &ftTime, const ULONGLONG &ullTime)
{
	ULONGLONG *pftTime = (ULONGLONG *)(&ftTime);
	*pftTime += ullTime;
	return ftTime;
}

__inline FILETIME & operator -= (FILETIME &ftTime, const ULONGLONG &ullTime)
{
	ULONGLONG *pftTime = (ULONGLONG *)(&ftTime);
	*pftTime -= ullTime;
	return ftTime;
}

__inline LONGLONG operator - (const FILETIME &ftTime1, const FILETIME &ftTime2)
{
	const LONGLONG *pllTime1 = (const LONGLONG *)(&ftTime1);
	const LONGLONG *pllTime2 = (const LONGLONG *)(&ftTime2);

	return *pllTime1 - *pllTime2;
}

#define        UNREFERENCED_PARAMETER(P)     (P)
#define  NULL_PTR     0 
#endif ///////#ifndef __BASE_TYPE_DEFINE_


