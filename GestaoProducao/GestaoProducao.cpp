#include <windows.h>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>

#define _CHECKERROR 1 // Ativa a checagem de erros no padrao do exemplo do professor.
#include "../CheckForError.h"
#include "../Comunicacao.h"

using namespace std;

void ExibirCabecalho(bool bloqueada) {
    cout << "_________________________________________________" << endl;
    cout << "        TAREFA DE GESTAO DA PRODUCAO" << endl;
    cout << "_________________________________________________" << endl;
    cout << "Tarefa de Gestao da Producao: "
         << (bloqueada ? "BLOQUEADA" : "DESBLOQUEADA") << endl;
}

void ExibirMensagemPCP(const string& mensagem) {
    stringstream campos(mensagem);
    string tipo, nseq, inicio, duracao, ordem;
    if (!getline(campos, tipo, '/') || !getline(campos, nseq, '/')
        || !getline(campos, inicio, '/') || !getline(campos, duracao, '/')
        || !getline(campos, ordem) || tipo != "2") {
        cout << "[AVISO] Mensagem PCP invalida: " << mensagem << endl;
        return;
    }
    cout << "NSEQ: " << nseq << " INICIO: " << inicio
         << " DURACAO: " << duracao << " min OP: " << ordem << endl;
}

int main() {
    CheckForError(SetConsoleTitleW(L"Tela de Acompanhamento de Producao (Gestao)"));

    //Abre os eventos nomeados criados pelo Processo Principal
    HANDLE hEventoEncerramento = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\Encerramento");
    CheckForError(hEventoEncerramento);
    HANDLE hEventoBloqueio = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\BloqGestao");
    CheckForError(hEventoBloqueio);
    HANDLE hMailslot = CriarMailslotEntrada(MAILSLOT_GESTAO);


    ExibirCabecalho(false);

    bool estavaBloqueado = false;
    queue<string> mensagensPendentes;

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
                cout << "Tarefa de Gestao da Producao: DESBLOQUEADA" << endl;
            }
        }

        // Recebe comandos mesmo bloqueada e preserva os dados para a retomada.
        string mensagem;
        for (int i = 0; i < 64 && LerMensagemMailslot(hMailslot, mensagem); ++i) {
            if (mensagem == "LIMPAR") {
                LimparConsole();
                ExibirCabecalho(estavaBloqueado);
                cout << "[CONSOLE] Tela limpa." << endl;
            }
            else if (mensagem.compare(0, 2, "2/") == 0) {
                mensagensPendentes.push(mensagem);
            }
            else {
                cout << "[AVISO] Mensagem desconhecida: " << mensagem << endl;
            }
        }

        if (bloqueadoAgora) {
            continue;
        }

        // Limita o lote para voltar a verificar comandos e encerramento.
        for (int i = 0; i < 32 && !mensagensPendentes.empty(); ++i) {
            encerramento = WaitForSingleObject(hEventoEncerramento, 0);
            CheckForError(encerramento != WAIT_FAILED);
            status = WaitForSingleObject(hEventoBloqueio, 0);
            CheckForError(status != WAIT_FAILED);
            if (encerramento == WAIT_OBJECT_0 || status == WAIT_TIMEOUT) {
                break;
            }
            ExibirMensagemPCP(mensagensPendentes.front());
            mensagensPendentes.pop();
        }
    }
    cout << "\nEncerrando Tarefa de Gestao da Producao..." << endl;

    CheckForError(CloseHandle(hMailslot));
    CheckForError(CloseHandle(hEventoBloqueio));
    CheckForError(CloseHandle(hEventoEncerramento));
    return 0;
}
