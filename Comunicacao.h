#pragma once

#include <iostream>
#include <string>
#include "CheckForError.h"

const wchar_t* const MAILSLOT_EXIBICAO = L"\\\\.\\mailslot\\MailExibicao";
const wchar_t* const MAILSLOT_GESTAO = L"\\\\.\\mailslot\\MailGestao";

inline HANDLE CriarMailslotEntrada(const wchar_t* nome) {
    HANDLE mailslot = CreateMailslotW(nome, 512, 0, nullptr);
    CheckForError(mailslot != INVALID_HANDLE_VALUE);
    return mailslot;
}

// Consulta primeiro para nao bloquear a leitura do teclado e dos eventos.
inline bool LerMensagemMailslot(HANDLE mailslot, std::string& mensagem) {
    DWORD tamanho = 0;
    CheckForError(GetMailslotInfo(mailslot, nullptr, &tamanho, nullptr, nullptr));
    if (tamanho == MAILSLOT_NO_MESSAGE) {
        return false;
    }

    std::string buffer(tamanho + 1, '\0');
    OVERLAPPED operacao = {};
    operacao.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    CheckForError(operacao.hEvent);

    DWORD bytesLidos = 0;
    BOOL sucesso = ReadFile(mailslot, &buffer[0], tamanho, &bytesLidos, &operacao);
    if (!sucesso) {
        CheckForError(GetLastError() == ERROR_IO_PENDING);
    }
    CheckForError(GetOverlappedResult(mailslot, &operacao, &bytesLidos, TRUE));
    CheckForError(CloseHandle(operacao.hEvent));

    mensagem.assign(buffer.data(), bytesLidos);
    if (!mensagem.empty() && mensagem.back() == '\0') {
        mensagem.pop_back();
    }
    return true;
}

// A mensagem LIMPAR apaga a tela e o historico, sem descartar dados pendentes.
inline void LimparConsole() {
    std::cout.flush();
    // CONOUT$ garante permissao de leitura para consultar o modo do console.
    HANDLE console = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    CheckForError(console != INVALID_HANDLE_VALUE);

    DWORD modoOriginal = 0;
    CheckForError(GetConsoleMode(console, &modoOriginal));
    if (SetConsoleMode(console, modoOriginal | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        const wchar_t sequencia[] = L"\x1b[2J\x1b[3J\x1b[H";
        DWORD escritos = 0;
        BOOL sucesso = WriteConsoleW(console, sequencia,
            static_cast<DWORD>(sizeof(sequencia) / sizeof(wchar_t) - 1), &escritos, nullptr);
        DWORD erro = sucesso ? ERROR_SUCCESS : GetLastError();
        CheckForError(SetConsoleMode(console, modoOriginal));
        if (!sucesso) {
            SetLastError(erro);
        }
        CheckForError(sucesso);
        CheckForError(CloseHandle(console));
        return;
    }

    // Compatibilidade com consoles antigos sem sequencias de terminal virtual.
    CONSOLE_SCREEN_BUFFER_INFO informacoes = {};
    CheckForError(GetConsoleScreenBufferInfo(console, &informacoes));
    DWORD celulas = static_cast<DWORD>(informacoes.dwSize.X) * informacoes.dwSize.Y;
    DWORD escritos = 0;
    COORD inicio = { 0, 0 };
    CheckForError(FillConsoleOutputCharacterW(console, L' ', celulas, inicio, &escritos));
    CheckForError(FillConsoleOutputAttribute(console, informacoes.wAttributes,
        celulas, inicio, &escritos));
    CheckForError(SetConsoleCursorPosition(console, inicio));
    CheckForError(CloseHandle(console));
}
