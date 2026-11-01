
#include <windows.h>
#include <iostream>
#include <string>
#include <conio.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

using namespace std;

const int TAM_LISTA_1 = 100;
string listaCircular1[TAM_LISTA_1];
// Indices compartilhados, manipulados somente com hMutexLista1 adquirido
int pLivre1 = 0;// Proxima posicao a reservar
int pPublicacao1 = 0;// Proxima posicao a disponibilizar para retirada

// true: escrita concluida, mas ainda nao publicada no semaforo
bool prontaParaPublicar1[TAM_LISTA_1] = {};

HANDLE hMutexLista1 = NULL; //Protege o acesso aos apontadores e ao array
HANDLE hSemLivres1 = NULL; //Conta espacos livres (inicia em 100)
HANDLE hSemOcupadas1 = NULL; //Conta mensagens disponiveis (inicia em 0)
HANDLE hMutexConsole = NULL; //Protege o uso do cout no console principal


HANDLE hEventoBloqSupervisorio = NULL;
HANDLE hEventoBloqPCP = NULL;
HANDLE hEventoBloqRetirada = NULL;
HANDLE hEventoBloqExibicao = NULL;
HANDLE hEventoBloqGestao = NULL;

// Aguarda permissao para executar ou encerramento, com prioridade para encerrar
bool AguardarDesbloqueioOuTermino(HANDLE hEventoBloqueio, HANDLE hEventoEncerramento) {
    HANDLE eventos[] = { hEventoEncerramento, hEventoBloqueio };
    DWORD resultado = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);

    if (resultado == WAIT_OBJECT_0) {
        return false; //Encerramento solicitado, mesmo se a tarefa estiver bloqueada
    }

    if (resultado == WAIT_OBJECT_0 + 1) {
        return true; //Evento de bloqueio sinalizado: tarefa liberada
    }

    return false; //Falha na espera: interrompe a tarefa
}

// Gera mensagem de 50 caracteres do Supervisorio
// Formato: "1/NNNNN/NNN.N/NNN.N/NNN.N/NNN.N/NNN.N/HH:MM:SS.NNN"
string GerarMensagemSupervisorio(int nseq) {
    // Sorteia valores de 0.0 a 999.9 dividindo um inteiro por 10.0
    double volume = (rand() % 10000) / 10.0;
    double pressao = (rand() % 10000) / 10.0;
    double tzona1 = (rand() % 10000) / 10.0;
    double tzona2 = (rand() % 10000) / 10.0;
    double tzona3 = (rand() % 10000) / 10.0;

    //Pega a hora atual do Windows com milissegundos
    SYSTEMTIME horaAtual;
    GetLocalTime(&horaAtual);

    //Monta o texto formatado com zeros a esquerda
    char texto[51];
    sprintf_s(texto, sizeof(texto),
        "1/%05d/%05.1f/%05.1f/%05.1f/%05.1f/%05.1f/%02d:%02d:%02d.%03d",
        nseq, volume, pressao, tzona1, tzona2, tzona3,
        horaAtual.wHour, horaAtual.wMinute, horaAtual.wSecond, horaAtual.wMilliseconds
    );

    return string(texto);
}

// Gera mensagem de 31 caracteres do PCP
// Formato: "2/NNNNN/HH:MM:SS/NNNNN/AAAAAAAA"
string GerarMensagemPCP(int nseq) {
    int hora = rand() % 24;//0 a 23
    int minuto = rand() % 60;//0 a 59
    int segundo = rand() % 60;//0 a 59
    int duracao = rand() % 10000;// 0 a 9999 minutos

    // Sorteia um numero para a OP
    int numeroOP = rand() % 100000;

    char texto[32];
    sprintf_s(texto, sizeof(texto),
        "2/%05d/%02d:%02d:%02d/%05d/OP-%05d",
        nseq, hora, minuto, segundo, duracao, numeroOP
    );

    return string(texto);
}

