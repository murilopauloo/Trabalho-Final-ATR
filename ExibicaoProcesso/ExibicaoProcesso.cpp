#include <windows.h>
#include <iostream>
#include <sstream> 
using namespace std;

const int TAM_LISTA_2 = 200;
const int TAM_MSG_PROCESSO = 51;

struct ListaCircular2Compartilhada {
    int pLivre2;
    int pOcupado2;
    char mensagens[TAM_LISTA_2][TAM_MSG_PROCESSO];
};

// Desmembra "1/NNNNN/VOL/PRES/TZ1/TZ2/TZ3/HH:MM:SS.NNN" e imprime no formato exigido
void ExibirMensagemFormatada(const string& msg) {
    stringstream ss(msg);
    string tipo, nseq, vol, pres, tz1, tz2, tz3, hora;

    // Corta a string automaticamente a cada '/'
    getline(ss, tipo, '/');
    getline(ss, nseq, '/');
    getline(ss, vol, '/');
    getline(ss, pres, '/');
    getline(ss, tz1, '/');
    getline(ss, tz2, '/');
    getline(ss, tz3, '/');
    getline(ss, hora, '/');

    cout << "NSEQ: " << nseq << " TZ1: " << tz1 << " TZ2: " << tz2 << " TZ3: " << tz3
        << " VOL: " << vol << " PRES: " << pres << " HORA: " << hora << endl;
}

int main() {
    SetConsoleTitleW(L"Tela de Exibicao de Dados de Processo");

    //Abre os eventos nomeados criados pelo Processo Principal
    HANDLE hEventoEncerramento = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\ELT127_Encerramento");
    HANDLE hEventoBloqueio = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\ELT127_BloqExibicao");

    // 2. Abre a Memoria Compartilhada e os objetos de sincronizacao da Lista Circular 2
    HANDLE hMapLista2 = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"Local\\ELT127_MemLista2");
    HANDLE hMutexLista2 = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\ELT127_MutexLista2");
    HANDLE hSemLivres2 = OpenSemaphoreW(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, L"Local\\ELT127_SemLivres2");
    HANDLE hSemOcupadas2 = OpenSemaphoreW(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, L"Local\\ELT127_SemOcupadas2");

    if (!hEventoEncerramento || !hEventoBloqueio || !hMapLista2 || !hMutexLista2 || !hSemLivres2 || !hSemOcupadas2) {
        cout << "Erro ao abrir objetos compartilhados da Lista 2: " << GetLastError() << endl;
        Sleep(5000);
        return 1;
    }

    ListaCircular2Compartilhada* lpLista2 = (ListaCircular2Compartilhada*)MapViewOfFile(
        hMapLista2,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        sizeof(ListaCircular2Compartilhada)
    );

    if (!lpLista2) {
        cout << "Erro ao mapear Lista 2 na memoria: " << GetLastError() << endl;
        CloseHandle(hSemOcupadas2);
        CloseHandle(hSemLivres2);
        CloseHandle(hMutexLista2);
        CloseHandle(hMapLista2);
        CloseHandle(hEventoBloqueio);
        CloseHandle(hEventoEncerramento);
        return 1;
    }

    cout << "_________________________________________________" << endl;
    cout << "   TAREFA DE EXIBICAO DE PROCESSO      " << endl;
    cout << "_________________________________________________" << endl;
    cout << "Tarefa iniciada: DESBLOQUEADA" << endl;

    bool estavaBloqueado = false;

    while (true) {
        // Verifica encerramento imediato
        if (WaitForSingleObject(hEventoEncerramento, 0) == WAIT_OBJECT_0) {
            break;
        }

        // Verifica estado de bloqueio (tecla 'p')
        DWORD statusBloq = WaitForSingleObject(hEventoBloqueio, 0);
        bool bloqueadoAgora = (statusBloq == WAIT_TIMEOUT);

        if (bloqueadoAgora != estavaBloqueado) {
            estavaBloqueado = bloqueadoAgora;
            if (estavaBloqueado) {
                cout << "Tarefa de Exibicao de Processo: BLOQUEADA" << endl;
            }
            else {
                cout << "Tarefa de Exibicao de Processo: DESBLOQUEADA" << endl;
            }
        }

        if (estavaBloqueado) {
            HANDLE evBloq[2] = { hEventoEncerramento, hEventoBloqueio };
            if (WaitForMultipleObjects(2, evBloq, FALSE, 100) == WAIT_OBJECT_0) {
                break;
            }
            continue;
        }

        // Aguarda mensagem disponivel na Lista Circular 2 ou sinal de encerramento
        HANDLE evConsumo[2] = { hEventoEncerramento, hSemOcupadas2 };
        DWORD res = WaitForMultipleObjects(2, evConsumo, FALSE, 100);

        if (res == WAIT_TIMEOUT) {
            continue;
        }
        if (res == WAIT_OBJECT_0) {
            break;
        }

        // Retira mensagem da Lista Circular 2 sob exclusao mutua
        WaitForSingleObject(hMutexLista2, INFINITE);
        string msgLida(lpLista2->mensagens[lpLista2->pOcupado2]);
        lpLista2->pOcupado2 = (lpLista2->pOcupado2 + 1) % TAM_LISTA_2;
        ReleaseMutex(hMutexLista2);

        // Devolve 1 vaga livre para a Lista Circular 2
        ReleaseSemaphore(hSemLivres2, 1, nullptr);

        // Formata e exibe na tela exclusiva
        ExibirMensagemFormatada(msgLida);
    }

    cout << "\nEncerrando Tarefa de Exibicao de Processo..." << endl;

    UnmapViewOfFile(lpLista2);
    CloseHandle(hSemOcupadas2);
    CloseHandle(hSemLivres2);
    CloseHandle(hMutexLista2);
    CloseHandle(hMapLista2);
    CloseHandle(hEventoBloqueio);
    CloseHandle(hEventoEncerramento);
    return 0;
}