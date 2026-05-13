
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


#define MAX_USUARIOS      100
#define MAX_AGENDAMENTOS  500
#define TAM_NOME           60
#define TAM_CPF            15
#define TAM_SENHA          30
#define TAM_TELEFONE       20
#define TAM_DATA           11
#define TAM_OBS            80

#define ARQUIVO_USUARIOS     "usuarios.dat"
#define ARQUIVO_AGENDAMENTOS "agendamentos.dat"

/* Slots de 08:00 às 17:30, de 30 em 30 minutos */
#define TOTAL_SLOTS  20
static const char *SLOTS[TOTAL_SLOTS] = {
    "08:00","08:30","09:00","09:30","10:00","10:30",
    "11:00","11:30","12:00","12:30","13:00","13:30",
    "14:00","14:30","15:00","15:30","16:00","16:30",
    "17:00","17:30"
};

typedef enum { CLIENTE = 1, BARBEIRO, ADMIN } TipoPerfil;

typedef enum {
    SVC_CABELO       = 1,
    SVC_SOBRANCELHA  = 2,
    SVC_CABELO_SBH   = 3,
    SVC_BARBA        = 4,
    SVC_CABELO_BARBA = 5
} TipoServico;

typedef enum {
    AG_PENDENTE  = 1,
    AG_CONCLUIDO,
    AG_CANCELADO
} StatusAgendamento;

typedef struct {
    int        id;
    char       nome[TAM_NOME];
    char       cpf[TAM_CPF];
    char       senha[TAM_SENHA];
    char       telefone[TAM_TELEFONE];
    TipoPerfil perfil;
    int        ativo;
} Usuario;

typedef struct {
    int               id;
    int               id_cliente;
    int               id_barbeiro;
    char              data[TAM_DATA];
    int               slot;
    TipoServico       servico;
    StatusAgendamento status;
    char              obs[TAM_OBS];
    int               ativo;
} Agendamento;

typedef struct {
    TipoServico id;
    const char *nome;
    float       preco;
} InfoServico;

static Usuario     usuarios[MAX_USUARIOS];
static int         total_usr   = 0;
static int         prox_id_usr = 1;

static Agendamento agendamentos[MAX_AGENDAMENTOS];
static int         total_ag    = 0;
static int         prox_id_ag  = 1;

static Usuario    *logado = NULL;

static const InfoServico SERVICOS[] = {
    { SVC_CABELO,       "Corte de Cabelo",               40.0f },
    { SVC_SOBRANCELHA,  "Corte de Sobrancelha",           5.0f },
    { SVC_CABELO_SBH,   "Corte de Cabelo + Sobrancelha", 50.0f },
    { SVC_BARBA,        "Corte de Barba",                30.0f },
    { SVC_CABELO_BARBA, "Corte de Cabelo + Barba",       70.0f }
};
#define N_SERVICOS 5


void limpar_tela(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pausar(void) {
    printf("\n  Pressione ENTER para continuar...");
    while (getchar() != '\n');
}

void linha(char c, int n) {
    for (int i = 0; i < n; i++) putchar(c);
    putchar('\n');
}

void cabecalho(const char *titulo) {
    limpar_tela();
    linha('=', 60);
    printf("  BARBEARIA SISTEMA  --  %s\n", titulo);
    linha('=', 60);
    putchar('\n');
}

void ler_string(const char *prompt, char *dest, int tam) {
    printf("  %s", prompt);
    fflush(stdout);
    if (fgets(dest, tam, stdin))
        dest[strcspn(dest, "\n")] = '\0';
}

void ler_senha(const char *prompt, char *dest, int tam) {
    printf("  %s", prompt);
    fflush(stdout);
    if (fgets(dest, tam, stdin))
        dest[strcspn(dest, "\n")] = '\0';
}

int ler_int(const char *prompt) {
    printf("  %s", prompt);
    int v = 0;
    if (scanf("%d", &v) != 1) v = 0;
    while (getchar() != '\n');
    return v;
}


/* Verifica apenas a máscara: "NNN.NNN.NNN-NN" ou 11 dígitos crus */
int validar_cpf_formato(const char *cpf) {
    int len = (int)strlen(cpf);
    if (len == 14) {
        for (int i = 0; i < 14; i++) {
            if      (i == 3 || i == 7) { if (cpf[i] != '.') return 0; }
            else if (i == 11)          { if (cpf[i] != '-') return 0; }
            else                       { if (!isdigit((unsigned char)cpf[i])) return 0; }
        }
        return 1;
    }
    if (len == 11) {
        for (int i = 0; i < 11; i++)
            if (!isdigit((unsigned char)cpf[i])) return 0;
        return 1;
    }
    return 0;
}

/*
 * Valida os dois dígitos verificadores do CPF.
 * Deve ser chamada APÓS formatar_cpf(), pois espera o formato
 * "NNN.NNN.NNN-NN" (ou 11 dígitos puros).
 *
 * Algoritmo:
 *   1º dígito: soma ponderada dos 9 primeiros dígitos (pesos 10..2),
 *              resto = soma % 11; dígito = (resto < 2) ? 0 : 11 - resto
 *   2º dígito: soma ponderada dos 10 primeiros dígitos (pesos 11..2),
 *              mesma regra de cálculo.
 *   Rejeita também CPFs com todos os dígitos iguais (ex.: 111.111.111-11).
 */
int validar_cpf_digitos(const char *cpf_fmt) {
    /* Extrai somente os dígitos */
    char d[12] = {0};
    int j = 0;
    for (int i = 0; cpf_fmt[i] && j < 11; i++)
        if (isdigit((unsigned char)cpf_fmt[i])) d[j++] = cpf_fmt[i];
    if (j != 11) return 0;

    /* Rejeita sequências como "111.111.111-11" */
    int todos_iguais = 1;
    for (int i = 1; i < 11; i++)
        if (d[i] != d[0]) { todos_iguais = 0; break; }
    if (todos_iguais) return 0;

    /* 1º dígito verificador — pesos 10..2 sobre os 9 primeiros */
    int soma = 0;
    for (int i = 0; i < 9; i++)
        soma += (d[i] - '0') * (10 - i);
    int r = soma % 11;
    int dig1 = (r < 2) ? 0 : 11 - r;
    if ((d[9] - '0') != dig1) return 0;

    /* 2º dígito verificador — pesos 11..2 sobre os 10 primeiros */
    soma = 0;
    for (int i = 0; i < 10; i++)
        soma += (d[i] - '0') * (11 - i);
    r = soma % 11;
    int dig2 = (r < 2) ? 0 : 11 - r;
    if ((d[10] - '0') != dig2) return 0;

    return 1;
}

void formatar_cpf(const char *entrada, char *saida) {
    char d[12] = {0}; int j = 0;
    for (int i = 0; entrada[i] && j < 11; i++)
        if (isdigit((unsigned char)entrada[i])) d[j++] = entrada[i];
    snprintf(saida, TAM_CPF, "%c%c%c.%c%c%c.%c%c%c-%c%c",
             d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10]);
}

