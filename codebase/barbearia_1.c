/*
 * Sistema de Gestao de Barbearia v3.0
 * Estruturas de Dados: Pilha (Stack) e Fila (Queue)
 *
 * PILHA (LIFO - Last In, First Out):
 *   Usada para historico de acoes (ex: desfazer operacoes).
 *   push() insere no topo | pop() remove do topo.
 *
 * FILA (FIFO - First In, First Out):
 *   Usada para fila de espera de atendimento.
 *   enqueue() insere no final | dequeue() remove do inicio.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── Constantes ─────────────────────────────────────────── */
#define MAX_USR      100
#define MAX_AG       500
#define TAM_NOME      60
#define TAM_CPF       15
#define TAM_SENHA     30
#define TAM_TEL       20
#define TAM_DATA      11
#define TAM_OBS       80
#define MAX_FALHAS     5
#define TOTAL_SLOTS   20
#define N_SVC          5

#define ARQ_USR  "usuarios.dat"
#define ARQ_AG   "agendamentos.dat"

/* ─── Slots de horario ───────────────────────────────────── */
static const char *SLOTS[TOTAL_SLOTS] = {
    "08:00","08:30","09:00","09:30","10:00","10:30",
    "11:00","11:30","12:00","12:30","13:00","13:30",
    "14:00","14:30","15:00","15:30","16:00","16:30",
    "17:00","17:30"
};

/* ─── Enums ───────────────────────────────────────────────── */
typedef enum { CLIENTE=1, BARBEIRO, ADMIN }              Perfil;
typedef enum { SVC_CAB=1, SVC_SBH, SVC_CAB_SBH,
               SVC_BARBA, SVC_CAB_BARBA }                Servico;
typedef enum { AG_PEND=1, AG_OK, AG_CANCEL }             StatusAg;

/* ─── Structs principais ──────────────────────────────────── */
typedef struct {
    int id;
    char nome[TAM_NOME], cpf[TAM_CPF],
         senha[TAM_SENHA], tel[TAM_TEL];
    Perfil perfil;
    int ativo;
} Usuario;

typedef struct {
    int id, id_cli, id_barb, slot;
    char data[TAM_DATA], obs[TAM_OBS];
    Servico svc;
    StatusAg status;
    int ativo;
} Agendamento;

typedef struct { Servico id; const char *nome; float preco; } InfoSvc;

/* ─────────────────────────────────────────────────────────────
 *  PILHA (Stack) — historico de IDs de agendamentos alterados
 *  Principio LIFO: o ultimo elemento inserido e o primeiro removido.
 * ──────────────────────────────────────────────────────────── */
#define PILHA_MAX 50
typedef struct {
    int dados[PILHA_MAX];
    int topo;           /* indice do proximo espaco livre */
} Pilha;

/* Inicializa pilha vazia (topo = 0) */
static void pilha_init(Pilha *p) { p->topo = 0; }

/* push: insere valor no topo; retorna 0 se cheia */
static int pilha_push(Pilha *p, int val) {
    if (p->topo >= PILHA_MAX) return 0;
    p->dados[p->topo++] = val;  /* armazena e incrementa topo */
    return 1;
}

/* pop: remove e retorna o valor do topo; retorna -1 se vazia */
static int pilha_pop(Pilha *p) {
    if (p->topo == 0) return -1;
    return p->dados[--p->topo];  /* decrementa topo e retorna valor */
}

/* ─────────────────────────────────────────────────────────────
 *  FILA (Queue) — ordem de espera dos clientes
 *  Principio FIFO: o primeiro elemento inserido e o primeiro removido.
 * ──────────────────────────────────────────────────────────── */
#define FILA_MAX 50
typedef struct {
    int dados[FILA_MAX];
    int inicio, fim, tamanho;
} Fila;

/* Inicializa fila vazia */
static void fila_init(Fila *f) { f->inicio = f->fim = f->tamanho = 0; }

/* enqueue: insere no final da fila usando aritmetica circular */
static int fila_enqueue(Fila *f, int val) {
    if (f->tamanho >= FILA_MAX) return 0;
    f->dados[f->fim] = val;
    f->fim = (f->fim + 1) % FILA_MAX;  /* avanca circulando no array */
    f->tamanho++;
    return 1;
}

/* dequeue: remove e retorna o elemento do inicio da fila */
static int fila_dequeue(Fila *f) {
    if (f->tamanho == 0) return -1;
    int val = f->dados[f->inicio];
    f->inicio = (f->inicio + 1) % FILA_MAX;  /* avanca circulando */
    f->tamanho--;
    return val;
}

/* ─── Dados globais ───────────────────────────────────────── */
static Usuario    usuarios[MAX_USR];
static int        total_usr = 0, prox_id_usr = 1;
static Agendamento agendamentos[MAX_AG];
static int        total_ag = 0,  prox_id_ag  = 1;
static Usuario   *logado = NULL;

/* Estruturas de dados ativas */
static Pilha historico;   /* pilha: IDs dos ultimos agendamentos alterados */
static Fila  fila_espera; /* fila:  IDs de clientes aguardando atendimento  */

