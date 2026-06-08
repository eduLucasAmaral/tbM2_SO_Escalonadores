#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Processo {
    int chegada = 0;
    string id;
    vector<int> paginas;
    size_t proxima_pagina = 0;
    int quantum_usado = 0;
    int page_faults = 0;
    int tempo_conclusao = -1;
    bool concluido = false;
    bool bloqueado = false;
    int desbloqueio_em = -1;
    int ordem_entrada = 0;
    int ordem_bloqueio = 0;
};

struct Frame {
    bool ocupado = false;
    int pagina = -1;
    long long ultimo_uso = -1;
};

struct EventoBloqueio {
    int tempo_desbloqueio = 0;
    int ordem = 0;
    int indice_processo = -1;
};

struct ComparadorBloqueio {
    bool operator()(const EventoBloqueio& a, const EventoBloqueio& b) const {
        if (a.tempo_desbloqueio != b.tempo_desbloqueio) {
            return a.tempo_desbloqueio > b.tempo_desbloqueio;
        }
        return a.ordem > b.ordem;
    }
};

class SimuladorRRLRU {
private:
    int quantum;
    int tamanho_ram;
    int penalidade_io;
    vector<Processo> processos;
    vector<Frame> ram;
    unordered_map<int, int> pagina_para_frame;
    queue<int> fila_prontos;
    priority_queue<EventoBloqueio, vector<EventoBloqueio>, ComparadorBloqueio> fila_bloqueados;
    int tempo_atual = 0;
    int ordem_eventos = 0;
    int proximo_indice_entrada = 0;
    int processos_concluidos = 0;
    int processo_ativo = -1;
        // vector<string> log_eventos;

    static string trim(const string& texto) {
        size_t inicio = texto.find_first_not_of(" \t\r\n");
        if (inicio == string::npos) {
            return "";
        }
        size_t fim = texto.find_last_not_of(" \t\r\n");
        return texto.substr(inicio, fim - inicio + 1);
    }

    void registrar_log(string texto) {
        cout << texto << '\n';
        cout.flush();
    }

    void enfileirar_pronto(int indice_processo) {
        Processo& processo = processos[indice_processo];
        if (processo.concluido || processo.bloqueado) {
            return;
        }
        fila_prontos.push(indice_processo);
    }