int validar_data(const char *data) {
    if ((int)strlen(data) != 10) return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 2 || i == 5) { if (data[i] != '/') return 0; }
        else { if (!isdigit((unsigned char)data[i])) return 0; }
    }
    int dd = atoi(data);
    int mm = atoi(data + 3);
    int aa = atoi(data + 6);
    return (dd >= 1 && dd <= 31 && mm >= 1 && mm <= 12 && aa >= 2024);
}

void data_hoje(char *buf) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    snprintf(buf, TAM_DATA, "%02d/%02d/%04d",
             tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900);
}

/* Converte DD/MM/AAAA para AAAAMMDD para comparação */
int cmp_data(const char *a, const char *b) {
    char fa[9], fb[9];
    snprintf(fa, 9, "%c%c%c%c%c%c%c%c",
             a[6],a[7],a[8],a[9],a[3],a[4],a[0],a[1]);
    snprintf(fb, 9, "%c%c%c%c%c%c%c%c",
             b[6],b[7],b[8],b[9],b[3],b[4],b[0],b[1]);
    return strcmp(fa, fb);
}

const InfoServico *info_servico(TipoServico s) {
    for (int i = 0; i < N_SERVICOS; i++)
        if (SERVICOS[i].id == s) return &SERVICOS[i];
    return &SERVICOS[0];
}

void listar_servicos_tabela(void) {
    printf("  +------+-------------------------------------+---------+\n");
    printf("  |  No  |  Servico                            |  Preco  |\n");
    printf("  +------+-------------------------------------+---------+\n");
    for (int i = 0; i < N_SERVICOS; i++)
        printf("  |  %-3d |  %-35s |  R$%3.0f  |\n",
               SERVICOS[i].id, SERVICOS[i].nome, (double)SERVICOS[i].preco);
    printf("  +------+-------------------------------------+---------+\n");
}


void salvar_usuarios(void) {
    FILE *f = fopen(ARQUIVO_USUARIOS, "wb");
    if (!f) { printf("  [ERRO] Nao salvou usuarios.\n"); return; }
    fwrite(&total_usr,   sizeof(int), 1, f);
    fwrite(&prox_id_usr, sizeof(int), 1, f);
    fwrite(usuarios,     sizeof(Usuario), total_usr, f);
    fclose(f);
}

void salvar_agendamentos(void) {
    FILE *f = fopen(ARQUIVO_AGENDAMENTOS, "wb");
    if (!f) { printf("  [ERRO] Nao salvou agendamentos.\n"); return; }
    fwrite(&total_ag,    sizeof(int), 1, f);
    fwrite(&prox_id_ag,  sizeof(int), 1, f);
    fwrite(agendamentos, sizeof(Agendamento), total_ag, f);
    fclose(f);
}

void carregar_usuarios(void) {
    FILE *f = fopen(ARQUIVO_USUARIOS, "rb");
    if (!f) {
        Usuario admin = {0};
        admin.id = prox_id_usr++;
        strcpy(admin.nome,     "Administrador");
        strcpy(admin.cpf,      "000.000.000-00");
        strcpy(admin.senha,    "admin123");
        strcpy(admin.telefone, "(00) 00000-0000");
        admin.perfil = ADMIN;
        admin.ativo  = 1;
        usuarios[total_usr++] = admin;
        salvar_usuarios();
        return;
    }
    fread(&total_usr,   sizeof(int), 1, f);
    fread(&prox_id_usr, sizeof(int), 1, f);
    fread(usuarios,     sizeof(Usuario), total_usr, f);
    fclose(f);
}