//Função auxiliar de deposito de mensages
bool DepositarNaLista1(
    const string& mensagem,
    const string& nomeTarefa,
    int& apontadorIndividual,
    HANDLE hEventoBloqueio,
    HANDLE eventoEncerramento)
{
    DWORD resultado = WaitForSingleObject(hSemLivres1, 0);

    if (resultado == WAIT_TIMEOUT) {
        WaitForSingleObject(hMutexConsole, INFINITE);
        cout << "[AVISO] Lista circular 1 cheia! Tarefa "
             << nomeTarefa << " aguardando posicao livre..." << endl;
        ReleaseMutex(hMutexConsole);

        HANDLE eventos[] = { eventoEncerramento, hSemLivres1 };
        resultado = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);

        //Continua somente se uma vaga foi adqurida
        if (resultado != WAIT_OBJECT_0 + 1) {
            return false;
        }
    }
    else if (resultado != WAIT_OBJECT_0) {
        return false; //Falha ao aguardar o semaforo
    }

    //A tarefa pode ter sido bloqueada enquanto aguardava uma vaga
    if (!AguardarDesbloqueioOuTermino(hEventoBloqueio, eventoEncerramento)) {
        ReleaseSemaphore(hSemLivres1, 1, nullptr);
        return false;
    }

    //Reserva uma posicao exclusiva para esta tarefa
    WaitForSingleObject(hMutexLista1, INFINITE);

    apontadorIndividual = pLivre1;
    pLivre1 = (pLivre1 + 1) % TAM_LISTA_1;

    ReleaseMutex(hMutexLista1);

    //Cada produtor escreve na posicao que reservou
    listaCircular1[apontadorIndividual] = mensagem;

    //Apos reservar, conclui a escrita e a publicacao sem novas esperas de bloqueio, evitando deixar uma lacuna na fila
    WaitForSingleObject(hMutexLista1, INFINITE);

    prontaParaPublicar1[apontadorIndividual] = true;
    int quantidadePublicada = 0;

    // Disponibiliza somente mensagens consecutivas ja prontas
    while (prontaParaPublicar1[pPublicacao1]) {
        prontaParaPublicar1[pPublicacao1] = false;
        pPublicacao1 = (pPublicacao1 + 1) % TAM_LISTA_1;
        quantidadePublicada++;
    }

    if (quantidadePublicada > 0) {
        ReleaseSemaphore(hSemOcupadas1, quantidadePublicada, nullptr);
    }

    ReleaseMutex(hMutexLista1);
    return true;
}

// ============================================================
// THREADS
// ============================================================

// Thread de leitura do Supervisorio
DWORD WINAPI LeituraSupervisorio(LPVOID parametro) {
    HANDLE eventoEncerramento = (HANDLE)parametro;

    //Inicializa os numeros aleatorios para esta thread
    srand((unsigned int)time(NULL) + 1);

    int nseq = 1; // Comeca em 1
    int pDepositoIndividual = -1; // Apontador local desta tarefa.

    while (WaitForSingleObject(eventoEncerramento, 0) == WAIT_TIMEOUT) {
        //Verifica se a tecla 's' bloqueou a tarefa ou se 'ESC' foi premido
        if (!AguardarDesbloqueioOuTermino(hEventoBloqSupervisorio, eventoEncerramento)) {
            break;
        }

        //Gera a mensagem formatada com 50 caracteres
        string msg = GerarMensagemSupervisorio(nseq);

        //Incrementa o NSEQ (volta a 0 apos atingir 99999)
        nseq++;
        if (nseq > 99999) {
            nseq = 0;
        }

        //Deposita a mensagem na Lista Circular 1
        if (!DepositarNaLista1(msg, "Leitura do Supervisorio", pDepositoIndividual,
                hEventoBloqSupervisorio, eventoEncerramento)) {
            break; //Sai se o utilizador premiu ESC enquanto a lista estava cheia
        }

        //Temporizacao provisoria da Etapa 1
        Sleep(1000);
    }

    return 0;
}

// Thread de leitura do PCP
DWORD WINAPI LeituraPCP(LPVOID parametro) {
    HANDLE eventoEncerramento = (HANDLE)parametro;

    // Inicializa os numeros aleatorios
    srand((unsigned int)time(NULL) + 2);

    int nseq = 1; // Comeca em 1
    int pDepositoIndividual = -1; // Apontador local desta tarefa.

    while (WaitForSingleObject(eventoEncerramento, 0) == WAIT_TIMEOUT) {
        //Verifica se a tecla 'm' bloqueou a tarefa ou se 'ESC' foi premido
        if (!AguardarDesbloqueioOuTermino(hEventoBloqPCP, eventoEncerramento)) {
            break;
        }

        //Gera a mensagem formatada com 31 caracteres
        string msg = GerarMensagemPCP(nseq);

        //Incrementa o NSEQ
        nseq++;
        if (nseq > 99999) {
            nseq = 0;
        }

        //Deposita a mensagem na Lista Circular
        if (!DepositarNaLista1(msg, "Leitura do PCP", pDepositoIndividual,
                hEventoBloqPCP, eventoEncerramento)) {
            break; //Sai se o utilizador digitou ESC enquanto a lista estava cheia
        }

        //Temporizacao provisoria da Etapa 1
        Sleep(1000);
    }

    return 0;
}

