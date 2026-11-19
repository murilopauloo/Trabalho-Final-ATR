//
//	Programação Multithreaded em ambiente Windows NT© - uma visão de  Automação
// 
//	Autores: Constantino Seixas Filho/ Marcelo Szuster
//
//	Capítulo 2 - CheckForError.h
//
//	Versão: 1.1	20/01/1999
//

#pragma once

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment( lib, "USER32" )

#define CheckForError(ret) if (!(ret)) CheckReturn(__FILE__, __LINE__)

__inline void CheckReturn(LPCSTR FileName, int Line)
{
#ifdef _CHECKERROR
	DWORD	ErrorCode = GetLastError();
	LPSTR	lpMsgBuff = NULL;
	char	OutBuffer[1024];

	// Traduz retorno de GetLastError em um string !!!
	// A versao ANSI mantem a mensagem em char mesmo nos projetos Unicode.
	DWORD MessageLength = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
				| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				ErrorCode,
				LANG_NEUTRAL,
				(LPSTR)&lpMsgBuff,  // Aloca o buffer dinamicamente
				0,
				NULL);
	if (MessageLength != 0) {
		_snprintf_s(OutBuffer, sizeof(OutBuffer), _TRUNCATE,
			"\nErro: %s Arquivo %s Linha %d\n", lpMsgBuff, FileName, Line);
	}
	else {
		_snprintf_s(OutBuffer, sizeof(OutBuffer), _TRUNCATE,
			"\nErro: codigo %lu Arquivo %s Linha %d\n", ErrorCode, FileName, Line);
	}
#ifndef	_WINDOWS // Modo Console
	printf("%s", OutBuffer);
#else			 // Modo Windows
	MessageBoxA(NULL, OutBuffer, "ERRO", MB_ICONWARNING|MB_OK|MB_TASKMODAL|MB_SETFOREGROUND);
#endif
	if (lpMsgBuff != NULL) {
		LocalFree(lpMsgBuff);
	}
	exit(EXIT_FAILURE);
#endif

}

