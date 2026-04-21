/*
 * symtab.c — Implementação da Tabela de Símbolos
 *
 * INF01083 — Linguagens de Programação II / Compiladores — 2026/1
 * UFRGS / INF — Prof. Nicolas Maillard
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

/* -----------------------------------------------------------------------
 * Função hash djb2 (Daniel J. Bernstein)
 * Distribui bem strings de identificadores típicos de programas.
 * ----------------------------------------------------------------------- */
static unsigned int hash_djb2(const char *str)
{
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++) != '\0')
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash % SYMTAB_SIZE;
}

/* -----------------------------------------------------------------------
 * symtab_new
 * ----------------------------------------------------------------------- */
symtab_t *symtab_new(void)
{
    symtab_t *tab = (symtab_t *) calloc(1, sizeof(symtab_t));
    if (tab == NULL) {
        fprintf(stderr, "[SYMTAB] ERRO: falha ao alocar tabela de símbolos.\n");
        return NULL;
    }
    /* calloc já zerifica buckets[] e count */
    return tab;
}

/* -----------------------------------------------------------------------
 * symtab_insert
 * ----------------------------------------------------------------------- */
sym_entry_t *symtab_insert(symtab_t *tab, const char *lexeme, int lineno)
{
    if (tab == NULL || lexeme == NULL) return NULL;

    unsigned int idx = hash_djb2(lexeme);

    /* Verifica se já existe */
    sym_entry_t *cur = tab->buckets[idx];
    while (cur != NULL) {
        if (strcmp(cur->lexeme, lexeme) == 0)
            return cur; /* já existe — retorna entrada existente */
        cur = cur->next;
    }

    /* Cria nova entrada */
    sym_entry_t *entry = (sym_entry_t *) calloc(1, sizeof(sym_entry_t));
    if (entry == NULL) {
        fprintf(stderr, "[SYMTAB] ERRO: falha ao alocar entrada.\n");
        return NULL;
    }
    entry->lexeme   = strdup(lexeme);
    entry->lineno   = lineno;
    entry->nature   = SYM_UNKNOWN;
    entry->datatype = SYM_TYPE_UNKNOWN;
    entry->offset   = 0;
    entry->next     = NULL;

    /* Insere no início do bucket (política LIFO — O(1)) */
    entry->next     = tab->buckets[idx];
    tab->buckets[idx] = entry;
    tab->count++;

    return entry;
}

/* -----------------------------------------------------------------------
 * symtab_lookup
 * ----------------------------------------------------------------------- */
sym_entry_t *symtab_lookup(symtab_t *tab, const char *lexeme)
{
    if (tab == NULL || lexeme == NULL) return NULL;

    unsigned int idx = hash_djb2(lexeme);
    sym_entry_t *cur = tab->buckets[idx];
    while (cur != NULL) {
        if (strcmp(cur->lexeme, lexeme) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL; /* não encontrado */
}

/* -----------------------------------------------------------------------
 * symtab_print
 * ----------------------------------------------------------------------- */
void symtab_print(const symtab_t *tab, FILE *out)
{
    if (tab == NULL) return;
    if (out  == NULL) out = stdout;

    fprintf(out, "=== Tabela de Símbolos (%d entradas) ===\n", tab->count);
    for (int i = 0; i < SYMTAB_SIZE; i++) {
        sym_entry_t *cur = tab->buckets[i];
        while (cur != NULL) {
            fprintf(out, "  [bucket %3d]  %-20s  linha=%d\n",
                    i, cur->lexeme, cur->lineno);
            cur = cur->next;
        }
    }
    fprintf(out, "==========================================\n");
}

/* -----------------------------------------------------------------------
 * symtab_free
 * ----------------------------------------------------------------------- */
void symtab_free(symtab_t *tab)
{
    if (tab == NULL) return;

    for (int i = 0; i < SYMTAB_SIZE; i++) {
        sym_entry_t *cur = tab->buckets[i];
        while (cur != NULL) {
            sym_entry_t *next = cur->next;
            free(cur->lexeme);
            free(cur);
            cur = next;
        }
    }
    free(tab);
}

/*-------------------------------
* Função auxiliar para tamanho de cada variavel
* Fiz somente dos basicos e do resto chutei 8
* Temos que fazer de todos os tipos do pdf?
*---------------------------------*/
static int get_type_size(sym_datatype_t type) {
    switch (type) {
        case SYM_TYPE_INT:   return 4;
        case SYM_TYPE_FLOAT: return 8;
        case SYM_TYPE_CHAR:  return 1;
        case SYM_TYPE_BOOL:  return 1;
        default:             return 0; // Para void ou desconhecidos
    }
}

/*
* Função da tarefa E2 - A
*/
void symtab_set_offsets(symtab_t *tab, sym_scope_t scope) {
    if (tab == NULL) return;

    int current_offset = 0;

    // Percorre todos os buckets da hash table
    for (int i = 0; i < SYMTAB_SIZE; i++) {
        sym_entry_t *entry = tab->buckets[i];
        
        // Percorre a lista ligada em cada bucket
        while (entry != NULL) {
            // Apenas processa símbolos que pertencem ao escopo atual
            if (entry->scope == scope) {
                // 1. Define o offset atual para este símbolo
                entry->offset = current_offset;
                
                // 2. Calcula quanto espaço ele ocupa
                int base_size = get_type_size(entry->datatype);
                int total_size = base_size;
                
                // 3. Se for array, multiplica pelo tamanho do array
                if (entry->nature == SYM_ARRAY) {
                    total_size = base_size * entry->array_size;
                }
                
                // 4. Incrementa o offset para a próxima variável
                current_offset += total_size;
            }
            entry = entry->next;
        }
    }
}
