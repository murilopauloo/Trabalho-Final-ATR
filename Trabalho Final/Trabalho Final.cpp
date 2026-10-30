// main.cpp - Processo Principal

#include <windows.h>
#include <iostream>
#include <string>
#include <conio.h>

using namespace std;

// ============================================================
// THREADS
// ============================================================

// Thread de leitura do Supervisorio
DWORD WINAPI LeituraSupervisorio(LPVOID parametro) {

    HANDLE eventoEncerramento = (HANDLE)parametro;

    while (WaitForSingleObject(eventoEncerramento, 0) == WAIT_TIMEOUT) {

        // TODO: Gerar mensagens do Supervisorio
        // TODO: Inserir mensagens na lista circular

        Sleep(1000);
    }

    return 0;
}

// Thread de leitura do PCP
DWORD WINAPI LeituraPCP(LPVOID parametro) {

    HANDLE eventoEncerramento = (HANDLE)parametro;

    while (WaitForSingleObject(eventoEncerramento, 0) == WAIT_TIMEOUT) {

        // TODO: Gerar mensagens do PCP
        // TODO: Inserir mensagens na lista circular

        Sleep(1000);
    }

    return 0;
}

// Thread de retirada de mensagens
DWORD WINAPI RetiradaMensagens(LPVOID parametro) {

    HANDLE eventoEncerramento = (HANDLE)parametro;

    while (WaitForSingleObject(eventoEncerramento, 0) == WAIT_TIMEOUT) {

        // TODO: Retirar mensagens da lista circular
        // TODO: Exibir mensagens no console principal

        Sleep(100);
    }

    return 0;
}

// ============================================================
// PROCESSO PRINCIPAL
// ============================================================

int main() {

    // --------------------------------------------------------
    // 1. DECLARACAO DOS PROCESSOS
    // --------------------------------------------------------

    STARTUPINFOW siExibicao = {};
    STARTUPINFOW siGestao = {};

    PROCESS_INFORMATION piExibicao = {};
    PROCESS_INFORMATION piGestao = {};

    siExibicao.cb = sizeof(siExibicao);
    siGestao.cb = sizeof(siGestao);

    // --------------------------------------------------------
    // 2. CRIACAO DO EVENTO DE ENCERRAMENTO
    // --------------------------------------------------------

    HANDLE eventoEncerramento = CreateEventW(
        nullptr,
        TRUE,
        FALSE,
        L"Local\\ELT127_Encerramento"
    );

    if (eventoEncerramento == NULL) {
        cout << "Erro ao criar evento de encerramento." << endl;
        return 1;
    }

    // --------------------------------------------------------
    // 3. CRIACAO DOS PROCESSOS
    // --------------------------------------------------------

    // Processo de Exibicao dos Dados de Processo
    BOOL processoExibicao = CreateProcessW(
        L"ExibicaoProcesso.exe",
        nullptr,
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &siExibicao,
        &piExibicao
    );

    if (!processoExibicao) {
        cout << "Erro ao criar processo de exibicao: "
            << GetLastError() << endl;
    }

    // Processo de Gestao da Producao
    BOOL processoGestao = CreateProcessW(
        L"GestaoProducao.exe",
        nullptr,
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &siGestao,
        &piGestao
    );

    if (!processoGestao) {
        cout << "Erro ao criar processo de gestao: "
            << GetLastError() << endl;
    }

    // --------------------------------------------------------
    // 4. CRIACAO DAS THREADS
    // --------------------------------------------------------

    HANDLE hThreadSupervisorio = CreateThread(
        nullptr,
        0,
        LeituraSupervisorio,
        eventoEncerramento,
        0,
        nullptr
    );

    HANDLE hThreadPCP = CreateThread(
        nullptr,
        0,
        LeituraPCP,
        eventoEncerramento,
        0,
        nullptr
    );

    HANDLE hThreadRetirada = CreateThread(
        nullptr,
        0,
        RetiradaMensagens,
        eventoEncerramento,
        0,
        nullptr
    );

    if (!hThreadSupervisorio || !hThreadPCP || !hThreadRetirada) {
        cout << "Erro ao criar uma ou mais threads." << endl;
        SetEvent(eventoEncerramento);
    }

    // --------------------------------------------------------
    // 5. LEITURA DO TECLADO
    // --------------------------------------------------------

    cout << "======================================" << endl;
    cout << " SISTEMA DE MONITORAMENTO INDUSTRIAL " << endl;
    cout << "======================================" << endl;

    cout << "\nComandos disponiveis:\n" << endl;
    cout << "s - Bloquear/Desbloquear Supervisorio" << endl;
    cout << "m - Bloquear/Desbloquear PCP" << endl;
    cout << "r - Bloquear/Desbloquear Retirada" << endl;
    cout << "p - Bloquear/Desbloquear Exibicao" << endl;
    cout << "g - Bloquear/Desbloquear Gestao" << endl;
    cout << "1 - Limpar console de processo" << endl;
    cout << "2 - Limpar console de gestao" << endl;
    cout << "ESC - Encerrar aplicacao" << endl;

    bool executando = true;

    while (executando &&
        hThreadSupervisorio &&
        hThreadPCP &&
        hThreadRetirada) {

        int tecla = _getch();

        switch (tecla) {

        case 's':
            // TODO: Bloquear/Desbloquear Supervisorio
            break;

        case 'm':
            // TODO: Bloquear/Desbloquear PCP
            break;

        case 'r':
            // TODO: Bloquear/Desbloquear Retirada
            break;

        case 'p':
            // TODO: Bloquear/Desbloquear Exibicao
            break;

        case 'g':
            // TODO: Bloquear/Desbloquear Gestao
            break;

        case '1':
            // TODO: Limpar console de processo
            break;

        case '2':
            // TODO: Limpar console de gestao
            break;

        case 27: // ESC
            executando = false;
            break;
        }
    }

    // --------------------------------------------------------
    // 6. ENCERRAMENTO DAS THREADS
    // --------------------------------------------------------

    SetEvent(eventoEncerramento);

    HANDLE threads[] = {
        hThreadSupervisorio,
        hThreadPCP,
        hThreadRetirada
    };

    for (HANDLE thread : threads) {
        if (thread != NULL) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
        }
    }

    // --------------------------------------------------------
    // 7. ENCERRAMENTO DOS PROCESSOS
    // --------------------------------------------------------

    // Os executaveis filhos devem responder ao evento nomeado
    // Local\ELT127_Encerramento para finalizar normalmente.

    if (processoExibicao) {
        CloseHandle(piExibicao.hThread);
        CloseHandle(piExibicao.hProcess);
    }

    if (processoGestao) {
        CloseHandle(piGestao.hThread);
        CloseHandle(piGestao.hProcess);
    }

    // --------------------------------------------------------
    // 8. LIBERACAO DOS RECURSOS
    // --------------------------------------------------------

    CloseHandle(eventoEncerramento);

    cout << "\nProcesso principal encerrado." << endl;

    return 0;
}