/* ─── Tabela de servicos ──────────────────────────────────── */
static const InfoSvc SVCS[N_SVC] = {
    {SVC_CAB,      "Corte de Cabelo",              40.f},
    {SVC_SBH,      "Corte de Sobrancelha",          5.f},
    {SVC_CAB_SBH,  "Corte de Cabelo + Sobrancelha", 50.f},
    {SVC_BARBA,    "Corte de Barba",               30.f},
    {SVC_CAB_BARBA,"Corte de Cabelo + Barba",      70.f}
};

/* ─── Utilitarios basicos ─────────────────────────────────── */
static int  eh_digito(char c)   { return c>='0' && c<='9'; }
static int  eh_espaco(char c)   { return c==' '||c=='\n'||c=='\t'||c=='\r'; }
static char minusculo(char c)   { return (c>='A'&&c<='Z') ? c+32 : c; }
static void limpar_buf(void)    { int c; while((c=getchar())!='\n'&&c!=EOF); }

void limpar_tela(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pausar(void) {
    printf("\n  Pressione ENTER para continuar...");
    fflush(stdout);
    limpar_buf();
}

static void linha(char c, int n) {
    for (int i=0; i<n; i++) putchar(c);
    putchar('\n');
}

void cabecalho(const char *t) {
    limpar_tela();
    linha('=', 60);
    printf("  BARBEARIA  --  %s\n", t);
    linha('=', 60);
    putchar('\n');
}

/* ─── Leitura de entrada ──────────────────────────────────── */
int ler_str(const char *p, char *dest, int tam) {
    printf("  %s", p);
    fflush(stdout);
    dest[0] = '\0';
    if (!fgets(dest, tam, stdin)) return 0;
    int n = strlen(dest);
    if (n>0 && dest[n-1]=='\n') dest[n-1]='\0';
    else limpar_buf();
    return 1;
}

int ler_int(const char *p) {
    char buf[32];
    if (!ler_str(p, buf, sizeof(buf))) return -1;
    return buf[0] ? atoi(buf) : 0;
}

static int confirmar(const char *msg) {
    char r[10];
    printf("  %s (s/N): ", msg);
    fflush(stdout);
    if (!fgets(r, sizeof(r), stdin)) return 0;
    if (!strchr(r,'\n')) limpar_buf();
    return minusculo(r[0])=='s';
}

/* ─── Validacoes ─────────────────────────────────────────── */
int validar_cpf(const char *in, char *out) {
    char d[12]; int n=0;
    for (int i=0; in[i]; i++) {
        if (eh_digito(in[i])) { if(n<11) d[n++]=in[i]; else return 0; }
    }
    if (n!=11) return 0;
    /* verifica todos iguais */
    int igual=1;
    for (int i=1;i<11;i++) if(d[i]!=d[0]){igual=0;break;}
    if (igual) return 0;
    /* digito verificador 1 */
    int s=0; for(int i=0;i<9;i++) s+=(d[i]-'0')*(10-i);
    int r=(s*10)%11; if(r==10)r=0; if(r!=(d[9]-'0')) return 0;
    /* digito verificador 2 */
    s=0; for(int i=0;i<10;i++) s+=(d[i]-'0')*(11-i);
    r=(s*10)%11; if(r==10)r=0; if(r!=(d[10]-'0')) return 0;
    if(out) snprintf(out,TAM_CPF,"%c%c%c.%c%c%c.%c%c%c-%c%c",
        d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10]);
    return 1;
}

int validar_data(const char *d) {
    if (!d || strlen(d)!=10 || d[2]!='/' || d[5]!='/') return 0;
    for (int i=0;i<10;i++) if(i!=2&&i!=5&&!eh_digito(d[i])) return 0;
    int dia=atoi(d), mes=atoi(d+3), ano=atoi(d+6);
    if (mes<1||mes>12||ano<2024||dia<1) return 0;
    int maxd = (mes==2) ? ((ano%4==0&&ano%100!=0)||(ano%400==0)?29:28)
             : (mes==4||mes==6||mes==9||mes==11) ? 30 : 31;
    return dia<=maxd;
}

int validar_tel(const char *t) {
    if (!t||!t[0]) return 0;
    int n=0;
    for(int i=0;t[i];i++) {
        if(eh_digito(t[i])) n++;
        else if(t[i]!='('&&t[i]!=')'&&t[i]!=' '&&t[i]!='-'&&t[i]!='+') return 0;
    }
    return n>=10&&n<=11;
}

int str_valida(const char *s, int min) {
    if(!s) return 0;
    int c=0; for(int i=0;s[i];i++) if(!eh_espaco(s[i])) c++;
    return c>=min;
}

/* ─── Data de hoje ────────────────────────────────────────── */
void data_hoje(char *buf) {
    time_t t=time(NULL); struct tm *tm=localtime(&t);
    snprintf(buf,TAM_DATA,"%02d/%02d/%04d",
        tm->tm_mday, tm->tm_mon+1, tm->tm_year+1900);
}

/* Compara datas no formato DD/MM/AAAA */
int cmp_data(const char *a, const char *b) {
    int r=strcmp(a+6,b+6); if(r) return r;
    r=strncmp(a+3,b+3,2);  if(r) return r;
    return strncmp(a,b,2);
}

