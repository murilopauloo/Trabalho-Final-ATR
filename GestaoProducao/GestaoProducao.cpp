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
        cout << "Certifique-se de iniciar a aplicacao pelo Processo Principal." << endl;
        Sleep(5000);
        return 1;
    }

    cout << "================================================" << endl;
    cout << "        TAREFA DE GESTAO DA PRODUCAO            " << endl;
    cout << "================================================" << endl;
    cout << "[ESTADO] Tarefa iniciada: DESBLOQUEADA" << endl;

    bool estavaBloqueado = false;

    //Laco principal, monitora encerramento (ESC) e mudancas de estado (tecla 'g')
    while (WaitForSingleObject(hEventoEncerramento, 100) == WAIT_TIMEOUT) {

        DWORD status = WaitForSingleObject(hEventoBloqueio, 0);
        bool bloqueadoAgora = (status == WAIT_TIMEOUT);

        //Imprime apenas quando o estado mudar (quando o usuario apertar 'g')
        if (bloqueadoAgora != estavaBloqueado) {
            estavaBloqueado = bloqueadoAgora;
            if (estavaBloqueado) {
                cout << "[ESTADO] Tarefa de Gestao da Producao: BLOQUEADA (tecla 'g')" << endl;
            }
            else {
                cout << "[ESTADO] Tarefa de Gestao da Producao: DESBLOQUEADA (tecla 'g')" << endl;
            }
        }
    }
    cout << "\nEncerrando Tarefa de Gestao da Producao..." << endl;

    CloseHandle(hEventoBloqueio);
    CloseHandle(hEventoEncerramento);
    return 0;
}