    void despachar_proximo() {
        if (processo_ativo != -1 || fila_prontos.empty()) {
            return;
        }

        processo_ativo = fila_prontos.front();
        fila_prontos.pop();
        processos[processo_ativo].quantum_usado = 0;
        registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processos[processo_ativo].id + " ganhou a CPU.");
    }

    void adicionar_novas_chegadas() {
        while (proximo_indice_entrada < static_cast<int>(processos.size()) &&
               processos[proximo_indice_entrada].chegada <= tempo_atual) {
            Processo& processo = processos[proximo_indice_entrada];
            if (processo.chegada == tempo_atual) {
                registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processo.id + " chegou ao sistema.");
            }
            if (processo.paginas.empty()) {
                processo.concluido = true;
                processo.tempo_conclusao = max(tempo_atual, processo.chegada);
                processos_concluidos++;
                registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processo.id + " concluido sem acessos.");
            } else {
                enfileirar_pronto(proximo_indice_entrada);
            }
            proximo_indice_entrada++;
        }
    }

    void liberar_bloqueados() {
        while (!fila_bloqueados.empty() && fila_bloqueados.top().tempo_desbloqueio <= tempo_atual) {
            EventoBloqueio evento = fila_bloqueados.top();
            fila_bloqueados.pop();
            Processo& processo = processos[evento.indice_processo];
            if (processo.concluido) {
                continue;
            }
            processo.bloqueado = false;
            processo.desbloqueio_em = -1;
            registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processo.id + " saiu da fila de bloqueados.");
            enfileirar_pronto(evento.indice_processo);
        }
    }

    int encontrar_frame_da_pagina(int pagina) const {
        auto it = pagina_para_frame.find(pagina);
        if (it == pagina_para_frame.end()) {
            return -1;
        }
        return it->second;
    }

    int escolher_frame_lru() const {
        int escolhido = -1;
        long long menor_uso = LLONG_MAX;
        for (int i = 0; i < static_cast<int>(ram.size()); ++i) {
            if (!ram[i].ocupado) {
                return i;
            }
            if (ram[i].ultimo_uso < menor_uso ||
                (ram[i].ultimo_uso == menor_uso && (escolhido == -1 || i < escolhido))) {
                menor_uso = ram[i].ultimo_uso;
                escolhido = i;
            }
        }
        return escolhido;
    }

    void remover_pagina_do_frame(int frame) {
        if (frame < 0 || frame >= static_cast<int>(ram.size()) || !ram[frame].ocupado) {
            return;
        }
        pagina_para_frame.erase(ram[frame].pagina);
        ram[frame].ocupado = false;
        ram[frame].pagina = -1;
        ram[frame].ultimo_uso = -1;
    }

    void carregar_pagina_na_ram(int pagina) {
        int frame_livre = -1;
        for (int i = 0; i < static_cast<int>(ram.size()); ++i) {
            if (!ram[i].ocupado) {
                frame_livre = i;
                break;
            }
        }

        if (frame_livre == -1) {
            frame_livre = escolher_frame_lru();
            if (frame_livre != -1) {
                registrar_log("[Tempo " + to_string(tempo_atual) + "] LRU removeu pagina " +
                              to_string(ram[frame_livre].pagina) + " do frame " +
                              to_string(frame_livre) + ".");
                remover_pagina_do_frame(frame_livre);
            }
        }

        if (frame_livre == -1) {
            return;
        }

        ram[frame_livre].ocupado = true;
        ram[frame_livre].pagina = pagina;
        ram[frame_livre].ultimo_uso = tempo_atual;
        pagina_para_frame[pagina] = frame_livre;
    }

    void bloquear_processso_por_page_fault(int indice_processo, int pagina) {
        Processo& processo = processos[indice_processo];
        processo.bloqueado = true;
        processo.desbloqueio_em = tempo_atual + penalidade_io;
        processo.page_faults++;
        processo.quantum_usado = 0;
        processo.ordem_bloqueio = ++ordem_eventos;
        fila_bloqueados.push({processo.desbloqueio_em, processo.ordem_bloqueio, indice_processo});
        processo_ativo = -1;

        registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processo.id +
                      " sofreu Page Fault na pagina " + to_string(pagina) + ".");
        registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processo.id +
                      " foi para a fila de bloqueados por " + to_string(penalidade_io) + " tique(s).");
        carregar_pagina_na_ram(pagina);
    }

    void finalizar_processo(int indice_processo) {
        Processo& processo = processos[indice_processo];
        processo.concluido = true;
        processo.tempo_conclusao = tempo_atual + 1;
        processos_concluidos++;
        processo_ativo = -1;
        registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processo.id + " concluiu sua ultima pagina.");
    }

    bool ha_processos_pendentes() const {
        return processos_concluidos < static_cast<int>(processos.size());
    }

    void executar_tique() {
        liberar_bloqueados();
        adicionar_novas_chegadas();
        despachar_proximo();

        if (processo_ativo == -1) {
            registrar_log("[Tempo " + to_string(tempo_atual) + "] CPU ociosa.");
            tempo_atual++;
            return;
        }

        int indice_processo = processo_ativo;
        Processo& processo = processos[indice_processo];

        if (processo.concluido || processo.bloqueado) {
            processo_ativo = -1;
            tempo_atual++;
            return;
        }

        if (processo.proxima_pagina >= processo.paginas.size()) {
            finalizar_processo(indice_processo);
            tempo_atual++;
            return;
        }

        int pagina = processo.paginas[processo.proxima_pagina];
        int frame = encontrar_frame_da_pagina(pagina);

        if (frame == -1) {
            bloquear_processso_por_page_fault(indice_processo, pagina);
            tempo_atual++;
            return;
        }

        ram[frame].ultimo_uso = tempo_atual;
        processo.quantum_usado++;
        processo.proxima_pagina++;
        registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processo.id +
                      " acessou pagina " + to_string(pagina) + " com HIT no frame " + to_string(frame) + ".");

        if (processo.proxima_pagina >= processo.paginas.size()) {
            finalizar_processo(indice_processo);
        } else if (processo.quantum_usado >= quantum) {
            processo.quantum_usado = 0;
            enfileirar_pronto(indice_processo);
            processo_ativo = -1;
            registrar_log("[Tempo " + to_string(tempo_atual) + "] " + processo.id + " sofreu preempcao por quantum.");
        }

        tempo_atual++;
    }

    void imprimir_ram_final() const {
        cout << "\nEstado final da RAM:\n";
        for (int i = 0; i < static_cast<int>(ram.size()); ++i) {
            cout << "Frame " << i << ": ";
            if (!ram[i].ocupado) {
                cout << "vazio\n";
            } else {
                cout << "pagina " << ram[i].pagina << " (ultimo uso: " << ram[i].ultimo_uso << ")\n";
            }
        }
    }

