#include <iostream>    // Para entrada e saída de dados
#include <vector>      // Para usar vetores dinâmicos
#include <iomanip>     // Para formatação de saída
#include <cstdlib>     // Para rand() e srand()
#include <ctime>       // Para gerar números aleatórios
#include <algorithm>   // Para operações com vetores

using namespace std;

// Corezinhas :)
const string VERDE = "\033[32m";
const string VERMELHO = "\033[31m";
const string AMARELO = "\033[33m";
const string AZUL = "\033[34m";
const string CIANO = "\033[36m";
const string BRANCO = "\033[37m";
const string NEGRITO = "\033[1m";
const string RESET = "\033[0m";

// Estrutura que representa uma entrada na Tabela de Páginas
struct PaginaInfo {
    int numero_frame;      // Número do frame onde a página está na RAM (-1 se não estiver)
    char bit_validado;     // 'v' = válido (na RAM), 'i' = inválido (no disco)
};

// Classe que simula o sistema de paginação
class SimuladorPageFault {
private:
    vector<int> ram;                           // Memória RAM (cada posição é um frame)
    vector<PaginaInfo> tabela_paginas;         // Tabela de páginas do processo
    int tamanho_ram;                           // Quantidade de frames na RAM
    int numero_paginas;                        // Número total de páginas
    int page_faults;                           // Contador de page faults
    int acessos_bem_sucedidos;                 // Contador de acessos bem-sucedidos
    int interrupcoes;                          // Contador de interrupções
    
public:
    // Construtor para inicializar o simulador
    SimuladorPageFault(int tam_ram, int num_paginas) 
        : tamanho_ram(tam_ram), numero_paginas(num_paginas), 
          page_faults(0), acessos_bem_sucedidos(0), interrupcoes(0) {
        
        ram.resize(tamanho_ram, -1);  // -1 indica frame vazio
        tabela_paginas.resize(num_paginas);
        
        inicializar_sistema();
    }
    
    // Inicialização do sistema com aproximadamente 50% de frames ocupados
    void inicializar_sistema() {
        int frames_para_ocupar = tamanho_ram / 2;
        
        // Preencher metade dos frames com páginas aleatórias
        for (int i = 0; i < frames_para_ocupar; i++) {
            int pagina = rand() % numero_paginas;
            ram[i] = pagina;
            tabela_paginas[pagina].numero_frame = i;
            tabela_paginas[pagina].bit_validado = 'v';
        }
        
        // Marcar páginas não carregadas como inválidas
        for (int i = 0; i < numero_paginas; i++) {
            if (tabela_paginas[i].bit_validado != 'v') {
                tabela_paginas[i].numero_frame = -1;
                tabela_paginas[i].bit_validado = 'i';
            }
        }
    }
    
    // Simular o acesso a uma página
    bool acessar_pagina(int numero_pagina) {
        if (numero_pagina < 0 || numero_pagina >= numero_paginas) {
            return false;
        }
        
        // CASO 1: Acesso bem-sucedido (página está na RAM)
        if (tabela_paginas[numero_pagina].bit_validado == 'v') {
            acessos_bem_sucedidos++;
            return true;
        }
        
        // CASO 2 e 3: Página não está na RAM (Page Fault)
        page_faults++;
        
        // Encontrar um frame vazio
        int frame_livre = -1;
        for (int i = 0; i < tamanho_ram; i++) {
            if (ram[i] == -1) {
                frame_livre = i;
                break;
            }
        }
        
        // Se não houver frame livre, retornar false
        if (frame_livre == -1) {
            return false;
        }
        
        // Carregar a página no frame livre
        ram[frame_livre] = numero_pagina;
        tabela_paginas[numero_pagina].numero_frame = frame_livre;
        tabela_paginas[numero_pagina].bit_validado = 'v';
        
        return false;
    }
    
    // Exibir o estado atual da RAM
    void exibir_ram() {
        cout << "\n" << NEGRITO << CIANO << "╔════════════════════════════════════════╗\n"
             << "║        ESTADO DA MEMÓRIA RAM           ║\n"
             << "╚════════════════════════════════════════╝" << RESET << "\n\n";
        
        cout << NEGRITO << "Frame │ Página │ Status\n";
        cout << "──────┼────────┼──────────────\n" << RESET;
        
        for (int i = 0; i < tamanho_ram; i++) {
            cout << setw(5) << i << " │ ";
            
            if (ram[i] == -1) {
                cout << VERDE << setw(6) << "LIVRE" << RESET;
                cout << " │ Vago\n";
            } else {
                cout << AMARELO << setw(6) << ram[i] << RESET;
                cout << " │ Ocupado\n";
            }
        }
        cout << "\n";
    }
    
    // Exibir a Tabela de Páginas
    void exibir_tabela_paginas() {
        cout << "\n" << NEGRITO << CIANO << "╔════════════════════════════════════════╗\n"
             << "║     TABELA DE PÁGINAS DO PROCESSO      ║\n"
             << "╚════════════════════════════════════════╝" << RESET << "\n\n";
        
        cout << NEGRITO << "Página │ Frame │ Validade │ Localização\n";
        cout << "───────┼───────┼──────────┼────────────────────\n" << RESET;
        
        for (int i = 0; i < numero_paginas; i++) {
            cout << setw(6) << i << " │ ";
            
            if (tabela_paginas[i].numero_frame == -1) {
                cout << VERMELHO << setw(5) << "-" << RESET << " │ ";
            } else {
                cout << VERDE << setw(5) << tabela_paginas[i].numero_frame << RESET << " │ ";
            }
            
            cout << setw(8);
            if (tabela_paginas[i].bit_validado == 'v') {
                cout << VERDE << 'v' << RESET;
            } else {
                cout << VERMELHO << 'i' << RESET;
            }
            cout << "     │ ";
            
            if (tabela_paginas[i].bit_validado == 'v') {
                cout << VERDE << "RAM (Frame " << tabela_paginas[i].numero_frame << ")" << RESET;
            } else {
                cout << VERMELHO << "Disco (Backing Store)" << RESET;
            }
            cout << "\n";
        }
        cout << "\n";
    }
    