void carregar_agendamentos(void) {
    FILE *f = fopen(ARQUIVO_AGENDAMENTOS, "rb");
    if (!f) return;
    fread(&total_ag,    sizeof(int), 1, f);
    fread(&prox_id_ag,  sizeof(int), 1, f);
    fread(agendamentos, sizeof(Agendamento), total_ag, f);
    fclose(f);
}


Usuario *buscar_cpf(const char *cpf) {
    for (int i = 0; i < total_usr; i++)
        if (usuarios[i].ativo && strcmp(usuarios[i].cpf, cpf) == 0)
            return &usuarios[i];
    return NULL;
}

Usuario *buscar_id_usr(int id) {
    for (int i = 0; i < total_usr; i++)
        if (usuarios[i].ativo && usuarios[i].id == id)
            return &usuarios[i];
    return NULL;
}

const char *nome_perfil(TipoPerfil p) {
    switch (p) {
        case CLIENTE:  return "Cliente";
        case BARBEIRO: return "Barbeiro";
        case ADMIN:    return "Admin";
        default:       return "?";
    }
}

const char *status_str(StatusAgendamento s) {
    switch (s) {
        case AG_PENDENTE:  return "Pendente";
        case AG_CONCLUIDO: return "Concluido";
        case AG_CANCELADO: return "Cancelado";
        default:           return "?";
    }
}


int fazer_login(void) {
    cabecalho("LOGIN");
    char cpf_raw[TAM_CPF], cpf_fmt[TAM_CPF], senha[TAM_SENHA];

    ler_string("CPF   : ", cpf_raw, TAM_CPF);
    if (!validar_cpf_formato(cpf_raw)) {
        printf("\n  [!] Formato de CPF invalido.\n"); pausar(); return 0;
    }
    formatar_cpf(cpf_raw, cpf_fmt);

    /* O CPF do admin (000.000.000-00) é especial: pula a validação de dígitos */
    if (strcmp(cpf_fmt, "000.000.000-00") != 0 && !validar_cpf_digitos(cpf_fmt)) {
        printf("\n  [!] CPF invalido (digitos verificadores incorretos).\n");
        pausar(); return 0;
    }

    ler_senha("Senha : ", senha, TAM_SENHA);

    Usuario *u = buscar_cpf(cpf_fmt);
    if (u && strcmp(u->senha, senha) == 0) {
        logado = u;
        printf("\n  Bem-vindo, %s! [%s]\n", u->nome, nome_perfil(u->perfil));
        pausar(); return 1;
    }
    printf("\n  [!] CPF ou senha incorretos.\n");
    pausar(); return 0;
}


void imprimir_usuario(const Usuario *u) {
    printf("  +---------------------------------------------------+\n");
    printf("  | ID      : %-38d  |\n", u->id);
    printf("  | Nome    : %-38s  |\n", u->nome);
    printf("  | CPF     : %-38s  |\n", u->cpf);
    printf("  | Tel.    : %-38s  |\n", u->telefone);
    printf("  | Perfil  : %-38s  |\n", nome_perfil(u->perfil));
    printf("  +---------------------------------------------------+\n");
}

void cadastrar_usuario(void) {
    cabecalho("CADASTRO DE USUARIO");
    if (total_usr >= MAX_USUARIOS) {
        printf("  [!] Limite atingido.\n"); pausar(); return;
    }

    Usuario novo = {0};
    char cpf_raw[TAM_CPF], cpf_fmt[TAM_CPF], confirma[TAM_SENHA];

    ler_string("Nome completo  : ", novo.nome, TAM_NOME);
    if (strlen(novo.nome) < 3) { printf("  [!] Nome muito curto.\n"); pausar(); return; }

    ler_string("CPF            : ", cpf_raw, TAM_CPF);
    if (!validar_cpf_formato(cpf_raw)) {
        printf("  [!] CPF invalido.\n"); pausar(); return;
    }
    formatar_cpf(cpf_raw, cpf_fmt);
    if (!validar_cpf_digitos(cpf_fmt)) {
        printf("  [!] CPF invalido (digitos verificadores incorretos).\n");
        pausar(); return;
    }
    if (buscar_cpf(cpf_fmt)) { printf("  [!] CPF ja cadastrado.\n"); pausar(); return; }
    strcpy(novo.cpf, cpf_fmt);

    ler_senha("Senha          : ", novo.senha, TAM_SENHA);
    if (strlen(novo.senha) < 4) { printf("  [!] Minimo 4 caracteres.\n"); pausar(); return; }
    ler_senha("Confirma senha : ", confirma, TAM_SENHA);
    if (strcmp(novo.senha, confirma) != 0) { printf("  [!] Senhas diferentes.\n"); pausar(); return; }

    ler_string("Telefone       : ", novo.telefone, TAM_TELEFONE);

    int perfil_max = (logado && logado->perfil == ADMIN) ? 3 : 1;
    printf("\n  Perfil:\n    1 - Cliente\n");
    if (perfil_max >= 2) printf("    2 - Barbeiro\n");
    if (perfil_max >= 3) printf("    3 - Admin\n");
    int op = ler_int("Opcao [1]: ");
    if (op < 1 || op > perfil_max) op = 1;
    novo.perfil = (TipoPerfil)op;

    novo.id    = prox_id_usr++;
    novo.ativo = 1;
    usuarios[total_usr++] = novo;
    salvar_usuarios();
    printf("\n  Cadastro realizado! (ID: %d)\n", novo.id);
    pausar();
}