//Thread de retirada de mensagens
DWORD WINAPI RetiradaMensagens(LPVOID parametro) {
    HANDLE eventoEncerramento = (HANDLE)parametro;

    // Apontador individual desta tarefa.
    int pRetiradaIndividual = 0;
    bool estavaBloqueado = false;

    // Centraliza a verificacao de permissao e a exibicao do estado.
    auto aguardarPermissao = [&]() -> bool {
        DWORD estado = WaitForSingleObject(hEventoBloqRetirada, 0);
        if (estado == WAIT_FAILED) {
            return false;
        }

        if (estado == WAIT_TIMEOUT && !estavaBloqueado) {
            estavaBloqueado = true;

            WaitForSingleObject(hMutexConsole, INFINITE);
            cout << "[STATUS] Tarefa de Retirada: BLOQUEADA" << endl;
            ReleaseMutex(hMutexConsole);
        }

        if (!AguardarDesbloqueioOuTermino(hEventoBloqRetirada, eventoEncerramento)) {
            return false;
        }

        if (estavaBloqueado) {
            estavaBloqueado = false;

            WaitForSingleObject(hMutexConsole, INFINITE);
            cout << "[STATUS] Tarefa de Retirada: DESBLOQUEADA" << endl;
            ReleaseMutex(hMutexConsole);
        }

        return true;
    };

    while (true) {
        //Primeira verificacao: inicio da iteracao.
        if (!aguardarPermissao()) {
            break;
        }

        HANDLE eventos[] = { eventoEncerramento, hSemOcupadas1 };

        //Verifica o bloqueio mesmo quando nao chegam mensagens.
        DWORD resultado = WaitForMultipleObjects(2, eventos, FALSE, 100);
        if (resultado == WAIT_TIMEOUT) {
            continue;
        }

        if (resultado != WAIT_OBJECT_0 + 1) {
            break; //Encerramento solicitado ou falha na espera.
        }

        //Segunda verificacao, imediatamente apos adquirir uma mensagem.
        if (!aguardarPermissao()) {
            ReleaseSemaphore(hSemOcupadas1, 1, nullptr);
            break;
        }

        WaitForSingleObject(hMutexLista1, INFINITE);

        string msgRetirada = listaCircular1[pRetiradaIndividual];
        pRetiradaIndividual = (pRetiradaIndividual + 1) % TAM_LISTA_1;

        ReleaseMutex(hMutexLista1);

        //Somente agora a posicao pode ser reutilizada.
        ReleaseSemaphore(hSemLivres1, 1, nullptr);

        WaitForSingleObject(hMutexConsole, INFINITE);
        cout << msgRetirada << endl;
        ReleaseMutex(hMutexConsole);
    }

    return 0;
}

// ============================================================
// PROCESSO PRINCIPAL