    // Exibir estatísticas da simulação
    void exibir_estatisticas() {
        int total_acessos = acessos_bem_sucedidos + page_faults;
        double taxa_acerto = (total_acessos > 0) ? 
                             (double)acessos_bem_sucedidos / total_acessos * 100 : 0;
        
        cout << "\n" << NEGRITO << CIANO << "╔════════════════════════════════════════╗\n"
             << "║           ESTATÍSTICAS FINAIS          ║\n"
             << "╚════════════════════════════════════════╝" << RESET << "\n\n";
        
        cout << NEGRITO << "Total de acessos: " << BRANCO << total_acessos << RESET << "\n";
        cout << VERDE << "Acessos bem-sucedidos: " << acessos_bem_sucedidos << RESET << "\n";
        cout << VERMELHO << "Page Faults: " << page_faults << RESET << "\n";
        cout << AMARELO << "Interrupções: " << interrupcoes << RESET << "\n";
        cout << CIANO << setprecision(2) << fixed << "Taxa de acerto: " 
             << taxa_acerto << "%" << RESET << "\n\n";
    }
    
    // Executar a simulação com uma sequência de acessos
    void executar_simulacao(vector<int>& sequencia_acessos) {
        cout << "\n" << NEGRITO << CIANO << "╔════════════════════════════════════════╗\n"
             << "║      INICIANDO SIMULAÇÃO DE ACESSOS    ║\n"
             << "╚════════════════════════════════════════╝" << RESET << "\n";
        
        for (size_t i = 0; i < sequencia_acessos.size(); i++) {
            int pagina = sequencia_acessos[i];
            
            cout << "\n" << NEGRITO << "Acesso #" << (i + 1) << RESET;
            cout << "   CPU solicita acesso à " << AMARELO << "Página " << pagina << RESET << "\n";
            cout << string(50, '-') << "\n";
            
            // Verificar validade da página
            if (tabela_paginas[pagina].bit_validado == 'v') {
                // CASO 1: Acesso bem-sucedido
                cout << VERDE << "  [HIT] Acesso bem-sucedido!" << RESET << "\n";
                cout << "   A página " << pagina << " está no Frame " 
                     << tabela_paginas[pagina].numero_frame << " da RAM.\n";
                acessar_pagina(pagina);
            } else {
                // CASO 2 e 3: Page Fault
                cout << VERMELHO << "  [MISS] PAGE FAULT!" << RESET << "\n";
                cout << "   A página " << pagina << " não está na RAM.\n";
                
                // Verificar se há frames livres
                bool tem_frame_livre = false;
                for (int j = 0; j < tamanho_ram; j++) {
                    if (ram[j] == -1) {
                        tem_frame_livre = true;
                        break;
                    }
                }
                
                if (!tem_frame_livre) {
                    cout << AMARELO << "   [SO] Sem RAM livre, necessário algoritmo de substituição" << RESET << "\n";
                    interrupcoes++;
                    cout << VERMELHO << "   [ABORTO] Acesso encerrado para a Página " << pagina << ".\n" << RESET;
                    continue;
                }

                cout << CIANO << "   [SO] Buscando página " << pagina 
                     << " no disco (Backing Store)...\n";
                cout << "   [SO] Carregando página " << pagina << " para a RAM...\n";

                acessar_pagina(pagina);

                cout << VERDE << "   [RESTART] Instrução reiniciada! Página " << pagina 
                     << " agora está no Frame " << tabela_paginas[pagina].numero_frame << ".\n" << RESET;
            }
        }
        
        exibir_ram();
        exibir_tabela_paginas();
        exibir_estatisticas();
    }
};

int main() {
    srand(time(0));
    
    // Configuraçõesiniciais da simulação
    int TAMANHO_RAM = 5;        // 5 frames na RAM
    int NUMERO_PAGINAS = 6;     // 6 páginas no processo
    
    // Criar o simulador
    SimuladorPageFault simulador(TAMANHO_RAM, NUMERO_PAGINAS);
    
    // Exibir estado inicial
    cout << "\n" << NEGRITO << CIANO << "╔════════════════════════════════════════╗\n"
         << "║   SIMULADOR DE PAGE FAULT - SO (C++)   ║\n"
         << "║         Gerenciamento de Memória       ║\n"
         << "╚════════════════════════════════════════╝" << RESET << "\n";
    
    cout << "\nConfigurações:\n";
    cout << "  • Tamanho da RAM: " << NEGRITO << TAMANHO_RAM << " frames" << RESET << "\n";
    cout << "  • Número de páginas: " << NEGRITO << NUMERO_PAGINAS << RESET << "\n";
    
    simulador.exibir_ram();
    simulador.exibir_tabela_paginas();
    
    // Sequência de acessos (exemplo do enunciado)
    vector<int> sequencia_acessos = {2, 5, 1, 3, 2, 4, 5, 0, 3, 4};
    
    // Executar a simulação
    simulador.executar_simulacao(sequencia_acessos);
    
    cout << "\n" << NEGRITO << CIANO << "╔════════════════════════════════════════╗\n"
         << "║         SIMULAÇÃO FINALIZADA!          ║\n"
         << "╚════════════════════════════════════════╝" << RESET << "\n\n";
    
    return 0;
}