void cadastrar_barbeiro(void) {
    cabecalho("CADASTRAR BARBEIRO");
    if (total_usr >= MAX_USUARIOS) {
        printf("  [!] Limite de usuarios atingido.\n"); pausar(); return;
    }

    Usuario novo = {0};
    char cpf_raw[TAM_CPF], cpf_fmt[TAM_CPF], confirma[TAM_SENHA];

    ler_string("Nome completo  : ", novo.nome, TAM_NOME);
    if (strlen(novo.nome) < 3) { printf("  [!] Nome muito curto.\n"); pausar(); return; }

    ler_string("CPF            : ", cpf_raw, TAM_CPF);
    if (!validar_cpf_formato(cpf_raw)) {
        printf("  [!] CPF invalido.\n"); pausar(); return;
    }
    formatar_cpf(cpf_raw, cpf_fmt);
    if (!validar_cpf_digitos(cpf_fmt)) {
        printf("  [!] CPF invalido (digitos verificadores incorretos).\n");
        pausar(); return;
    }
    if (buscar_cpf(cpf_fmt)) { printf("  [!] CPF ja cadastrado.\n"); pausar(); return; }
    strcpy(novo.cpf, cpf_fmt);

    ler_senha("Senha          : ", novo.senha, TAM_SENHA);
    if (strlen(novo.senha) < 4) { printf("  [!] Minimo 4 caracteres.\n"); pausar(); return; }
    ler_senha("Confirma senha : ", confirma, TAM_SENHA);
    if (strcmp(novo.senha, confirma) != 0) { printf("  [!] Senhas diferentes.\n"); pausar(); return; }

    ler_string("Telefone       : ", novo.telefone, TAM_TELEFONE);

    novo.perfil = BARBEIRO;
    novo.id     = prox_id_usr++;
    novo.ativo  = 1;
    usuarios[total_usr++] = novo;
    salvar_usuarios();
    printf("\n  Barbeiro cadastrado com sucesso! (ID: %d)\n", novo.id);
    pausar();
}

void listar_usuarios(void) {
    cabecalho("LISTA DE USUARIOS");
    int ok = 0;
    for (int i = 0; i < total_usr; i++)
        if (usuarios[i].ativo) { imprimir_usuario(&usuarios[i]); putchar('\n'); ok = 1; }
    if (!ok) printf("  Nenhum usuario cadastrado.\n");
    pausar();
}

void buscar_usuario(void) {
    cabecalho("BUSCAR USUARIO");
    printf("  1 - Por CPF\n  2 - Por ID\n");
    int op = ler_int("Opcao: ");
    Usuario *u = NULL;
    if (op == 1) {
        char r[TAM_CPF], f[TAM_CPF];
        ler_string("CPF: ", r, TAM_CPF);
        if (!validar_cpf_formato(r)) { printf("  [!] CPF invalido.\n"); pausar(); return; }
        formatar_cpf(r, f);
        if (!validar_cpf_digitos(f)) {
            printf("  [!] CPF invalido (digitos verificadores incorretos).\n");
            pausar(); return;
        }
        u = buscar_cpf(f);
    } else {
        int id = ler_int("ID: "); u = buscar_id_usr(id);
    }
    if (u) { putchar('\n'); imprimir_usuario(u); }
    else     printf("  [!] Nao encontrado.\n");
    pausar();
}

void excluir_usuario(void) {
    cabecalho("EXCLUIR USUARIO");
    char r[TAM_CPF], f[TAM_CPF], senha[TAM_SENHA];

    ler_string("CPF do usuario: ", r, TAM_CPF);
    if (!validar_cpf_formato(r)) { printf("  [!] CPF invalido.\n"); pausar(); return; }
    formatar_cpf(r, f);

    /* CPF do admin (000.000.000-00) dispensa validação de dígitos */
    if (strcmp(f, "000.000.000-00") != 0 && !validar_cpf_digitos(f)) {
        printf("  [!] CPF invalido (digitos verificadores incorretos).\n");
        pausar(); return;
    }

    Usuario *u = buscar_cpf(f);
    if (!u) { printf("  [!] Usuario nao encontrado.\n"); pausar(); return; }

    if (!logado || logado->perfil != ADMIN) {
        ler_senha("Confirme sua senha: ", senha, TAM_SENHA);
        if (strcmp(u->senha, senha) != 0) { printf("  [!] Senha incorreta.\n"); pausar(); return; }
    }
    if (u->perfil == ADMIN) {
        int nadm = 0;
        for (int i = 0; i < total_usr; i++)
            if (usuarios[i].ativo && usuarios[i].perfil == ADMIN) nadm++;
        if (nadm <= 1) {
            printf("  [!] Impossivel excluir o unico administrador.\n");
            pausar(); return;
        }
    }
    putchar('\n'); imprimir_usuario(u);
    printf("\n  Confirmar exclusao? (s/N): ");
    char conf[4]; fgets(conf, sizeof(conf), stdin);
    if (tolower((unsigned char)conf[0]) != 's') {
        printf("  Cancelado.\n"); pausar(); return;
    }

    /* Cancela agendamentos pendentes do usuario */
    for (int i = 0; i < total_ag; i++)
        if (agendamentos[i].ativo && agendamentos[i].id_cliente == u->id
            && agendamentos[i].status == AG_PENDENTE)
            agendamentos[i].status = AG_CANCELADO;
    salvar_agendamentos();

    u->ativo = 0;
    if (logado && logado->id == u->id) logado = NULL;
    salvar_usuarios();
    printf("\n  Usuario excluido.\n");
    pausar();
}