/* ─── Helpers de lookup ───────────────────────────────────── */
const InfoSvc *get_svc(Servico s) {
    return (s>=1&&s<=N_SVC) ? &SVCS[s-1] : &SVCS[0];
}
const char *str_perfil(Perfil p) {
    return p==ADMIN?"Admin":p==BARBEIRO?"Barbeiro":"Cliente";
}
const char *str_status(StatusAg s) {
    return s==AG_PEND?"Pendente":s==AG_OK?"Concluido":"Cancelado";
}

/* ─── Persistencia ────────────────────────────────────────── */
void salvar_usr(void) {
    FILE *f=fopen(ARQ_USR,"wb");
    if(!f){puts("  [ERRO] Nao salvou usuarios.");return;}
    fwrite(&total_usr,sizeof(int),1,f);
    fwrite(&prox_id_usr,sizeof(int),1,f);
    fwrite(usuarios,sizeof(Usuario),total_usr,f);
    fclose(f);
}

void salvar_ag(void) {
    FILE *f=fopen(ARQ_AG,"wb");
    if(!f){puts("  [ERRO] Nao salvou agendamentos.");return;}
    fwrite(&total_ag,sizeof(int),1,f);
    fwrite(&prox_id_ag,sizeof(int),1,f);
    fwrite(agendamentos,sizeof(Agendamento),total_ag,f);
    fclose(f);
}

static void criar_admin(void) {
    Usuario a={0};
    a.id=prox_id_usr++;
    strcpy(a.nome,"Administrador");
    strcpy(a.cpf,"065.974.841-00");
    strcpy(a.senha,"adm123");
    strcpy(a.tel,"(00) 00000-0000");
    a.perfil=ADMIN; a.ativo=1;
    usuarios[total_usr++]=a;
    salvar_usr();
}

void carregar_usr(void) {
    FILE *f=fopen(ARQ_USR,"rb");
    if(!f){criar_admin();return;}
    fread(&total_usr,sizeof(int),1,f);
    fread(&prox_id_usr,sizeof(int),1,f);
    if(total_usr<0||total_usr>MAX_USR) { total_usr=0;prox_id_usr=1;criar_admin(); }
    else fread(usuarios,sizeof(Usuario),total_usr,f);
    fclose(f);
}

void carregar_ag(void) {
    FILE *f=fopen(ARQ_AG,"rb");
    if(!f) return;
    fread(&total_ag,sizeof(int),1,f);
    fread(&prox_id_ag,sizeof(int),1,f);
    if(total_ag<0||total_ag>MAX_AG) puts("  [AVISO] Agendamentos corrompidos.");
    else fread(agendamentos,sizeof(Agendamento),total_ag,f);
    fclose(f);
}

/* ─── Buscas ──────────────────────────────────────────────── */
Usuario    *buscar_cpf(const char *cpf) {
    for(int i=0;i<total_usr;i++)
        if(usuarios[i].ativo && !strcmp(usuarios[i].cpf,cpf)) return &usuarios[i];
    return NULL;
}
Usuario    *buscar_usr(int id) {
    for(int i=0;i<total_usr;i++)
        if(usuarios[i].ativo && usuarios[i].id==id) return &usuarios[i];
    return NULL;
}
Agendamento *buscar_ag(int id) {
    for(int i=0;i<total_ag;i++)
        if(agendamentos[i].ativo && agendamentos[i].id==id) return &agendamentos[i];
    return NULL;
}

/* ─── Impressao ───────────────────────────────────────────── */
void print_usr(const Usuario *u) {
    if(!u) return;
    printf("  +---------------------------------------------------+\n");
    printf("  | ID     : %-38d  |\n", u->id);
    printf("  | Nome   : %-38s  |\n", u->nome);
    printf("  | CPF    : %-38s  |\n", u->cpf);
    printf("  | Tel.   : %-38s  |\n", u->tel);
    printf("  | Perfil : %-38s  |\n", str_perfil(u->perfil));
    printf("  +---------------------------------------------------+\n");
}

void print_ag(const Agendamento *ag) {
    if(!ag) return;
    const char *hor = (ag->slot>=0&&ag->slot<TOTAL_SLOTS)?SLOTS[ag->slot]:"??:??";
    Usuario *cli = buscar_usr(ag->id_cli);
    Usuario *bar = ag->id_barb ? buscar_usr(ag->id_barb) : NULL;
    const InfoSvc *s = get_svc(ag->svc);
    printf("  +---------------------------------------------------+\n");
    printf("  | ID Ag.    : %-38d  |\n", ag->id);
    printf("  | Data      : %-38s  |\n", ag->data);
    printf("  | Horario   : %-38s  |\n", hor);
    printf("  | Servico   : %-38s  |\n", s->nome);
    printf("  | Valor     : R$ %-35.2f  |\n", (double)s->preco);
    printf("  | Cliente   : %-38s  |\n", cli?cli->nome:"?");
    printf("  | Barbeiro  : %-38s  |\n", bar?bar->nome:"Qualquer disponivel");
    printf("  | Status    : %-38s  |\n", str_status(ag->status));
    if(ag->obs[0]) printf("  | Obs.      : %-38s  |\n", ag->obs);
    printf("  +---------------------------------------------------+\n");
}

