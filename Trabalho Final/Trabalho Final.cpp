
#include <windows.h>
#include <iostream>
#include <string>
#include <conio.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#define _CHECKERROR 1
#include "../CheckForError.h"
#include "../Comunicacao.h"

using namespace std;

const int TAM_LISTA_1 = 100;
string listaCircular1[TAM_LISTA_1];

const int TAM_LISTA_2 = 200;
const int TAM_MSG_PROCESSO = 51;

struct ListaCircular2Compartilhada {
    int pLivre2;
    int pOcupado2;
    char mensagens[TAM_LISTA_2][TAM_MSG_PROCESSO];
};


// Indices compartilhados
int pLivre1 = 0;// Proxima posicao a reservar
int pPublicacao1 = 0;// Proxima posicao a disponibilizar para retirada

// true: escrita concluida, mas ainda nao publicada no semaforo
bool prontaParaPublicar1[TAM_LISTA_1] = {};

HANDLE hMutexLista1 = NULL; //Protege o acesso aos apontadores e ao array
HANDLE hSemLivres1 = NULL; //Conta espacos livres (inicia em 100)
HANDLE hSemOcupadas1 = NULL; //Conta mensagens disponiveis (inicia em 0)
HANDLE hMutexConsole = NULL; //Protege o uso do cout no console principal

HANDLE hMapLista2 = NULL;     
ListaCircular2Compartilhada* lpLista2 = nullptr; 
HANDLE hMutexLista2 = NULL;
HANDLE hSemLivres2 = NULL;
HANDLE hSemOcupadas2 = NULL;

HANDLE hEventoBloqSupervisorio = NULL;
HANDLE hEventoBloqPCP = NULL;
HANDLE hEventoBloqRetirada = NULL;
HANDLE hEventoBloqExibicao = NULL;
HANDLE hEventoBloqGestao = NULL;

// Aguarda permissao para executar ou encerramento, com prioridade para encerrar
bool AguardarDesbloqueioOuTermino(HANDLE hEventoBloqueio, HANDLE hEventoEncerramento) {
    HANDLE eventos[] = { hEventoEncerramento, hEventoBloqueio };
    DWORD resultado = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
    CheckForError(resultado != WAIT_FAILED);

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
    CheckForError(resultado != WAIT_FAILED);

    if (resultado == WAIT_TIMEOUT) {
        CheckForError(WaitForSingleObject(hMutexConsole, INFINITE) != WAIT_FAILED);
        cout << "[AVISO] Lista circular 1 cheia! Tarefa "
             << nomeTarefa << " aguardando posicao livre..." << endl;
        CheckForError(ReleaseMutex(hMutexConsole));

        HANDLE eventos[] = { eventoEncerramento, hSemLivres1 };
        resultado = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
        CheckForError(resultado != WAIT_FAILED);

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
        CheckForError(ReleaseSemaphore(hSemLivres1, 1, nullptr));
        return false;
    }

    //Reserva uma posicao exclusiva para esta tarefa
    CheckForError(WaitForSingleObject(hMutexLista1, INFINITE) != WAIT_FAILED);

    apontadorIndividual = pLivre1;
    pLivre1 = (pLivre1 + 1) % TAM_LISTA_1;

    CheckForError(ReleaseMutex(hMutexLista1));

    //Cada produtor escreve na posicao que reservou
    listaCircular1[apontadorIndividual] = mensagem;

    //Apos reservar, conclui a escrita e a publicacao sem novas esperas de bloqueio, evitando deixar uma lacuna na fila
    CheckForError(WaitForSingleObject(hMutexLista1, INFINITE) != WAIT_FAILED);

    prontaParaPublicar1[apontadorIndividual] = true;
    int quantidadePublicada = 0;

    // Disponibiliza somente mensagens consecutivas ja prontas
    while (prontaParaPublicar1[pPublicacao1]) {
        prontaParaPublicar1[pPublicacao1] = false;
        pPublicacao1 = (pPublicacao1 + 1) % TAM_LISTA_1;
        quantidadePublicada++;
    }

    if (quantidadePublicada > 0) {
        CheckForError(ReleaseSemaphore(hSemOcupadas1, quantidadePublicada, nullptr));
    }

    CheckForError(ReleaseMutex(hMutexLista1));
    return true;
}