void alterar_senha(void) {
    cabecalho("ALTERAR SENHA");
    if (!logado) { printf("  [!] Nenhum usuario logado.\n"); pausar(); return; }
    char atual[TAM_SENHA], nova[TAM_SENHA], conf[TAM_SENHA];
    ler_senha("Senha atual   : ", atual, TAM_SENHA);
    if (strcmp(logado->senha, atual) != 0) { printf("  [!] Senha incorreta.\n"); pausar(); return; }
    ler_senha("Nova senha    : ", nova, TAM_SENHA);
    if (strlen(nova) < 4) { printf("  [!] Minimo 4 caracteres.\n"); pausar(); return; }
    ler_senha("Confirma      : ", conf, TAM_SENHA);
    if (strcmp(nova, conf) != 0) { printf("  [!] Senhas diferentes.\n"); pausar(); return; }
    strcpy(logado->senha, nova);
    salvar_usuarios();
    printf("\n  Senha alterada com sucesso.\n"); pausar();
}


void imprimir_agendamento(const Agendamento *ag) {
    Usuario *cli  = buscar_id_usr(ag->id_cliente);
    Usuario *barb = ag->id_barbeiro ? buscar_id_usr(ag->id_barbeiro) : NULL;
    const InfoServico *svc = info_servico(ag->servico);

    printf("  +---------------------------------------------------+\n");
    printf("  | ID Agend. : %-38d  |\n", ag->id);
    printf("  | Data      : %-38s  |\n", ag->data);
    printf("  | Horario   : %-38s  |\n", SLOTS[ag->slot]);
    printf("  | Servico   : %-38s  |\n", svc->nome);
    printf("  | Valor     : R$ %-35.2f  |\n", (double)svc->preco);
    printf("  | Cliente   : %-38s  |\n", cli ? cli->nome : "?");
    printf("  | Barbeiro  : %-38s  |\n", barb ? barb->nome : "Qualquer disponivel");
    printf("  | Status    : %-38s  |\n", status_str(ag->status));
    if (ag->obs[0])
        printf("  | Obs.      : %-38s  |\n", ag->obs);
    printf("  +---------------------------------------------------+\n");
}


int slot_ocupado(const char *data, int slot, int id_barbeiro, int ignorar_id) {
    for (int i = 0; i < total_ag; i++) {
        Agendamento *ag = &agendamentos[i];
        if (!ag->ativo || ag->id == ignorar_id) continue;
        if (ag->status == AG_CANCELADO) continue;
        if (strcmp(ag->data, data) != 0 || ag->slot != slot) continue;
        if (id_barbeiro > 0 && ag->id_barbeiro > 0 && ag->id_barbeiro != id_barbeiro) continue;
        return 1;
    }
    return 0;
}


void criar_agendamento(void) {
    cabecalho("NOVO AGENDAMENTO");

    if (total_ag >= MAX_AGENDAMENTOS) {
        printf("  [!] Limite de agendamentos atingido.\n"); pausar(); return;
    }

    Agendamento novo = {0};

    /* Define o cliente */
    if (logado->perfil == ADMIN) {
        printf("  Agendar para:\n    1 - Mim mesmo\n    2 - Um cliente (CPF)\n");
        int op = ler_int("Opcao: ");
        if (op == 2) {
            char r[TAM_CPF], f[TAM_CPF];
            ler_string("CPF do cliente: ", r, TAM_CPF);
            if (!validar_cpf_formato(r)) { printf("  [!] CPF invalido.\n"); pausar(); return; }
            formatar_cpf(r, f);
            if (!validar_cpf_digitos(f)) {
                printf("  [!] CPF invalido (digitos verificadores incorretos).\n");
                pausar(); return;
            }
            Usuario *cli = buscar_cpf(f);
            if (!cli) { printf("  [!] Cliente nao encontrado.\n"); pausar(); return; }
            novo.id_cliente = cli->id;
        } else {
            novo.id_cliente = logado->id;
        }
    } else {
        novo.id_cliente = logado->id;
    }

    char hoje[TAM_DATA];
    data_hoje(hoje);
    printf("\n  Hoje: %s\n", hoje);
    char data[TAM_DATA];
    ler_string("  Data (DD/MM/AAAA): ", data, TAM_DATA);
    if (!validar_data(data)) { printf("  [!] Data invalida.\n"); pausar(); return; }
    if (cmp_data(data, hoje) < 0) { printf("  [!] Data no passado.\n"); pausar(); return; }
    strcpy(novo.data, data);

//serviços
    printf("\n");
    listar_servicos_tabela();
    int svc = ler_int("Escolha o servico [1-5]: ");
    if (svc < 1 || svc > N_SERVICOS) { printf("  [!] Servico invalido.\n"); pausar(); return; }
    novo.servico = (TipoServico)svc;

    /* Slots disponíveis */
    printf("\n  Horarios em %s:\n\n", data);
    int disp = 0;
    for (int s = 0; s < TOTAL_SLOTS; s++) {
        int ocup = slot_ocupado(data, s, 0, -1);
        printf("    %2d - %s  %s\n", s + 1, SLOTS[s], ocup ? "[Ocupado   ]" : "[Disponivel]");
        if (!ocup) disp++;
    }
    if (disp == 0) {
        printf("\n  [!] Sem horarios disponiveis nesse dia.\n");
        pausar(); return;
    }

    int slot_escolha = ler_int("\n  Numero do horario: ");
    if (slot_escolha < 1 || slot_escolha > TOTAL_SLOTS) {
        printf("  [!] Horario invalido.\n"); pausar(); return;
    }
    slot_escolha--;
    if (slot_ocupado(data, slot_escolha, 0, -1)) {
        printf("  [!] Horario ja ocupado.\n"); pausar(); return;
    }
    novo.slot = slot_escolha;

    /* Barbeiro preferido */
    printf("\n  Escolher barbeiro especifico? (s/N): ");
    char c[4]; fgets(c, sizeof(c), stdin);
    novo.id_barbeiro = 0;
    if (tolower((unsigned char)c[0]) == 's') {
        printf("\n  Barbeiros:\n");
        int nb = 0;
        for (int i = 0; i < total_usr; i++)
            if (usuarios[i].ativo && usuarios[i].perfil == BARBEIRO) {
                printf("    ID %-3d - %s\n", usuarios[i].id, usuarios[i].nome);
                nb++;
            }
        if (nb == 0) {
            printf("  Nenhum barbeiro cadastrado.\n");
        } else {
            int bid = ler_int("  ID do barbeiro (0 = qualquer): ");
            if (bid > 0) {
                Usuario *b = buscar_id_usr(bid);
                if (b && b->perfil == BARBEIRO)
                    novo.id_barbeiro = bid;
                else
                    printf("  Barbeiro nao encontrado. Agendando para qualquer disponivel.\n");
            }
        }
    }

    /* Observações */
    ler_string("Observacoes (Enter para pular): ", novo.obs, TAM_OBS);

    novo.id     = prox_id_ag++;
    novo.status = AG_PENDENTE;
    novo.ativo  = 1;
    agendamentos[total_ag++] = novo;
    salvar_agendamentos();

    printf("\n  Agendamento realizado!\n\n");
    imprimir_agendamento(&agendamentos[total_ag - 1]);
    pausar();
}