void listar_svcs(void) {
    printf("  +------+-------------------------------------+---------+\n");
    printf("  |  No  |  Servico                            |  Preco  |\n");
    printf("  +------+-------------------------------------+---------+\n");
    for(int i=0;i<N_SVC;i++)
        printf("  |  %-3d |  %-35s |  R$%3.0f  |\n",
               SVCS[i].id, SVCS[i].nome, (double)SVCS[i].preco);
    printf("  +------+-------------------------------------+---------+\n");
}

/* ─── Validacao de CPF com prompt ────────────────────────── */
static int ler_cpf(const char *prompt, char *saida, int permite_adm) {
    char raw[TAM_CPF];
    if(!ler_str(prompt,raw,TAM_CPF)||!raw[0]){puts("  [!] Entrada vazia.");return 0;}
    if(!validar_cpf(raw,saida)){puts("  [!] CPF invalido.");return 0;}
    if(!permite_adm&&!strcmp(saida,"065.974.841-00")){puts("  [!] CPF reservado.");return 0;}
    return 1;
}

/* ─── Login ───────────────────────────────────────────────── */
int fazer_login(void) {
    static int falhas=0;
    if(falhas>=MAX_FALHAS){puts("  [!] Bloqueado. Reinicie.");pausar();return 0;}
    cabecalho("LOGIN");
    char cpf_r[TAM_CPF], cpf_f[TAM_CPF], senha[TAM_SENHA];
    if(!ler_str("CPF   : ",cpf_r,TAM_CPF)||!validar_cpf(cpf_r,cpf_f)){
        puts("  [!] CPF invalido.");pausar();return 0;
    }
    if(!ler_str("Senha : ",senha,TAM_SENHA)||!senha[0]){
        puts("  [!] Senha vazia.");pausar();return 0;
    }
    Usuario *u=buscar_cpf(cpf_f);
    if(u&&!strcmp(u->senha,senha)){
        falhas=0; logado=u;
        printf("\n  Bem-vindo, %s! [%s]\n",u->nome,str_perfil(u->perfil));
        pausar();return 1;
    }
    printf("\n  [!] CPF ou senha incorretos. Restantes: %d\n",MAX_FALHAS-++falhas);
    pausar();return 0;
}

/* ─── Cadastro de usuario ─────────────────────────────────── */
static void cadastrar_interno(int perfil_fixo, int nivel_max) {
    cabecalho(perfil_fixo==BARBEIRO?"CADASTRAR BARBEIRO":"CADASTRO");
    if(total_usr>=MAX_USR){puts("  [!] Limite atingido.");pausar();return;}
    Usuario u={0};
    char cpf_f[TAM_CPF], conf[TAM_SENHA];
    if(!ler_str("Nome completo  : ",u.nome,TAM_NOME)||!str_valida(u.nome,3)){
        puts("  [!] Nome invalido.");pausar();return;
    }
    if(!ler_cpf("CPF            : ",cpf_f,0)){pausar();return;}
    if(buscar_cpf(cpf_f)){puts("  [!] CPF ja cadastrado.");pausar();return;}
    strcpy(u.cpf,cpf_f);
    if(!ler_str("Senha          : ",u.senha,TAM_SENHA)||strlen(u.senha)<4){
        puts("  [!] Senha minima 4 chars.");pausar();return;
    }
    if(!ler_str("Confirma senha : ",conf,TAM_SENHA)||strcmp(u.senha,conf)){
        puts("  [!] Senhas nao conferem.");pausar();return;
    }
    if(!ler_str("Telefone       : ",u.tel,TAM_TEL)||!validar_tel(u.tel)){
        puts("  [!] Telefone invalido.");pausar();return;
    }
    if(perfil_fixo) {
        u.perfil=(Perfil)perfil_fixo;
    } else {
        printf("\n  Perfil: 1-Cliente");
        if(nivel_max>=2) printf("  2-Barbeiro");
        if(nivel_max>=3) printf("  3-Admin");
        putchar('\n');
        int op=ler_int("Opcao [1]: ");
        u.perfil=(op>=1&&op<=nivel_max)?(Perfil)op:CLIENTE;
    }
    u.id=prox_id_usr++; u.ativo=1;
    usuarios[total_usr++]=u;
    salvar_usr();

    /* Adiciona cliente na fila de espera (demonstracao de FILA) */
    if(u.perfil==CLIENTE) {
        if(fila_enqueue(&fila_espera, u.id))
            printf("\n  Cliente adicionado a fila de espera. (FILA - enqueue)\n");
    }

    printf("\n  Cadastro realizado! (ID: %d)\n",u.id);
    pausar();
}

void cadastrar_usr(void)   { cadastrar_interno(0, logado&&logado->perfil==ADMIN?3:1); }
void cadastrar_barb(void)  { cadastrar_interno(BARBEIRO,0); }

/* ─── Listagem / busca de usuarios ──────────────────────────*/
void listar_usr(void) {
    cabecalho("LISTA DE USUARIOS");
    int found=0;
    for(int i=0;i<total_usr;i++)
        if(usuarios[i].ativo){print_usr(&usuarios[i]);putchar('\n');found=1;}
    if(!found) puts("  Nenhum usuario ativo.");
    pausar();
}

