// main.cpp - Processo Principal

#include <windows.h>
#include <iostream>
#include <string>
#include <conio.h>

using namespace std;

const int TAM_LISTA_1 = 5;
string listaCircular1[TAM_LISTA_1];
int pLivre1 = 0;   //Apontador de insercao (Produtores)
int pOcupado1 = 0; //Apontador de retirada (Consumidor)

HANDLE hMutexLista1 = NULL; //Protege o acesso aos apontadores e ao array
HANDLE hSemLivres1 = NULL; //Conta espacos livres (inicia em 100)
HANDLE hSemOcupadas1 = NULL; //Conta mensagens disponiveis (inicia em 0)
HANDLE hMutexConsole = NULL; //Protege o uso do cout no console principal


//FUNÇÃO AUXILIAR DE DEPOSITO DE MENSAGEM
bool DepositarNaLista1(const string& mensagem, const string& nomeTarefa, HANDLE eventoEncerramento) {
    //Verifica sem bloquear (timeout = 0) se ha espaco livre
    DWORD resultado = WaitForSingleObject(hSemLivres1, 0);

    if (resultado == WAIT_TIMEOUT) {

        //O semaforo estava em 0: a lista esta cheia
        WaitForSingleObject(hMutexConsole, INFINITE);
        cout << "[AVISO] Lista circular 1 cheia! Tarefa " << nomeTarefa
            << " bloqueada aguardando posicao livre..." << endl;
        ReleaseMutex(hMutexConsole);

        //Bloqueia esperando OU abrir espaco na lista OU o comando ESC de encerramento
        HANDLE eventosEspera[2] = { eventoEncerramento, hSemLivres1 };
        DWORD causaDesbloqueio = WaitForMultipleObjects(2, eventosEspera, FALSE, INFINITE);

        //Se desbloqueou porque o indice 0 (eventoEncerramento) foi acionado, aborta
        if (causaDesbloqueio == WAIT_OBJECT_0) {
            return false;
        }
    }

    //Temos 1 vaga garantida pelo semaforo. Travamos o Mutex para escrever na memoria:
    WaitForSingleObject(hMutexLista1, INFINITE);

    listaCircular1[pLivre1] = mensagem;
    pLivre1 = (pLivre1 + 1) % TAM_LISTA_1;

    ReleaseMutex(hMutexLista1);

    //Sinaliza para a tarefa de Retirada que ha 1 nova mensagem ocupada
    ReleaseSemaphore(hSemOcupadas1, 1, nullptr);
    return true;
}

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

    while (true) {

        HANDLE eventosEspera[2] = { eventoEncerramento, hSemOcupadas1 };
        DWORD status = WaitForMultipleObjects(2, eventosEspera, FALSE, INFINITE);

        if (status == WAIT_OBJECT_0) {
            break; // Evento de encerramento (ESC) acionado! Sai do while.
        }

        //Se chegou aqui, hSemOcupadas1 foi decrementado (ha mensagem para ler)
        WaitForSingleObject(hMutexLista1, INFINITE);

        string msgRetirada = listaCircular1[pOcupado1];
        pOcupado1 = (pOcupado1 + 1) % TAM_LISTA_1;

        ReleaseMutex(hMutexLista1);

        //Libera 1 posicao livre no semaforo para os produtores (Supervisorio e PCP)
        ReleaseSemaphore(hSemLivres1, 1, nullptr);

        //Exibe na console principal (requisito da Etapa 1)
        WaitForSingleObject(hMutexConsole, INFINITE);
        cout << "[RETIRADA] " << msgRetirada << endl;
        ReleaseMutex(hMutexConsole);
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

    // Criacao dos objetos de sincronizacao da Lista Circular 1
    hMutexLista1 = CreateMutexW(nullptr, FALSE, nullptr);
    hSemLivres1 = CreateSemaphoreW(nullptr, TAM_LISTA_1, TAM_LISTA_1, nullptr);
    hSemOcupadas1 = CreateSemaphoreW(nullptr, 0, TAM_LISTA_1, nullptr);
    hMutexConsole = CreateMutexW(nullptr, FALSE, nullptr);

    if (!hMutexLista1 || !hSemLivres1 || !hSemOcupadas1 || !hMutexConsole) {
        cout << "Erro ao criar objetos de sincronizacao da lista." << endl;
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
    CloseHandle(hMutexLista1);
    CloseHandle(hSemLivres1);
    CloseHandle(hSemOcupadas1);
    CloseHandle(hMutexConsole);

    cout << "\nProcesso principal encerrado." << endl;

    return 0;
}