void listar_agendamentos_filtrado(int id_cliente, int id_barbeiro,
                                   const char *data_filtro, int so_pendentes) {
    int ok = 0;
    for (int i = 0; i < total_ag; i++) {
        Agendamento *ag = &agendamentos[i];
        if (!ag->ativo) continue;
        if (id_cliente  > 0 && ag->id_cliente  != id_cliente)  continue;
        if (id_barbeiro > 0 && ag->id_barbeiro != id_barbeiro) continue;
        if (data_filtro && data_filtro[0] && strcmp(ag->data, data_filtro) != 0) continue;
        if (so_pendentes && ag->status != AG_PENDENTE) continue;
        imprimir_agendamento(ag); putchar('\n'); ok = 1;
    }
    if (!ok) printf("  Nenhum agendamento encontrado.\n");
}

void meus_agendamentos(void) {
    cabecalho("MEUS AGENDAMENTOS");
    printf("  1 - Todos\n  2 - Apenas pendentes\n  3 - Por data\n");
    int op = ler_int("Opcao: ");
    char data_f[TAM_DATA] = {0};
    int pend = 0;
    if (op == 2) pend = 1;
    if (op == 3) ler_string("Data (DD/MM/AAAA): ", data_f, TAM_DATA);
    listar_agendamentos_filtrado(logado->id, 0, data_f[0] ? data_f : NULL, pend);
    pausar();
}

void agenda_barbeiro(void) {
    cabecalho("MINHA AGENDA");
    printf("  1 - Hoje\n  2 - Por data\n  3 - Todos os pendentes\n");
    int op = ler_int("Opcao: ");
    char data_f[TAM_DATA] = {0};
    int pend = 0;
    if      (op == 1) { data_hoje(data_f); }
    else if (op == 2) { ler_string("Data (DD/MM/AAAA): ", data_f, TAM_DATA); }
    else              { pend = 1; }
    listar_agendamentos_filtrado(0, logado->id, data_f[0] ? data_f : NULL, pend);
    pausar();
}

void listar_todos_agendamentos(void) {
    cabecalho("TODOS OS AGENDAMENTOS");
    printf("  1 - Todos\n  2 - Pendentes\n  3 - Por data\n  4 - Por cliente (CPF)\n");
    int op = ler_int("Opcao: ");
    char data_f[TAM_DATA] = {0};
    int pend = 0, id_cli = 0;
    if      (op == 2) pend = 1;
    else if (op == 3) ler_string("Data (DD/MM/AAAA): ", data_f, TAM_DATA);
    else if (op == 4) {
        char r[TAM_CPF], f[TAM_CPF];
        ler_string("CPF do cliente: ", r, TAM_CPF);
        if (validar_cpf_formato(r)) {
            formatar_cpf(r, f);
            if (!validar_cpf_digitos(f)) {
                printf("  [!] CPF invalido (digitos verificadores incorretos).\n");
                pausar(); return;
            }
            Usuario *u = buscar_cpf(f);
            if (u) id_cli = u->id;
            else { printf("  [!] Cliente nao encontrado.\n"); pausar(); return; }
        }
    }
    listar_agendamentos_filtrado(id_cli, 0, data_f[0] ? data_f : NULL, pend);
    pausar();
}