void buscar_usuario(void) {
    cabecalho("BUSCAR USUARIO");
    printf("  1 - Por CPF\n  2 - Por ID\n");
    int op=ler_int("Opcao: ");
    Usuario *u=NULL;
    if(op==1){
        char cpf[TAM_CPF];
        if(!ler_cpf("CPF: ",cpf,1)){pausar();return;}
        u=buscar_cpf(cpf);
    } else if(op==2){
        int id=ler_int("ID: ");
        if(id>0) u=buscar_usr(id);
    } else {puts("  [!] Opcao invalida.");}
    if(u){putchar('\n');print_usr(u);}
    else puts("  [!] Nao encontrado.");
    pausar();
}

/* ─── Excluir usuario ─────────────────────────────────────── */
void excluir_usr(void) {
    cabecalho("EXCLUIR USUARIO");
    char cpf[TAM_CPF];
    if(!ler_cpf("CPF: ",cpf,1)){pausar();return;}
    Usuario *u=buscar_cpf(cpf);
    if(!u){puts("  [!] Nao encontrado.");pausar();return;}
    if(!logado||logado->perfil!=ADMIN){
        char sv[TAM_SENHA];
        if(!ler_str("Confirme sua senha: ",sv,TAM_SENHA)||strcmp(u->senha,sv)){
            puts("  [!] Senha incorreta.");pausar();return;
        }
    }
    if(u->perfil==ADMIN){
        int adm=0;
        for(int i=0;i<total_usr;i++) if(usuarios[i].ativo&&usuarios[i].perfil==ADMIN) adm++;
        if(adm<=1){puts("  [!] Unico admin.");pausar();return;}
    }
    putchar('\n'); print_usr(u);
    if(!confirmar("\n  Confirmar exclusao?")){puts("  Cancelado.");pausar();return;}
    /* cancela agendamentos pendentes do usuario */
    for(int i=0;i<total_ag;i++)
        if(agendamentos[i].ativo&&agendamentos[i].id_cli==u->id&&agendamentos[i].status==AG_PEND)
            agendamentos[i].status=AG_CANCEL;
    salvar_ag();
    u->ativo=0;
    if(logado&&logado->id==u->id) logado=NULL;
    salvar_usr();
    puts("\n  Usuario excluido.");
    pausar();
}

/* ─── Alterar senha ───────────────────────────────────────── */
void alterar_senha(void) {
    cabecalho("ALTERAR SENHA");
    if(!logado){puts("  [!] Faca login primeiro.");pausar();return;}
    char atual[TAM_SENHA], nova[TAM_SENHA], conf[TAM_SENHA];
    if(!ler_str("Senha atual   : ",atual,TAM_SENHA)||strcmp(logado->senha,atual)){
        puts("  [!] Senha incorreta.");pausar();return;
    }
    if(!ler_str("Nova senha    : ",nova,TAM_SENHA)||strlen(nova)<4){
        puts("  [!] Minimo 4 chars.");pausar();return;
    }
    if(!strcmp(nova,logado->senha)){puts("  [!] Igual a atual.");pausar();return;}
    if(!ler_str("Confirme nova : ",conf,TAM_SENHA)||strcmp(nova,conf)){
        puts("  [!] Senhas divergem.");pausar();return;
    }
    strcpy(logado->senha,nova);
    salvar_usr();
    puts("\n  Senha alterada com sucesso.");
    pausar();
}

/* ─── Agendamentos ────────────────────────────────────────── */
int slot_livre(const char *data, int slot, int id_barb, int ignorar) {
    for(int i=0;i<total_ag;i++){
        Agendamento *a=&agendamentos[i];
        if(a->ativo&&a->id!=ignorar&&a->status!=AG_CANCEL&&
           !strcmp(a->data,data)&&a->slot==slot)
            if(id_barb<=0||a->id_barb<=0||a->id_barb==id_barb) return 0;
    }
    return 1;
}

void listar_ags(int id_cli, int id_barb, const char *data, int so_pend) {
    int found=0;
    for(int i=0;i<total_ag;i++){
        Agendamento *a=&agendamentos[i];
        if(!a->ativo) continue;
        if(id_cli>0&&a->id_cli!=id_cli) continue;
        if(id_barb>0&&a->id_barb!=id_barb) continue;
        if(data&&data[0]&&strcmp(a->data,data)) continue;
        if(so_pend&&a->status!=AG_PEND) continue;
        print_ag(a); putchar('\n'); found=1;
    }
    if(!found) puts("  Nenhum agendamento encontrado.");
}

