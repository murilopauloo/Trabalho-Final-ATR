#include <windows.h>
#include <iostream>

using namespace std;

int main() {
    SetConsoleTitleW(L"Tela de Acompanhamento de Producao (Gestao)");

    //Abre os eventos nomeados criados pelo Processo Principal
    HANDLE hEventoEncerramento = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\ELT127_Encerramento");
    HANDLE hEventoBloqueio = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\ELT127_BloqGestao");

    if (!hEventoEncerramento || !hEventoBloqueio) {
        cout << "Erro ao abrir eventos de sincronizacao: " << GetLastError() << endl;
        Sleep(5000);
        return 1;
    }

    cout << "_________________________________________________" << endl;
    cout << "        TAREFA DE GESTAO DA PRODUCAO            " << endl;
    cout << "_________________________________________________" << endl;
    cout << "Tarefa iniciada: DESBLOQUEADA" << endl;

    bool estavaBloqueado = false;

    //Laço principal, monitora encerramento (ESC) e mudancas de estado (tecla 'g')
    while (WaitForSingleObject(hEventoEncerramento, 100) == WAIT_TIMEOUT) {

        DWORD status = WaitForSingleObject(hEventoBloqueio, 0);
        bool bloqueadoAgora = (status == WAIT_TIMEOUT);

        //Imprime apenas quando o estado mudar (quando o usuario apertar 'g')
        if (bloqueadoAgora != estavaBloqueado) {
            estavaBloqueado = bloqueadoAgora;
            if (estavaBloqueado) {
                cout << "Tarefa de Gestao da Producao: BLOQUEADA" << endl;
            }
            else {
                cout << " Tarefa de Gestao da Producao: DESBLOQUEADA" << endl;
            }
        }
    }
    cout << "\nEncerrando Tarefa de Gestao da Producao..." << endl;

    CloseHandle(hEventoBloqueio);
    CloseHandle(hEventoEncerramento);
    return 0;
}