#include <windows.h>
#include <iostream>

using namespace std;

int main() {
    SetConsoleTitleW(L"Tela de Exibicao de Dados de Processo");

    //Abre os eventos nomeados criados pelo Processo Principal
    HANDLE hEventoEncerramento = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\ELT127_Encerramento");
    HANDLE hEventoBloqueio = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\ELT127_BloqExibicao");

    if (!hEventoEncerramento || !hEventoBloqueio) {
        cout << "Erro ao abrir eventos de sincronizacao: " << GetLastError() << endl;
        cout << "Certifique-se de iniciar a aplicacao pelo Processo Principal." << endl;
        Sleep(5000);
        return 1;
    }

    cout << "================================================" << endl;
    cout << "   TAREFA DE EXIBICAO DE DADOS DE PROCESSO      " << endl;
    cout << "================================================" << endl;
    cout << "[ESTADO] Tarefa iniciada: DESBLOQUEADA" << endl;

    bool estavaBloqueado = false;

    //Laco principal: monitora encerramento (ESC) e mudancas de estado (tecla 'p')
    while (WaitForSingleObject(hEventoEncerramento, 100) == WAIT_TIMEOUT) {

        //Verifica se o evento de bloqueio esta sinalizado (timeout = 0)
        //Sinalizado (WAIT_OBJECT_0) = Desbloqueado | Nao-sinalizado (WAIT_TIMEOUT) = Bloqueado
        DWORD status = WaitForSingleObject(hEventoBloqueio, 0);
        bool bloqueadoAgora = (status == WAIT_TIMEOUT);

        //Imprime apenas quando o estado mudar (quando o usuario apertar 'p')
        if (bloqueadoAgora != estavaBloqueado) {
            estavaBloqueado = bloqueadoAgora;
            if (estavaBloqueado) {
                cout << "[ESTADO] Tarefa de Exibicao de Processo: BLOQUEADA (tecla 'p')" << endl;
            }
            else {
                cout << "[ESTADO] Tarefa de Exibicao de Processo: DESBLOQUEADA (tecla 'p')" << endl;
            }
        }
    }

    cout << "\nEncerrando Tarefa de Exibicao de Dados de Processo..." << endl;

    CloseHandle(hEventoBloqueio);
    CloseHandle(hEventoEncerramento);
    return 0;
}