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
        Sleep(5000);
        return 1;
    }

    cout << "_________________________________________________" << endl;
    cout << "   TAREFA DE EXIBICAO DE PROCESSO      " << endl;
    cout << "_________________________________________________" << endl;
    cout << "Tarefa iniciada: DESBLOQUEADA" << endl;

    bool estavaBloqueado = false;

    //Laçoo principal: monitora encerramento (ESC) e mudanças de estado (tecla 'p')
    while (WaitForSingleObject(hEventoEncerramento, 100) == WAIT_TIMEOUT) {

        //Verifica se o evento de bloqueio esta sinalizado
        DWORD status = WaitForSingleObject(hEventoBloqueio, 0);
        bool bloqueadoAgora = (status == WAIT_TIMEOUT);

        //Imprime apenas quando o estado mudar (apertar 'p')
        if (bloqueadoAgora != estavaBloqueado) {
            estavaBloqueado = bloqueadoAgora;
            if (estavaBloqueado) {
                cout << "Tarefa de Exibicao de Processo: BLOQUEADA" << endl;
            }
            else {
                cout << "Tarefa de Exibicao de Processo: DESBLOQUEADA" << endl;
            }
        }
    }

    cout << "\nEncerrando Tarefa de Exibicao de Processo..." << endl;

    CloseHandle(hEventoBloqueio);
    CloseHandle(hEventoEncerramento);
    return 0;
}