Agendamento *buscar_ag_por_id(int id) {
    for (int i = 0; i < total_ag; i++)
        if (agendamentos[i].ativo && agendamentos[i].id == id)
            return &agendamentos[i];
    return NULL;
}

void cancelar_agendamento(void) {
    cabecalho("CANCELAR AGENDAMENTO");

    if (logado->perfil == CLIENTE) {
        printf("  Seus agendamentos pendentes:\n\n");
        listar_agendamentos_filtrado(logado->id, 0, NULL, 1);
    } else {
        printf("  Agendamentos pendentes:\n\n");
        listar_agendamentos_filtrado(0, 0, NULL, 1);
    }

    int id = ler_int("\n  ID do agendamento (0 = voltar): ");
    if (id == 0) return;

    Agendamento *ag = buscar_ag_por_id(id);
    if (!ag) { printf("  [!] Nao encontrado.\n"); pausar(); return; }
    if (ag->status != AG_PENDENTE) {
        printf("  [!] So e possivel cancelar agendamentos pendentes.\n");
        pausar(); return;
    }
    if (logado->perfil == CLIENTE && ag->id_cliente != logado->id) {
        printf("  [!] Voce so pode cancelar seus proprios agendamentos.\n");
        pausar(); return;
    }

    putchar('\n'); imprimir_agendamento(ag);
    printf("\n  Confirmar cancelamento? (s/N): ");
    char conf[4]; fgets(conf, sizeof(conf), stdin);
    if (tolower((unsigned char)conf[0]) != 's') {
        printf("  Operacao cancelada.\n"); pausar(); return;
    }

    ag->status = AG_CANCELADO;
    salvar_agendamentos();
    printf("\n  Agendamento cancelado.\n");
    pausar();
}

void concluir_agendamento(void) {
    cabecalho("MARCAR COMO CONCLUIDO");
    int id_barb = (logado->perfil == BARBEIRO) ? logado->id : 0;
    printf("  Agendamentos pendentes:\n\n");
    listar_agendamentos_filtrado(0, id_barb, NULL, 1);

    int id = ler_int("\n  ID do agendamento (0 = voltar): ");
    if (id == 0) return;

    Agendamento *ag = buscar_ag_por_id(id);
    if (!ag || ag->status != AG_PENDENTE) {
        printf("  [!] Agendamento invalido ou nao pendente.\n"); pausar(); return;
    }
    if (logado->perfil == BARBEIRO && ag->id_barbeiro != 0 && ag->id_barbeiro != logado->id) {
        printf("  [!] Esse agendamento nao esta na sua agenda.\n"); pausar(); return;
    }
    ag->status = AG_CONCLUIDO;
    salvar_agendamentos();
    printf("\n  Agendamento concluido!\n");
    pausar();
}

void excluir_agendamento_permanente(void) {
    cabecalho("EXCLUIR AGENDAMENTO (PERMANENTE)");
    listar_agendamentos_filtrado(0, 0, NULL, 0);

    int id = ler_int("\n  ID do agendamento (0 = voltar): ");
    if (id == 0) return;

    Agendamento *ag = buscar_ag_por_id(id);
    if (!ag) { printf("  [!] Nao encontrado.\n"); pausar(); return; }

    putchar('\n'); imprimir_agendamento(ag);
    printf("\n  Excluir PERMANENTEMENTE? (s/N): ");
    char conf[4]; fgets(conf, sizeof(conf), stdin);
    if (tolower((unsigned char)conf[0]) != 's') {
        printf("  Cancelado.\n"); pausar(); return;
    }
    ag->ativo = 0;
    salvar_agendamentos();
    printf("\n  Agendamento removido do sistema.\n");
    pausar();
}


void relatorio_admin(void) {
    cabecalho("RELATORIO GERAL");

    int pend = 0, conc = 0, canc = 0, hoje_pend = 0;
    float receita = 0.0f;
    char hoje[TAM_DATA]; data_hoje(hoje);

    for (int i = 0; i < total_ag; i++) {
        if (!agendamentos[i].ativo) continue;
        switch (agendamentos[i].status) {
            case AG_PENDENTE:
                pend++;
                if (strcmp(agendamentos[i].data, hoje) == 0) hoje_pend++;
                break;
            case AG_CONCLUIDO:
                conc++;
                receita += info_servico(agendamentos[i].servico)->preco;
                break;
            case AG_CANCELADO: canc++; break;
        }
    }

    int usr_a = 0, barb_a = 0, cli_a = 0, adm_a = 0;
    for (int i = 0; i < total_usr; i++) {
        if (!usuarios[i].ativo) continue;
        usr_a++;
        if (usuarios[i].perfil == BARBEIRO) barb_a++;
        if (usuarios[i].perfil == CLIENTE)  cli_a++;
        if (usuarios[i].perfil == ADMIN)    adm_a++;
    }

    printf("  === USUARIOS ================================\n");
    printf("  Total ativos : %d\n", usr_a);
    printf("  Clientes     : %d\n", cli_a);
    printf("  Barbeiros    : %d\n", barb_a);
    printf("  Admins       : %d\n", adm_a);
    printf("\n  === AGENDAMENTOS ============================\n");
    printf("  Hoje (pend.) : %d\n", hoje_pend);
    printf("  Pendentes    : %d\n", pend);
    printf("  Concluidos   : %d\n", conc);
    printf("  Cancelados   : %d\n", canc);
    printf("\n  === FINANCEIRO ==============================\n");
    printf("  Receita total: R$ %.2f\n", (double)receita);
    printf("  ============================================\n");
    pausar();
}