void criar_ag(void) {
    cabecalho("NOVO AGENDAMENTO");
    if(!logado){puts("  [!] Nenhum usuario logado.");pausar();return;}
    if(total_ag>=MAX_AG){puts("  [!] Limite atingido.");pausar();return;}
    Agendamento a={0};
    /* define cliente */
    if(logado->perfil==ADMIN){
        printf("  1-Para voce  2-Para um cliente\n");
        int op=ler_int("Opcao: ");
        if(op==2){
            char cpf[TAM_CPF];
            if(!ler_cpf("CPF do cliente: ",cpf,0)){pausar();return;}
            Usuario *cli=buscar_cpf(cpf);
            if(!cli){puts("  [!] Cliente nao encontrado.");pausar();return;}
            a.id_cli=cli->id;
        } else a.id_cli=logado->id;
    } else a.id_cli=logado->id;
    /* data */
    char hoje[TAM_DATA]; data_hoje(hoje);
    printf("\n  Hoje: %s\n",hoje);
    int tent=0;
    while(1){
        if(!ler_str("  Data (DD/MM/AAAA): ",a.data,TAM_DATA)||!a.data[0]){
            puts("  [!] Entrada invalida.");pausar();return;
        }
        if(!validar_data(a.data)) puts("  [!] Data invalida.");
        else if(cmp_data(a.data,hoje)<0) puts("  [!] Data no passado.");
        else break;
        if(++tent>=3){puts("  [!] Muitas tentativas.");pausar();return;}
    }
    /* servico */
    putchar('\n'); listar_svcs();
    int sv=ler_int("Servico [1-5]: ");
    if(sv<1||sv>N_SVC){puts("  [!] Servico invalido.");pausar();return;}
    a.svc=(Servico)sv;
    /* horario */
    printf("\n  Horarios em %s:\n\n",a.data);
    int disp=0;
    for(int s=0;s<TOTAL_SLOTS;s++){
        int livre=slot_livre(a.data,s,0,-1);
        printf("    %2d - %s  [%s]\n",s+1,SLOTS[s],livre?"Disponivel":"Ocupado   ");
        if(livre) disp++;
    }
    if(!disp){puts("\n  [!] Sem horarios disponiveis.");pausar();return;}
    int hs=ler_int("\n  Numero do horario [1-20]: ");
    if(hs<1||hs>TOTAL_SLOTS){puts("  [!] Fora da faixa.");pausar();return;}
    hs--;
    if(!slot_livre(a.data,hs,0,-1)){puts("  [!] Horario ja ocupado.");pausar();return;}
    a.slot=hs;
    /* barbeiro */
    if(confirmar("\n  Barbeiro especifico?")){
        int qtd=0;
        printf("\n  Barbeiros:\n");
        for(int i=0;i<total_usr;i++)
            if(usuarios[i].ativo&&usuarios[i].perfil==BARBEIRO){
                printf("    ID %-3d - %s\n",usuarios[i].id,usuarios[i].nome); qtd++;
            }
        if(qtd){
            int ib=ler_int("  ID (0=qualquer): ");
            if(ib>0){
                Usuario *b=buscar_usr(ib);
                if(b&&b->perfil==BARBEIRO) a.id_barb=ib;
                else puts("  Barbeiro nao encontrado. Agendando para qualquer um.");
            }
        } else puts("  Sem barbeiros cadastrados.");
    }
    ler_str("Observacao (ENTER para pular): ",a.obs,TAM_OBS);
    a.id=prox_id_ag++; a.status=AG_PEND; a.ativo=1;
    agendamentos[total_ag++]=a;
    salvar_ag();

    /* Registra na pilha de historico (demonstracao de PILHA) */
    if(pilha_push(&historico, a.id))
        printf("\n  Agendamento registrado no historico. (PILHA - push, ID: %d)\n",a.id);

    printf("\n  Agendamento realizado!\n\n");
    print_ag(&agendamentos[total_ag-1]);
    pausar();
}

void meus_ags(void) {
    if(!logado) return;
    cabecalho("MEUS AGENDAMENTOS");
    printf("  1-Todos  2-Pendentes  3-Por data\n");
    int op=ler_int("Opcao: ");
    char fd[TAM_DATA]={0}; int pend=0;
    if(op==2) pend=1;
    else if(op==3){
        if(!ler_str("Data (DD/MM/AAAA): ",fd,TAM_DATA)||!validar_data(fd)){
            puts("  [!] Data invalida.");pausar();return;
        }
    }
    listar_ags(logado->id,0,fd[0]?fd:NULL,pend);
    pausar();
}

void agenda_barb(void) {
    if(!logado) return;
    cabecalho("MINHA AGENDA");
    printf("  1-Hoje  2-Por data  3-Todos pendentes\n");
    int op=ler_int("Opcao: ");
    char fd[TAM_DATA]={0}; int pend=0;
    if(op==1) data_hoje(fd);
    else if(op==2){
        if(!ler_str("Data (DD/MM/AAAA): ",fd,TAM_DATA)||!validar_data(fd)){
            puts("  [!] Data invalida.");pausar();return;
        }
    } else pend=1;
    listar_ags(0,logado->id,fd[0]?fd:NULL,pend);
    pausar();
}