public:
    SimuladorRRLRU(int quantum_, int tamanho_ram_, int penalidade_io_, vector<Processo> processos_)
        : quantum(quantum_), tamanho_ram(tamanho_ram_), penalidade_io(penalidade_io_),
          processos(move(processos_)), ram(tamanho_ram_) {
        sort(processos.begin(), processos.end(), [](const Processo& a, const Processo& b) {
            if (a.chegada != b.chegada) {
                return a.chegada < b.chegada;
            }
            return a.ordem_entrada < b.ordem_entrada;
        });
    }

    void executar() {
        while (ha_processos_pendentes()) {
            executar_tique();
        }
    }

    void imprimir_relatorio() const {
        cout << "Simulacao RR + LRU em tempo discreto\n";
        cout << "Quantum: " << quantum << "\n";
        cout << "Frames na RAM: " << tamanho_ram << "\n";
        cout << "Penalidade de I/O: " << penalidade_io << "\n";


        cout << "\nRelatorio final:\n";
        for (const Processo& processo : processos) {
            cout << processo.id << "\n";
            cout << "  Tempo de retorno: " << (processo.tempo_conclusao - processo.chegada) << "\n";
            cout << "  Total de page faults: " << processo.page_faults << "\n";
        }

        imprimir_ram_final();
    }

    friend vector<Processo> ler_processos_de_entrada(istream& in);
};

vector<Processo> ler_processos_de_entrada(istream& in) {
    vector<Processo> processos;
    string linha;
    int ordem = 0;

    while (getline(in, linha)) {
        linha = SimuladorRRLRU::trim(linha);
        if (linha.empty()) {
            continue;
        }

        stringstream ss(linha);
        Processo processo;
        if (!(ss >> processo.chegada >> processo.id)) {
            continue;
        }

        string resto;
        getline(ss, resto);
        resto = SimuladorRRLRU::trim(resto);
        for (char& caractere : resto) {
            if (caractere == ',') {
                caractere = ' ';
            }
        }

        stringstream paginas_stream(resto);
        int pagina = 0;
        while (paginas_stream >> pagina) {
            processo.paginas.push_back(pagina);
        }

        processo.ordem_entrada = ordem++;
        processos.push_back(move(processo));
    }

    return processos;
}

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc != 2) {
        cerr << "Uso: " << argv[0] << " arquivo.txt\n";
        return 1;
    }

    ifstream arquivo(argv[1]);
    if (!arquivo.is_open()) {
        cerr << "Erro: nao foi possivel abrir o arquivo de entrada.\n";
        return 1;
    }

    string primeira_linha;
    if (!getline(arquivo, primeira_linha)) {
        cerr << "Entrada invalida. Arquivo vazio.\n";
        return 1;
    }

    string descartar;
    if (primeira_linha.find_first_not_of(" \t\r\n") == string::npos) {
        while (getline(arquivo, primeira_linha)) {
            if (primeira_linha.find_first_not_of(" \t\r\n") != string::npos) {
                break;
            }
        }
    }

    if (primeira_linha.find_first_not_of(" \t\r\n") == string::npos) {
        cerr << "Entrada invalida. Arquivo vazio.\n";
        return 1;
    }

    vector<Processo> processos = ler_processos_de_entrada(arquivo);

    stringstream cabecalho(primeira_linha);
    vector<int> valores_cabecalho;
    int valor = 0;
    while (cabecalho >> valor) {
        valores_cabecalho.push_back(valor);
    }

    if (valores_cabecalho.size() < 3) {
        cerr << "Entrada invalida. A primeira linha deve conter: quantum frames penalidade_io\n";
        return 1;
    }

    int quantum = valores_cabecalho[0];
    int tamanho_ram = valores_cabecalho[1];
    int penalidade_io = valores_cabecalho[2];

    if (quantum <= 0 || tamanho_ram <= 0 || penalidade_io < 0) {
        cerr << "Valores invalidos na configuracao inicial.\n";
        return 1;
    }

    SimuladorRRLRU simulador(quantum, tamanho_ram, penalidade_io, move(processos));
    simulador.executar();
    simulador.imprimir_relatorio();
    return 0;
}