void menu_cliente(void) {
    int op;
    do {
        cabecalho("MENU CLIENTE");
        printf("  Ola, %s!\n\n", logado->nome);
        printf("  -- Agendamentos ---------------------------\n");
        printf("  1  - Novo agendamento\n");
        printf("  2  - Meus agendamentos\n");
        printf("  3  - Cancelar agendamento\n");
        printf("  -- Minha Conta ----------------------------\n");
        printf("  4  - Ver meu perfil\n");
        printf("  5  - Alterar senha\n");
        printf("  6  - Excluir minha conta\n");
        printf("  -------------------------------------------\n");
        printf("  0  - Sair\n\n");
        op = ler_int("Opcao: ");
        switch (op) {
            case 1: criar_agendamento();    break;
            case 2: meus_agendamentos();    break;
            case 3: cancelar_agendamento(); break;
            case 4:
                cabecalho("MEU PERFIL");
                imprimir_usuario(logado); pausar(); break;
            case 5: alterar_senha();  break;
            case 6: excluir_usuario(); if (!logado) op = 0; break;
        }
    } while (op != 0);
    logado = NULL;
}


void menu_barbeiro(void) {
    int op;
    do {
        cabecalho("MENU BARBEIRO");
        printf("  Ola, %s!\n\n", logado->nome);
        printf("  -- Agenda ---------------------------------\n");
        printf("  1  - Ver minha agenda\n");
        printf("  2  - Marcar servico como concluido\n");
        printf("  3  - Cancelar agendamento\n");
        printf("  -- Minha Conta ----------------------------\n");
        printf("  4  - Ver meu perfil\n");
        printf("  5  - Alterar senha\n");
        printf("  -------------------------------------------\n");
        printf("  0  - Sair\n\n");
        op = ler_int("Opcao: ");
        switch (op) {
            case 1: agenda_barbeiro();      break;
            case 2: concluir_agendamento(); break;
            case 3: cancelar_agendamento(); break;
            case 4:
                cabecalho("MEU PERFIL");
                imprimir_usuario(logado); pausar(); break;
            case 5: alterar_senha(); break;
        }
    } while (op != 0);
    logado = NULL;
}

void menu_admin(void) {
    int op;
    do {
        cabecalho("MENU ADMINISTRADOR");
        printf("  Ola, %s!\n\n", logado->nome);
        printf("  -- Usuarios --------------------------------\n");
        printf("  1  - Cadastrar cliente\n");
        printf("  2  - Cadastrar barbeiro\n");
        printf("  3  - Listar usuarios\n");
        printf("  4  - Buscar usuario\n");
        printf("  5  - Excluir usuario\n");
        printf("  -- Agendamentos ----------------------------\n");
        printf("  6  - Novo agendamento\n");
        printf("  7  - Ver todos os agendamentos\n");
        printf("  8  - Cancelar agendamento\n");
        printf("  9  - Marcar como concluido\n");
        printf("  10 - Excluir agendamento (permanente)\n");
        printf("  -- Sistema ---------------------------------\n");
        printf("  11 - Relatorio geral\n");
        printf("  12 - Alterar minha senha\n");
        printf("  --------------------------------------------\n");
        printf("  0  - Sair\n\n");
        op = ler_int("Opcao: ");
        switch (op) {
            case 1:  cadastrar_usuario();              break;
            case 2:  cadastrar_barbeiro();             break;
            case 3:  listar_usuarios();                break;
            case 4:  buscar_usuario();                 break;
            case 5:  excluir_usuario();                break;
            case 6:  criar_agendamento();              break;
            case 7:  listar_todos_agendamentos();      break;
            case 8:  cancelar_agendamento();           break;
            case 9:  concluir_agendamento();           break;
            case 10: excluir_agendamento_permanente(); break;
            case 11: relatorio_admin();                break;
            case 12: alterar_senha();                  break;
        }
    } while (op != 0);
    logado = NULL;
}


void menu_principal(void) {
    int op;
    do {
        cabecalho("INICIO");
        printf("  1 - Login\n");
        printf("  2 - Cadastrar-se (novo cliente)\n");
        printf("  0 - Sair\n\n");
        op = ler_int("Opcao: ");
        switch (op) {
            case 1:
                if (fazer_login()) {
                    switch (logado->perfil) {
                        case ADMIN:    menu_admin();    break;
                        case BARBEIRO: menu_barbeiro(); break;
                        case CLIENTE:  menu_cliente();  break;
                    }
                }
                break;
            case 2:
                cadastrar_usuario();
                break;
        }
    } while (op != 0);
}

int main(void) {
    carregar_usuarios();
    carregar_agendamentos();

    cabecalho("BEM-VINDO");
    printf("  Sistema de Gestao de Barbearia  --  v2.2\n\n");
    printf("  Acesso admin padrao:\n");
    printf("    CPF   : 000.000.000-00\n");
    printf("    Senha : admin123\n");
    pausar();

    menu_principal();

    limpar_tela();
    printf("  Obrigado por usar o Sistema de Barbearia!\n\n");
    return 0;
}