void listar_todos_ags(void) {
    cabecalho("TODOS OS AGENDAMENTOS");
    printf("  1-Todos  2-Pendentes  3-Por data  4-Por cliente\n");
    int op=ler_int("Opcao: ");
    char fd[TAM_DATA]={0}; int pend=0,id_cli=0;
    if(op==2) pend=1;
    else if(op==3){
        if(!ler_str("Data (DD/MM/AAAA): ",fd,TAM_DATA)||!validar_data(fd)){
            puts("  [!] Data invalida.");pausar();return;
        }
    } else if(op==4){
        char cpf[TAM_CPF];
        if(!ler_cpf("CPF do cliente: ",cpf,1)){pausar();return;}
        Usuario *c=buscar_cpf(cpf);
        if(!c){puts("  [!] Cliente nao localizado.");pausar();return;}
        id_cli=c->id;
    } else if(op!=1){puts("  [!] Opcao invalida.");pausar();return;}
    listar_ags(id_cli,0,fd[0]?fd:NULL,pend);
    pausar();
}

void cancelar_ag(void) {
    if(!logado) return;
    cabecalho("CANCELAR AGENDAMENTO");
    if(logado->perfil==CLIENTE) listar_ags(logado->id,0,NULL,1);
    else listar_ags(0,0,NULL,1);
    int id=ler_int("\n  ID do agendamento (0=voltar): ");
    if(!id) return;
    if(id<0){puts("  [!] ID invalido.");pausar();return;}
    Agendamento *ag=buscar_ag(id);
    if(!ag){puts("  [!] Nao localizado.");pausar();return;}
    if(ag->status!=AG_PEND){puts("  [!] Nao esta pendente.");pausar();return;}
    if(logado->perfil==CLIENTE&&ag->id_cli!=logado->id){
        puts("  [!] Permissao negada.");pausar();return;
    }
    putchar('\n'); print_ag(ag);
    if(!confirmar("\n  Confirmar cancelamento?")){puts("  Cancelado.");pausar();return;}
    ag->status=AG_CANCEL;
    salvar_ag();

    /* Registra cancelamento na pilha de historico (PILHA - push) */
    pilha_push(&historico, ag->id);
    printf("\n  Cancelamento registrado no historico. (PILHA - push, ID: %d)\n",ag->id);

    puts("\n  Agendamento cancelado.");
    pausar();
}

void concluir_ag(void) {
    if(!logado) return;
    cabecalho("CONCLUIR AGENDAMENTO");
    int id_barb = logado->perfil==BARBEIRO ? logado->id : 0;
    listar_ags(0,id_barb,NULL,1);
    int id=ler_int("\n  ID do agendamento (0=voltar): ");
    if(!id) return;
    Agendamento *ag=buscar_ag(id);
    if(!ag||ag->status!=AG_PEND){puts("  [!] Invalido ou nao pendente.");pausar();return;}
    if(logado->perfil==BARBEIRO&&ag->id_barb&&ag->id_barb!=logado->id){
        puts("  [!] Nao pertence a sua agenda.");pausar();return;
    }
    ag->status=AG_OK;
    salvar_ag();

    /* Demonstra dequeue: cliente atendido sai da fila de espera */
    int prox=fila_dequeue(&fila_espera);
    if(prox>0)
        printf("\n  Proximo cliente chamado da fila: ID %d (FILA - dequeue)\n",prox);

    puts("\n  Servico concluido.");
    pausar();
}

void excluir_ag_perm(void) {
    cabecalho("EXCLUIR AGENDAMENTO");
    listar_ags(0,0,NULL,0);
    int id=ler_int("\n  ID (0=voltar): ");
    if(!id) return;
    Agendamento *ag=buscar_ag(id);
    if(!ag){puts("  [!] Nao encontrado.");pausar();return;}
    putchar('\n'); print_ag(ag);
    if(!confirmar("\n  Excluir DEFINITIVAMENTE?")){puts("  Cancelado.");pausar();return;}
    ag->ativo=0;
    salvar_ag();
    puts("\n  Agendamento removido.");
    pausar();
}

/* ─── Relatorio ───────────────────────────────────────────── */
void relatorio(void) {
    cabecalho("RELATORIO GERAL");
    int pend=0,ok=0,canc=0,hoje_pend=0;
    int tot_usr=0,cli=0,barb=0,adm=0;
    double receita=0.0;
    char str_hoje[TAM_DATA]; data_hoje(str_hoje);
    for(int i=0;i<total_ag;i++){
        if(!agendamentos[i].ativo) continue;
        StatusAg s=agendamentos[i].status;
        if(s==AG_PEND){pend++;if(!strcmp(agendamentos[i].data,str_hoje))hoje_pend++;}
        else if(s==AG_OK){ok++;receita+=get_svc(agendamentos[i].svc)->preco;}
        else canc++;
    }
    for(int i=0;i<total_usr;i++){
        if(!usuarios[i].ativo) continue;
        tot_usr++;
        if(usuarios[i].perfil==CLIENTE)   cli++;
        else if(usuarios[i].perfil==BARBEIRO) barb++;
        else adm++;
    }
    printf("  === USUARIOS ========\n");
    printf("  Total    : %d  |  Clientes : %d\n",tot_usr,cli);
    printf("  Barbeiros: %d  |  Admins   : %d\n",barb,adm);
    printf("\n  === AGENDAMENTOS ====\n");
    printf("  Hoje(pend): %d  |  Pendentes  : %d\n",hoje_pend,pend);
    printf("  Concluidos: %d  |  Cancelados : %d\n",ok,canc);
    printf("\n  === FINANCEIRO ======\n");
    printf("  Receita total: R$ %.2f\n",receita);

    /* Demonstra a pilha: mostra ultimo agendamento alterado */
    printf("\n  === HISTORICO (PILHA) ==\n");
    int ult=pilha_pop(&historico);
    if(ult>0) printf("  Ultimo ag. alterado (pop): ID %d\n",ult);
    else       printf("  Historico vazio.\n");

    /* Demonstra a fila: mostra proximo da fila de espera */
    printf("\n  === FILA DE ESPERA (FILA) ==\n");
    printf("  Clientes aguardando: %d\n",fila_espera.tamanho);

    printf("  ====================\n");
    pausar();
}