//Função auxiliar de deposito de  mensagem de dados de processo na Lista Circular 2 
bool DepositarNaLista2(const string& mensagem, HANDLE hEventoBloqueio, HANDLE eventoEncerramento) {
    DWORD resultado = WaitForSingleObject(hSemLivres2, 0);
    CheckForError(resultado != WAIT_FAILED);

    if (resultado == WAIT_TIMEOUT) {
        CheckForError(WaitForSingleObject(hMutexConsole, INFINITE) != WAIT_FAILED);
        cout << "[AVISO] Lista circular 2 cheia! Tarefa de Retirada aguardando posicao livre..." << endl;
        CheckForError(ReleaseMutex(hMutexConsole));

        HANDLE eventos[] = { eventoEncerramento, hSemLivres2 };
        resultado = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
        CheckForError(resultado != WAIT_FAILED);
        if (resultado != WAIT_OBJECT_0 + 1) {
            return false; // ESC pressionado
        }
    }
    else if (resultado != WAIT_OBJECT_0) {
        return false;
    }

    if (!AguardarDesbloqueioOuTermino(hEventoBloqueio, eventoEncerramento)) {
        CheckForError(ReleaseSemaphore(hSemLivres2, 1, nullptr));
        return false;
    }

    // Escreve na memoria compartilhada protegida pelo Mutex
    CheckForError(WaitForSingleObject(hMutexLista2, INFINITE) != WAIT_FAILED);

    strcpy_s(lpLista2->mensagens[lpLista2->pLivre2], TAM_MSG_PROCESSO, mensagem.c_str());
    lpLista2->pLivre2 = (lpLista2->pLivre2 + 1) % TAM_LISTA_2;

    CheckForError(ReleaseMutex(hMutexLista2));

    //Avisa o processo ExibicaoProcesso quetem uma nova mensagem disponivel
    CheckForError(ReleaseSemaphore(hSemOcupadas2, 1, nullptr));
    return true;
}

// Envia uma string via Mailslot para um processo filho
bool EnviarMailslot(const wchar_t* nomeMailslot, const string& mensagem) {
    HANDLE hMail = CreateFileW(
        nomeMailslot,
        GENERIC_WRITE,
        FILE_SHARE_READ, 
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hMail == INVALID_HANDLE_VALUE) {
        return false; 
    }

    DWORD bytesEscritos = 0;
    BOOL sucesso = WriteFile(
        hMail,
        mensagem.c_str(),
        (DWORD)(mensagem.size() + 1),
        &bytesEscritos,
        nullptr
    );

    DWORD erro = sucesso ? ERROR_SUCCESS : GetLastError();
    CheckForError(CloseHandle(hMail));
    if (!sucesso) {
        SetLastError(erro);
        return false;
    }
    if (bytesEscritos != mensagem.size() + 1) {
        SetLastError(ERROR_WRITE_FAULT);
        return false;
    }
    return true;
}

void RegistrarMensagemConsole(const string& mensagem) {
    CheckForError(WaitForSingleObject(hMutexConsole, INFINITE) != WAIT_FAILED);
    cout << mensagem << endl;
    CheckForError(ReleaseMutex(hMutexConsole));
}