int main() {

    // --------------------------------------------------------
    //DECLARACAO DOS PROCESSOS
    // --------------------------------------------------------

    STARTUPINFOW siExibicao = {};
    STARTUPINFOW siGestao = {};

    PROCESS_INFORMATION piExibicao = {};
    PROCESS_INFORMATION piGestao = {};

    siExibicao.cb = sizeof(siExibicao);
    siGestao.cb = sizeof(siGestao);

    // --------------------------------------------------------
    //CRIACAO DO EVENTO DE ENCERRAMENTO
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
    
    hEventoBloqSupervisorio = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    hEventoBloqPCP = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    hEventoBloqRetirada = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    hEventoBloqExibicao = CreateEventW(nullptr, TRUE, TRUE, L"Local\\ELT127_BloqExibicao");
    hEventoBloqGestao = CreateEventW(nullptr, TRUE, TRUE, L"Local\\ELT127_BloqGestao");

    if (!eventoEncerramento || !hEventoBloqSupervisorio || !hEventoBloqPCP ||
        !hEventoBloqRetirada || !hEventoBloqExibicao || !hEventoBloqGestao) {
        cout << "Erro ao criar eventos de sincronizacao." << endl;
        return 1;
    }

    //Criacao dos objetos de sincronizacao da Lista Circular 
    hMutexLista1 = CreateMutexW(nullptr, FALSE, nullptr);
    hSemLivres1 = CreateSemaphoreW(nullptr, TAM_LISTA_1, TAM_LISTA_1, nullptr);
    hSemOcupadas1 = CreateSemaphoreW(nullptr, 0, TAM_LISTA_1, nullptr);
    hMutexConsole = CreateMutexW(nullptr, FALSE, nullptr);

    if (!hMutexLista1 || !hSemLivres1 || !hSemOcupadas1 || !hMutexConsole) {
        cout << "Erro ao criar objetos de sincronizacao da lista." << endl;
        return 1;
    }

    // --------------------------------------------------------
    //CRIACAO DOS PROCESSOS
    // --------------------------------------------------------
    wchar_t bufferCaminho[MAX_PATH];
    GetModuleFileNameW(nullptr, bufferCaminho, MAX_PATH);
    wstring pastaExe(bufferCaminho);
    pastaExe = pastaExe.substr(0, pastaExe.find_last_of(L"\\/"));

    wstring caminhoExibicao = pastaExe + L"\\ExibicaoProcesso.exe";
    wstring caminhoGestao = pastaExe + L"\\GestaoProducao.exe";

    // Processo de Exibicao dos Dados de Processo
    BOOL processoExibicao = CreateProcessW(
        caminhoExibicao.c_str(),
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
        caminhoGestao.c_str(),
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
    //CRIACAO DAS THREADS
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
    // LEITURA DO TECLADO
    // --------------------------------------------------------

    cout << "______________________________________" << endl;
    cout << " SISTEMA DE MONITORAMENTO INDUSTRIAL " << endl;
    cout << "______________________________________" << endl;

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

        case 's': {
            DWORD estado = WaitForSingleObject(hEventoBloqSupervisorio, 0);
            if (estado == WAIT_OBJECT_0)
                ResetEvent(hEventoBloqSupervisorio); // Bloqueia
            else if (estado == WAIT_TIMEOUT)
                SetEvent(hEventoBloqSupervisorio);   // Desbloqueia
            break;
        }

        case 'm': {
            DWORD estado = WaitForSingleObject(hEventoBloqPCP, 0);
            if (estado == WAIT_OBJECT_0)
                ResetEvent(hEventoBloqPCP);
            else if (estado == WAIT_TIMEOUT)
                SetEvent(hEventoBloqPCP);
            break;
        }

        case 'r': {
            DWORD estado = WaitForSingleObject(hEventoBloqRetirada, 0);
            if (estado == WAIT_OBJECT_0)
                ResetEvent(hEventoBloqRetirada);
            else if (estado == WAIT_TIMEOUT)
                SetEvent(hEventoBloqRetirada);
            break;
        }

        case 'p': {
            DWORD estado = WaitForSingleObject(hEventoBloqExibicao, 0);
            if (estado == WAIT_OBJECT_0)
                ResetEvent(hEventoBloqExibicao);
            else if (estado == WAIT_TIMEOUT)
                SetEvent(hEventoBloqExibicao);
            break;
        }

        case 'g': {
            DWORD estado = WaitForSingleObject(hEventoBloqGestao, 0);
            if (estado == WAIT_OBJECT_0)
                ResetEvent(hEventoBloqGestao);
            else if (estado == WAIT_TIMEOUT)
                SetEvent(hEventoBloqGestao);
            break;
        }

        case '1':
        case '2':
            cout << "[Aviso] Sinal de limpeza enviado\n";
            break;

        case 27: // ESC
            executando = false;
            break;
        }
    }

    // --------------------------------------------------------
    // ENCERRAMENTO DAS THREADS
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
    // ENCERRAMENTO DOS PROCESSOS
    // --------------------------------------------------------

    if (processoExibicao) {
        // Aguarda o filho responder ao evento de encerramento antes de fechar os handles.
        DWORD resultado = WaitForSingleObject(piExibicao.hProcess, INFINITE);
        if (resultado == WAIT_FAILED) {
            cout << "Erro ao aguardar processo de exibicao: " << GetLastError() << endl;
        }
        CloseHandle(piExibicao.hThread);
        CloseHandle(piExibicao.hProcess);
    }

    if (processoGestao) {
        DWORD resultado = WaitForSingleObject(piGestao.hProcess, INFINITE);
        if (resultado == WAIT_FAILED) {
            cout << "Erro ao aguardar processo de gestao: " << GetLastError() << endl;
        }
        CloseHandle(piGestao.hThread);
        CloseHandle(piGestao.hProcess);
    }

    // --------------------------------------------------------
    // 8. LIBERACAO DOS RECURSOS
    // --------------------------------------------------------

    CloseHandle(eventoEncerramento);
    CloseHandle(hEventoBloqSupervisorio);
    CloseHandle(hEventoBloqPCP);
    CloseHandle(hEventoBloqRetirada);
    CloseHandle(hEventoBloqExibicao);
    CloseHandle(hEventoBloqGestao);

    CloseHandle(hMutexLista1);
    CloseHandle(hSemLivres1);
    CloseHandle(hSemOcupadas1);
    CloseHandle(hMutexConsole);

    cout << "\nProcesso principal encerrado." << endl;

    return 0;
}
