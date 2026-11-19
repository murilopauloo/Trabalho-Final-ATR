#include <windows.h>
#include <iostream>

#define _CHECKERROR 1 // Ativa a checagem de erros no padrao do exemplo do professor.
#include "../CheckForError.h"

using namespace std;

int main() {
    CheckForError(SetConsoleTitleW(L"Tela de Acompanhamento de Producao (Gestao)"));

    //Abre os eventos nomeados criados pelo Processo Principal
    HANDLE hEventoEncerramento = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\ELT127_Encerramento");
    CheckForError(hEventoEncerramento);
    HANDLE hEventoBloqueio = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\ELT127_BloqGestao");
    CheckForError(hEventoBloqueio);


    cout << "_________________________________________________" << endl;
    cout << "        TAREFA DE GESTAO DA PRODUCAO            " << endl;
    cout << "_________________________________________________" << endl;
    cout << "Tarefa iniciada: DESBLOQUEADA" << endl;

    bool estavaBloqueado = false;

    //Laço principal, monitora encerramento (ESC) e mudancas de estado (tecla 'g')
    while (true) {
        DWORD encerramento = WaitForSingleObject(hEventoEncerramento, 100);
        CheckForError(encerramento != WAIT_FAILED);
        if (encerramento == WAIT_OBJECT_0) {
            break;
        }

        DWORD status = WaitForSingleObject(hEventoBloqueio, 0);
        CheckForError(status != WAIT_FAILED);
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

    CheckForError(CloseHandle(hEventoBloqueio));
    CheckForError(CloseHandle(hEventoEncerramento));
    return 0;
}