// Nao inicia o teclado e os produtores antes de os filhos criarem seus canais.
void AguardarMailslotPronto(const wchar_t* nome, HANDLE processo, HANDLE eventoEncerramento) {
    const ULONGLONG inicio = GetTickCount64();
    while (GetTickCount64() - inicio < 10000) {
        HANDLE canal = CreateFileW(nome, GENERIC_WRITE, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (canal != INVALID_HANDLE_VALUE) {
            CheckForError(CloseHandle(canal));
            return;
        }
        HANDLE eventos[] = { eventoEncerramento, processo };
        DWORD resultado = WaitForMultipleObjects(2, eventos, FALSE, 100);
        CheckForError(resultado != WAIT_FAILED);
        if (resultado != WAIT_TIMEOUT) {
            CheckForError(SetEvent(eventoEncerramento));
            SetLastError(ERROR_PROCESS_ABORTED);
            CheckForError(FALSE);
            return;
        }
    }
    CheckForError(SetEvent(eventoEncerramento));
    SetLastError(ERROR_TIMEOUT);
    CheckForError(FALSE);
}

void AlternarBloqueio(HANDLE evento, const char* nomeTarefa) {
    DWORD estado = WaitForSingleObject(evento, 0);
    CheckForError(estado != WAIT_FAILED);
    if (estado == WAIT_OBJECT_0) {
        CheckForError(ResetEvent(evento));
        RegistrarMensagemConsole(string("[TECLADO] Bloqueio solicitado: ") + nomeTarefa + ".");
    }
    else if (estado == WAIT_TIMEOUT) {
        CheckForError(SetEvent(evento));
        RegistrarMensagemConsole(string("[TECLADO] Desbloqueio solicitado: ") + nomeTarefa + ".");
    }
}

void SolicitarLimpeza(const wchar_t* mailslot, const char* destino) {
    if (EnviarMailslot(mailslot, "LIMPAR")) {
        RegistrarMensagemConsole(string("[TECLADO] Comando de limpeza enviado para ") + destino + ".");
    }
    else {
        DWORD erro = GetLastError();
        RegistrarMensagemConsole(string("[ERRO] Nao foi possivel enviar limpeza para ")
            + destino + ". Codigo: " + to_string(erro) + ".");
    }
}

bool TratarComandoTeclado(int tecla, HANDLE eventoEncerramento) {
    switch (tecla) {
    case 's':
        AlternarBloqueio(hEventoBloqSupervisorio, "Leitura do Supervisorio");
        break;
    case 'm':
        AlternarBloqueio(hEventoBloqPCP, "Leitura do PCP");
        break;
    case 'r':
        AlternarBloqueio(hEventoBloqRetirada, "Retirada de Mensagens");
        break;
    case 'p':
        AlternarBloqueio(hEventoBloqExibicao, "Exibicao de Processo");
        break;
    case 'g':
        AlternarBloqueio(hEventoBloqGestao, "Gestao da Producao");
        break;
    case '1':
        SolicitarLimpeza(MAILSLOT_EXIBICAO, "Exibicao de Processo");
        break;
    case '2':
        SolicitarLimpeza(MAILSLOT_GESTAO, "Gestao da Producao");
        break;
    case 27:
        RegistrarMensagemConsole("[TECLADO] Encerramento solicitado.");
        CheckForError(SetEvent(eventoEncerramento));
        return false;
    }
    return true;
}

// Preserva a mensagem enquanto a Gestao inicializa ou esta indisponivel.
bool EncaminharMensagemPCP(const string& mensagem, HANDLE eventoEncerramento) {
    bool avisou = false;
    while (AguardarDesbloqueioOuTermino(hEventoBloqRetirada, eventoEncerramento)) {
        if (EnviarMailslot(MAILSLOT_GESTAO, mensagem)) {
            return true;
        }
        DWORD erro = GetLastError();
        if (!avisou) {
            RegistrarMensagemConsole("[AVISO] Aguardando canal da Gestao para enviar PCP. Codigo: "
                + to_string(erro) + ".");
            avisou = true;
        }
        DWORD resultado = WaitForSingleObject(eventoEncerramento, 100);
        CheckForError(resultado != WAIT_FAILED);
        if (resultado == WAIT_OBJECT_0) {
            return false;
        }
    }
    return false;
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

    // Multiplicador para converter milissegundos em intervalos de 100 ns (padrao Win32)
    const LONGLONG nMultiplicadorParaMs = 10000;
    LARGE_INTEGER Preset;

    HANDLE hTimerSup = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    CheckForError(hTimerSup);

    // Primeiro disparo em 500 ms e periodo continuo de 500 ms
    Preset.QuadPart = -(500 * nMultiplicadorParaMs);
    CheckForError(SetWaitableTimer(hTimerSup, &Preset, 500, nullptr, nullptr, FALSE));

   
    HANDLE eventosTimer[2] = { eventoEncerramento, hTimerSup };

    while (true) {
        //Aguarda o pulso de 500 ms do Timer ou o acionamento de ESC
        DWORD ret = WaitForMultipleObjects(2, eventosTimer, FALSE, INFINITE);
        CheckForError(ret != WAIT_FAILED);
        if (ret == WAIT_OBJECT_0) {
            break; // ESC foi pressionado! Encerra imediatamente.
        }

        //Verifica se a tecla 's' bloqueou a tarefa ou se 'ESC' foi premido
        if (!AguardarDesbloqueioOuTermino(hEventoBloqSupervisorio, eventoEncerramento)) {
            break;
        }
        string msg = GerarMensagemSupervisorio(nseq);
        nseq++;
        if (nseq > 99999) {
            nseq = 0;
        }

        if (!DepositarNaLista1(msg, "Leitura do Supervisorio", pDepositoIndividual,
            hEventoBloqSupervisorio, eventoEncerramento)) {
            break;
        }
    }

    CheckForError(CloseHandle(hTimerSup));
    return 0;
}

// Thread de leitura do PCP
DWORD WINAPI LeituraPCP(LPVOID parametro) {
    HANDLE eventoEncerramento = (HANDLE)parametro;

    // Inicializa os numeros aleatorios
    srand((unsigned int)time(NULL) + 2);

    int nseq = 1; 
    int pDepositoIndividual = -1;

    const LONGLONG nMultiplicadorParaMs = 10000;
    LARGE_INTEGER Preset;

    
    HANDLE hTimerPCP = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    CheckForError(hTimerPCP);

    HANDLE eventosTimer[2] = { eventoEncerramento, hTimerPCP };

    while (true) {
        //Sorteia tempo aleatorio entre 1000 ms (1s) e 5000 ms (5s)
        int tempoAleatorioMs = 1000 + (rand() % 4001);
        Preset.QuadPart = -(tempoAleatorioMs * nMultiplicadorParaMs);

        //Programa o timer para disparar uma unica vez (periodo = 0) apos o tempo sorteado
        CheckForError(SetWaitableTimer(hTimerPCP, &Preset, 0, nullptr, nullptr, FALSE));

        DWORD ret = WaitForMultipleObjects(2, eventosTimer, FALSE, INFINITE);
        CheckForError(ret != WAIT_FAILED);
        if (ret == WAIT_OBJECT_0) {
            break;
        }

        if (!AguardarDesbloqueioOuTermino(hEventoBloqPCP, eventoEncerramento)) {
            break;
        }

        string msg = GerarMensagemPCP(nseq);
        nseq++;
        if (nseq > 99999) {
            nseq = 0;
        }
        if (!DepositarNaLista1(msg, "Leitura do PCP", pDepositoIndividual,
            hEventoBloqPCP, eventoEncerramento)) {
            break; 
        }
    }
  
    CheckForError(CloseHandle(hTimerPCP));
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
        CheckForError(estado != WAIT_FAILED);

        if (estado == WAIT_TIMEOUT && !estavaBloqueado) {
            estavaBloqueado = true;

            CheckForError(WaitForSingleObject(hMutexConsole, INFINITE) != WAIT_FAILED);
            cout << "[STATUS] Tarefa de Retirada: BLOQUEADA" << endl;
            CheckForError(ReleaseMutex(hMutexConsole));
        }

        if (!AguardarDesbloqueioOuTermino(hEventoBloqRetirada, eventoEncerramento)) {
            return false;
        }

        if (estavaBloqueado) {
            estavaBloqueado = false;

            CheckForError(WaitForSingleObject(hMutexConsole, INFINITE) != WAIT_FAILED);
            cout << "[STATUS] Tarefa de Retirada: DESBLOQUEADA" << endl;
            CheckForError(ReleaseMutex(hMutexConsole));
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
        CheckForError(resultado != WAIT_FAILED);
        if (resultado == WAIT_TIMEOUT) {
            continue;
        }

        if (resultado != WAIT_OBJECT_0 + 1) {
            break; //Encerramento solicitado ou falha na espera.
        }

        //Segunda verificacao, imediatamente apos adquirir uma mensagem.
        if (!aguardarPermissao()) {
            CheckForError(ReleaseSemaphore(hSemOcupadas1, 1, nullptr));
            break;
        }

        CheckForError(WaitForSingleObject(hMutexLista1, INFINITE) != WAIT_FAILED);

        string msgRetirada = listaCircular1[pRetiradaIndividual];
        pRetiradaIndividual = (pRetiradaIndividual + 1) % TAM_LISTA_1;

        CheckForError(ReleaseMutex(hMutexLista1));

        // A mensagem ja esta copiada: libera a vaga mesmo durante espera no IPC.
        CheckForError(ReleaseSemaphore(hSemLivres1, 1, nullptr));

        if (!msgRetirada.empty() && msgRetirada[0] == '1') {
            // Mensagem de Dados de Processo (50 chars) -> Vai para a Lista Circular 2
            if (!DepositarNaLista2(msgRetirada, hEventoBloqRetirada, eventoEncerramento)) {
                break;
            }
        }
        else if (!msgRetirada.empty() && msgRetirada[0] == '2') {
            if (!EncaminharMensagemPCP(msgRetirada, eventoEncerramento)) {
                break;
            }
        }
        else {
            RegistrarMensagemConsole("[AVISO] Mensagem de tipo desconhecido na Lista 1: " + msgRetirada);
        }
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
        L"Local\\Encerramento"
    );

    CheckForError(eventoEncerramento);
    
    hEventoBloqSupervisorio = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    CheckForError(hEventoBloqSupervisorio);
    hEventoBloqPCP = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    CheckForError(hEventoBloqPCP);
    hEventoBloqRetirada = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    CheckForError(hEventoBloqRetirada);
    hEventoBloqExibicao = CreateEventW(nullptr, TRUE, TRUE, L"Local\\BloqExibicao");
    CheckForError(hEventoBloqExibicao);
    hEventoBloqGestao = CreateEventW(nullptr, TRUE, TRUE, L"Local\\BloqGestao");
    CheckForError(hEventoBloqGestao);


    //Criacao dos objetos de sincronizacao da Lista Circular 
    hMutexLista1 = CreateMutexW(nullptr, FALSE, nullptr);
    CheckForError(hMutexLista1);
    hSemLivres1 = CreateSemaphoreW(nullptr, TAM_LISTA_1, TAM_LISTA_1, nullptr);
    CheckForError(hSemLivres1);
    hSemOcupadas1 = CreateSemaphoreW(nullptr, 0, TAM_LISTA_1, nullptr);
    CheckForError(hSemOcupadas1);
    hMutexConsole = CreateMutexW(nullptr, FALSE, nullptr);
    CheckForError(hMutexConsole);


    // Criacao da Memoria Compartilhada e Sincronizacao da Lista Circular 2 
    hMapLista2 = CreateFileMappingW(
        INVALID_HANDLE_VALUE, 
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(ListaCircular2Compartilhada),
        L"Local\\MemLista2"
    );

    CheckForError(hMapLista2);

    lpLista2 = (ListaCircular2Compartilhada*)MapViewOfFile(hMapLista2,FILE_MAP_ALL_ACCESS,0, 0, sizeof(ListaCircular2Compartilhada));

    CheckForError(lpLista2);

    ZeroMemory(lpLista2, sizeof(ListaCircular2Compartilhada));//Inicializa a memoria compartilhada zerada


    hMutexLista2 = CreateMutexW(nullptr, FALSE, L"Local\\MutexLista2");
    CheckForError(hMutexLista2);
    hSemLivres2 = CreateSemaphoreW(nullptr, TAM_LISTA_2, TAM_LISTA_2, L"Local\\SemLivres2");
    CheckForError(hSemLivres2);
    hSemOcupadas2 = CreateSemaphoreW(nullptr, 0, TAM_LISTA_2, L"Local\\SemOcupadas2");
    CheckForError(hSemOcupadas2);

    // --------------------------------------------------------
    //CRIACAO DOS PROCESSOS
    // --------------------------------------------------------
    wchar_t bufferCaminho[MAX_PATH];
    DWORD tamanhoCaminho = GetModuleFileNameW(nullptr, bufferCaminho, MAX_PATH);
    CheckForError(tamanhoCaminho);
    CheckForError(tamanhoCaminho < MAX_PATH);
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

    CheckForError(processoExibicao);

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

    CheckForError(processoGestao);
    AguardarMailslotPronto(MAILSLOT_EXIBICAO, piExibicao.hProcess, eventoEncerramento);
    AguardarMailslotPronto(MAILSLOT_GESTAO, piGestao.hProcess, eventoEncerramento);
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

    CheckForError(hThreadSupervisorio);

    HANDLE hThreadPCP = CreateThread(
        nullptr,
        0,
        LeituraPCP,
        eventoEncerramento,
        0,
        nullptr
    );

    CheckForError(hThreadPCP);

    HANDLE hThreadRetirada = CreateThread(
        nullptr,
        0,
        RetiradaMensagens,
        eventoEncerramento,
        0,
        nullptr
    );

    CheckForError(hThreadRetirada);

    // --------------------------------------------------------
    // LEITURA DO TECLADO
    // --------------------------------------------------------

    CheckForError(WaitForSingleObject(hMutexConsole, INFINITE) != WAIT_FAILED);
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
    cout << "\n[STATUS] Todas as tarefas iniciadas DESBLOQUEADAS." << endl;
    CheckForError(ReleaseMutex(hMutexConsole));

    bool executando = true;

    while (executando &&
        hThreadSupervisorio &&
        hThreadPCP &&
        hThreadRetirada) {

        int tecla = _getch();
        if (tecla == 0 || tecla == 224) {
            _getch(); // Descarta o segundo codigo das teclas estendidas.
            continue;
        }
        executando = TratarComandoTeclado(tecla, eventoEncerramento);
    }

    // --------------------------------------------------------
    // ENCERRAMENTO DAS THREADS
    // --------------------------------------------------------

    CheckForError(SetEvent(eventoEncerramento));

    HANDLE threads[] = {
        hThreadSupervisorio,
        hThreadPCP,
        hThreadRetirada
    };

    for (HANDLE thread : threads) {
        if (thread != NULL) {
            CheckForError(WaitForSingleObject(thread, INFINITE) != WAIT_FAILED);
            CheckForError(CloseHandle(thread));
        }
    }

    // --------------------------------------------------------
    // ENCERRAMENTO DOS PROCESSOS
    // --------------------------------------------------------

    if (processoExibicao) {
        // Aguarda o filho responder ao evento de encerramento antes de fechar os handles.
        DWORD resultado = WaitForSingleObject(piExibicao.hProcess, INFINITE);
        CheckForError(resultado != WAIT_FAILED);
        CheckForError(CloseHandle(piExibicao.hThread));
        CheckForError(CloseHandle(piExibicao.hProcess));
    }

    if (processoGestao) {
        DWORD resultado = WaitForSingleObject(piGestao.hProcess, INFINITE);
        CheckForError(resultado != WAIT_FAILED);
        CheckForError(CloseHandle(piGestao.hThread));
        CheckForError(CloseHandle(piGestao.hProcess));
    }

    // --------------------------------------------------------
    // 8. LIBERACAO DOS RECURSOS
    // --------------------------------------------------------

    CheckForError(CloseHandle(eventoEncerramento));
    CheckForError(CloseHandle(hEventoBloqSupervisorio));
    CheckForError(CloseHandle(hEventoBloqPCP));
    CheckForError(CloseHandle(hEventoBloqRetirada));
    CheckForError(CloseHandle(hEventoBloqExibicao));
    CheckForError(CloseHandle(hEventoBloqGestao));

    CheckForError(CloseHandle(hMutexLista1));
    CheckForError(CloseHandle(hSemLivres1));
    CheckForError(CloseHandle(hSemOcupadas1));
    CheckForError(CloseHandle(hMutexConsole));

    CheckForError(UnmapViewOfFile(lpLista2));
    CheckForError(CloseHandle(hMapLista2));
    CheckForError(CloseHandle(hMutexLista2));
    CheckForError(CloseHandle(hSemLivres2));
    CheckForError(CloseHandle(hSemOcupadas2));

    cout << "\nProcesso principal encerrado." << endl;

    return 0;
}