/* ─── Menus ───────────────────────────────────────────────── */
void menu_cli(void) {
    int op;
    do {
        if(!logado) break;
        cabecalho("PAINEL DO CLIENTE");
        printf("  Ola, %s!\n\n",logado->nome);
        printf("  1-Novo agendamento    2-Meus agendamentos\n");
        printf("  3-Cancelar servico    4-Ver meu perfil\n");
        printf("  5-Alterar senha       6-Excluir conta\n");
        printf("  0-Sair\n\n");
        op=ler_int("Opcao: ");
        switch(op){
            case 1: criar_ag(); break;
            case 2: meus_ags(); break;
            case 3: cancelar_ag(); break;
            case 4: cabecalho("MEU PERFIL"); if(logado)print_usr(logado); pausar(); break;
            case 5: alterar_senha(); break;
            case 6: excluir_usr(); if(!logado)op=0; break;
            case 0: break;
            default: puts("  [!] Opcao invalida."); pausar();
        }
    } while(op!=0&&logado);
    logado=NULL;
}

void menu_barb(void) {
    int op;
    do {
        if(!logado) break;
        cabecalho("PAINEL DO BARBEIRO");
        printf("  Ola, %s!\n\n",logado->nome);
        printf("  1-Minha agenda    2-Concluir servico\n");
        printf("  3-Cancelar ag.    4-Meu perfil\n");
        printf("  5-Alterar senha   0-Sair\n\n");
        op=ler_int("Opcao: ");
        switch(op){
            case 1: agenda_barb(); break;
            case 2: concluir_ag(); break;
            case 3: cancelar_ag(); break;
            case 4: cabecalho("MEU PERFIL"); if(logado)print_usr(logado); pausar(); break;
            case 5: alterar_senha(); break;
            case 0: break;
            default: puts("  [!] Opcao invalida."); pausar();
        }
    } while(op!=0&&logado);
    logado=NULL;
}

void menu_adm(void) {
    int op;
    do {
        if(!logado) break;
        cabecalho("PAINEL DO ADMIN");
        printf("  Admin: %s\n\n",logado->nome);
        printf("  1-Novo cliente        2-Novo barbeiro\n");
        printf("  3-Listar usuarios     4-Buscar usuario\n");
        printf("  5-Excluir usuario     6-Novo agendamento\n");
        printf("  7-Listar agendamentos 8-Cancelar ag.\n");
        printf("  9-Concluir ag.        10-Excluir ag.\n");
        printf("  11-Relatorio          12-Alterar senha\n");
        printf("  0-Sair\n\n");
        op=ler_int("Opcao: ");
        switch(op){
            case 1:  cadastrar_usr();  break;
            case 2:  cadastrar_barb(); break;
            case 3:  listar_usr();     break;
            case 4:  buscar_usuario(); break;
            case 5:  excluir_usr();    break;
            case 6:  criar_ag();       break;
            case 7:  listar_todos_ags();break;
            case 8:  cancelar_ag();    break;
            case 9:  concluir_ag();    break;
            case 10: excluir_ag_perm();break;
            case 11: relatorio();      break;
            case 12: alterar_senha();  break;
            case 0:  break;
            default: puts("  [!] Opcao invalida."); pausar();
        }
    } while(op!=0&&logado);
    logado=NULL;
}

void menu_principal(void) {
    int op;
    do {
        cabecalho("PAGINA INICIAL");
        printf("  1-Login    2-Cadastrar conta    0-Sair\n\n");
        op=ler_int("Opcao: ");
        switch(op){
            case 1:
                if(fazer_login()&&logado){
                    if(logado->perfil==ADMIN)    menu_adm();
                    else if(logado->perfil==BARBEIRO) menu_barb();
                    else menu_cli();
                }
                break;
            case 2: cadastrar_usr(); break;
            case 0: break;
            default: puts("  [!] Opcao invalida."); pausar();
        }
    } while(op!=0);
}

/* ─── Main ────────────────────────────────────────────────── */
int main(void) {
    /* Inicializa estruturas de dados */
    pilha_init(&historico);
    fila_init(&fila_espera);

    carregar_usr();
    carregar_ag();

    cabecalho("BEM-VINDO");
    printf("  Sistema de Gestao de Barbearia  v3.0\n\n");
    pausar();

    menu_principal();

    limpar_tela();
    printf("  Obrigado por utilizar o Sistema de Barbearia!\n\n");
    return 0;
}