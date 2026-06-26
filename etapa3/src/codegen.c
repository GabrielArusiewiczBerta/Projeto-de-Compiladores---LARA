/*
 * codegen.c — Gerador de Código TAC (ESQUELETO — Etapa 3)
 *
 * INF01083 — Linguagens de Programação II / Compiladores — 2026/1
 * UFRGS / INF — Prof. Nicolas Maillard
 *
 * Baseado na SOLUÇÃO COMPLETA da Etapa 2. A Etapa 3 adiciona suporte
 * a expressões booleanas com curto-circuito e controle de fluxo.
 *
 * =====================================================================
 * TAREFAS DOS ALUNOS:
 *
 * TODO-E3-A: Em codegen_bool_expr(), implemente os casos de '&&' e '||'
 *            usando back-patching. Consulte backpatch.h para a interface
 *            das listas de saltos pendentes.
 *
 * TODO-E3-B: Em codegen_stmt(), implemente o caso AST_IF com back-patching.
 *            Use codegen_bool_expr() para gerar código da condição.
 *
 * TODO-E3-C: Em codegen_stmt(), implemente o caso AST_WHILE.
 *            Gere um rótulo de início e use goto para fechar o laço.
 *
 * TODO-E3-D: Em codegen_stmt(), implemente o caso AST_FOR.
 *            children[0]=init, children[1]=cond, children[2]=step, children[3]=corpo.
 * =====================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"

/* -----------------------------------------------------------------------
 * Helpers internos (herdados da Etapa 2)
 * ----------------------------------------------------------------------- */
static int type_size(sym_datatype_t dt)
{
    switch (dt) {
        case SYM_TYPE_INT:   return 4;
        case SYM_TYPE_FLOAT: return 8;
        case SYM_TYPE_CHAR:  return 1;
        case SYM_TYPE_BOOL:  return 1;
        default:             return 4;
    }
}

static tac_op_t op_to_tac(const char *op)
{
    if (!op) return TAC_NOP;
    if (strcmp(op, "+")  == 0) return TAC_ADD;
    if (strcmp(op, "-")  == 0) return TAC_SUB;
    if (strcmp(op, "*")  == 0) return TAC_MUL;
    if (strcmp(op, "/")  == 0) return TAC_DIV;
    if (strcmp(op, "%")  == 0) return TAC_MOD;
    if (strcmp(op, "<")  == 0) return TAC_LT;
    if (strcmp(op, ">")  == 0) return TAC_GT;
    if (strcmp(op, "<=") == 0) return TAC_LE;
    if (strcmp(op, ">=") == 0) return TAC_GE;
    if (strcmp(op, "==") == 0) return TAC_EQ;
    if (strcmp(op, "!=") == 0) return TAC_NE;
    if (strcmp(op, "&&") == 0) return TAC_AND;
    if (strcmp(op, "||") == 0) return TAC_OR;
    return TAC_NOP;
}

/* -----------------------------------------------------------------------
 * codegen_new / codegen_free / codegen_emit
 * ----------------------------------------------------------------------- */
codegen_ctx_t *codegen_new(symtab_t *symtab)
{
    codegen_ctx_t *ctx = calloc(1, sizeof(codegen_ctx_t));
    if (!ctx) return NULL;
    ctx->symtab = symtab;
    return ctx;
}

void codegen_free(codegen_ctx_t *ctx)
{
    if (!ctx) return;
    free(ctx->current_func);
    free(ctx);
}

void codegen_emit(codegen_ctx_t *ctx, tac_op_t op,
                  const char *result, const char *arg1, const char *arg2)
{
    tac_instr_t *i = tac_new(op, result, arg1, arg2);
    ctx->code = tac_append(ctx->code, i);
}

/* -----------------------------------------------------------------------
 * codegen_program (solução completa da Etapa 2)
 * ----------------------------------------------------------------------- */
void codegen_program(codegen_ctx_t *ctx, ast_node_t *program)
{
    if (!program) return;
    ast_node_t *decl = program->children[0];
    int global_offset = 0;

    while (decl) {
        if (decl->type == AST_VAR_DECL) {
            const char *vname = decl->value;
            sym_entry_t *e = symtab_lookup(ctx->symtab, vname);
            if (e) {
                e->scope  = SYM_SCOPE_GLOBAL;
                e->offset = global_offset;
                int sz = type_size(e->datatype);
                global_offset += sz;
                char sz_str[16];
                snprintf(sz_str, sizeof(sz_str), "%d", sz);
                codegen_emit(ctx, TAC_DECL_GLOBAL, vname, sz_str, NULL);
            }
        } else if (decl->type == AST_ARRAY_DECL) {
            const char *vname = decl->value;
            sym_entry_t *e = symtab_lookup(ctx->symtab, vname);
            if (e) {
                e->scope  = SYM_SCOPE_GLOBAL;
                e->offset = global_offset;
                int nelems = e->array_size > 0 ? e->array_size : 1;
                int sz = type_size(e->datatype) * nelems;
                global_offset += sz;
                char sz_str[16];
                snprintf(sz_str, sizeof(sz_str), "%d", sz);
                codegen_emit(ctx, TAC_DECL_GLOBAL, vname, sz_str, NULL);
            }
        } else if (decl->type == AST_FUN_DECL) {
            codegen_fun(ctx, decl);
        }
        decl = decl->next;
    }
}

/* -----------------------------------------------------------------------
 * codegen_fun (solução completa da Etapa 2)
 * ----------------------------------------------------------------------- */
void codegen_fun(codegen_ctx_t *ctx, ast_node_t *fun_decl)
{
    if (!fun_decl || fun_decl->type != AST_FUN_DECL) return;
    const char *fname = fun_decl->value;
    free(ctx->current_func);
    ctx->current_func = strdup(fname);
    ctx->local_offset = 0;
    codegen_emit(ctx, TAC_BEGINFUNC, fname, NULL, NULL);

    ast_node_t *param = fun_decl->children[1];
    int param_offset = -4;
    while (param) {
        if (param->type == AST_PARAM && param->value) {
            sym_entry_t *e = symtab_lookup(ctx->symtab, param->value);
            if (e) {
                e->scope  = SYM_SCOPE_LOCAL;
                e->offset = param_offset;
                param_offset -= type_size(e->datatype);
            }
        }
        param = param->next;
    }

    ast_node_t *body = fun_decl->children[2];
    if (body && body->type == AST_BLOCK) {
        ast_node_t *stmt = body->children[0];
        while (stmt) { codegen_stmt(ctx, stmt); stmt = stmt->next; }
    }
    codegen_emit(ctx, TAC_ENDFUNC, fname, NULL, NULL);
}

/* -----------------------------------------------------------------------
 * codegen_bool_expr — traduz expressão booleana com curto-circuito
 * ----------------------------------------------------------------------- */
bool_result_t codegen_bool_expr(codegen_ctx_t *ctx, ast_node_t *expr)
{
    bool_result_t res = { NULL, NULL };
    if (!expr) return res;

    if (expr->type == AST_EXPR_BINARY &&
        (strcmp(expr->value, "&&") == 0 || strcmp(expr->value, "||") == 0))
    {
        if (strcmp(expr->value, "&&") == 0) {
            /*
             * TODO-E3-A (&&): implemente a geração de código para &&.
             *
             * Algoritmo:
             *   1. r1 = codegen_bool_expr(ctx, expr->children[0])
             *   2. Gere rótulo lmid; preencha r1.true_list com lmid
             *   3. Emita TAC_LABEL lmid
             *   4. r2 = codegen_bool_expr(ctx, expr->children[1])
             *   5. res.true_list  = r2.true_list
             *      res.false_list = patch_list_merge(r1.false_list, r2.false_list)
             *
             * Semântica: e1 && e2 é falso se e1 é falso (curto-circuito).
             *            Se e1 é verdadeiro, avalia e2.
             */
            fprintf(stderr, "[CODEGEN] TODO-E3-A: '&&' não implementado.\n");
            /* stub: gera código das subexpressões mas não conecta as listas */
            bool_result_t r1 = codegen_bool_expr(ctx, expr->children[0]);
            bool_result_t r2 = codegen_bool_expr(ctx, expr->children[1]);
            res.true_list  = r2.true_list;
            res.false_list = patch_list_merge(r1.false_list, r2.false_list);
            patch_list_free(r1.true_list);
        } else {
            /*
             * TODO-E3-A (||): implemente a geração de código para ||.
             *
             * Algoritmo:
             *   1. r1 = codegen_bool_expr(ctx, expr->children[0])
             *   2. Gere rótulo lmid; preencha r1.false_list com lmid
             *   3. Emita TAC_LABEL lmid
             *   4. r2 = codegen_bool_expr(ctx, expr->children[1])
             *   5. res.true_list  = patch_list_merge(r1.true_list, r2.true_list)
             *      res.false_list = r2.false_list
             *
             * Semântica: e1 || e2 é verdadeiro se e1 é verdadeiro (curto-circuito).
             */
            fprintf(stderr, "[CODEGEN] TODO-E3-A: '||' não implementado.\n");
            bool_result_t r1 = codegen_bool_expr(ctx, expr->children[0]);
            bool_result_t r2 = codegen_bool_expr(ctx, expr->children[1]);
            res.true_list  = patch_list_merge(r1.true_list, r2.true_list);
            res.false_list = r2.false_list;
            patch_list_free(r1.false_list);
        }
    } else {
        /* Expressão simples (relacional ou variável): emite JUMPT + JUMPF */
        char *val = codegen_expr(ctx, expr);
        tac_instr_t *jt = tac_new(TAC_JUMPT, "???", val, NULL);
        tac_instr_t *jf = tac_new(TAC_JUMPF, "???", val, NULL);
        ctx->code = tac_append(ctx->code, jt);
        ctx->code = tac_append(ctx->code, jf);
        res.true_list  = patch_list_make(jt);
        res.false_list = patch_list_make(jf);
        free(val);
    }
    return res;
}

/* -----------------------------------------------------------------------
 * codegen_stmt (solução E2 + stubs E3)
 * ----------------------------------------------------------------------- */
void codegen_stmt(codegen_ctx_t *ctx, ast_node_t *stmt)
{
    if (!stmt) return;
    switch (stmt->type) {

        case AST_VAR_DECL: {
            const char *vname = stmt->value;
            sym_entry_t *e = symtab_lookup(ctx->symtab, vname);
            if (e) {
                e->scope  = SYM_SCOPE_LOCAL;
                e->offset = ctx->local_offset;
                ctx->local_offset += type_size(e->datatype);
                char offset_str[16];
                snprintf(offset_str, sizeof(offset_str), "%d", e->offset);
                codegen_emit(ctx, TAC_DECL_LOCAL, vname, offset_str, NULL);
            }
            if (stmt->children[1]) {
                char *val = codegen_expr(ctx, stmt->children[1]);
                codegen_emit(ctx, TAC_COPY, vname, val, NULL);
                free(val);
            }
            break;
        }

        case AST_ASSIGN: {
            char *rval = codegen_expr(ctx, stmt->children[1]);
            ast_node_t *lv = stmt->children[0];
            if (lv->type == AST_SYMBOL) {
                if (strcmp(stmt->value, ":=") == 0) {
                    codegen_emit(ctx, TAC_COPY, lv->value, rval, NULL);
                } else if (strcmp(stmt->value, "+=") == 0) {
                    char *tmp = tac_new_temp();
                    codegen_emit(ctx, TAC_ADD, tmp, lv->value, rval);
                    codegen_emit(ctx, TAC_COPY, lv->value, tmp, NULL);
                    free(tmp);
                } else if (strcmp(stmt->value, "-=") == 0) {
                    char *tmp = tac_new_temp();
                    codegen_emit(ctx, TAC_SUB, tmp, lv->value, rval);
                    codegen_emit(ctx, TAC_COPY, lv->value, tmp, NULL);
                    free(tmp);
                }
            } else if (lv->type == AST_EXPR_INDEX) {
                char *idx = codegen_expr(ctx, lv->children[0]);
                codegen_emit(ctx, TAC_STORE, lv->value, idx, rval);
                free(idx);
            }
            free(rval);
            break;
        }

        case AST_PRINT: {
            char *val = codegen_expr(ctx, stmt->children[0]);
            codegen_emit(ctx, TAC_PRINT, NULL, val, NULL);
            free(val);
            break;
        }

        case AST_READ: {
            codegen_emit(ctx, TAC_READ, stmt->children[0]->value, NULL, NULL);
            break;
        }

        case AST_RETURN: {
            if (stmt->children[0]) {
                char *val = codegen_expr(ctx, stmt->children[0]);
                codegen_emit(ctx, TAC_RETURN, NULL, val, NULL);
                free(val);
            } else {
                codegen_emit(ctx, TAC_RETURN_VOID, NULL, NULL, NULL);
            }
            break;
        }

        case AST_CALL: {
            ast_node_t *arg = stmt->children[0];
            int nargs = 0;
            while (arg) { nargs++; arg = arg->next; }
            arg = stmt->children[0];
            while (arg) {
                char *val = codegen_expr(ctx, arg);
                codegen_emit(ctx, TAC_PARAM, NULL, val, NULL);
                free(val);
                arg = arg->next;
            }
            char nargs_str[16];
            snprintf(nargs_str, sizeof(nargs_str), "%d", nargs);
            codegen_emit(ctx, TAC_CALL, NULL, stmt->value, nargs_str);
            break;
        }

        case AST_BLOCK: {
            ast_node_t *s = stmt->children[0];
            while (s) { codegen_stmt(ctx, s); s = s->next; }
            break;
        }

        /* ---------------------------------------------------------------
         * TODO-E3-B: if/else
         *
         * Estrutura esperada:
         *   cond = codegen_bool_expr(ctx, stmt->children[0])
         *   Preenche cond.true_list com rótulo ltrue → emite ltrue
         *   Gera código do then-block (stmt->children[1])
         *   Se houver else (stmt->children[2]):
         *     Emite goto lend (salto para após o else)
         *     Preenche cond.false_list com rótulo lfalse → emite lfalse
         *     Gera código do else-block
         *     Emite lend
         *   Senão:
         *     Preenche cond.false_list com lfalse → emite lfalse
         * --------------------------------------------------------------- */
        case AST_IF:
            fprintf(stderr, "[CODEGEN] TODO-E3-B: if não implementado.\n");
            break;

        /* ---------------------------------------------------------------
         * TODO-E3-C: while
         *
         * Estrutura esperada:
         *   Emite rótulo lbegin
         *   cond = codegen_bool_expr(ctx, stmt->children[0])
         *   Preenche cond.true_list → emite lbody
         *   Gera código do corpo (stmt->children[1])
         *   Emite goto lbegin
         *   Preenche cond.false_list → emite lend
         * --------------------------------------------------------------- */
        case AST_WHILE:
            fprintf(stderr, "[CODEGEN] TODO-E3-C: while não implementado.\n");
            break;

        /* ---------------------------------------------------------------
         * TODO-E3-D: for
         *
         * children[0]=init (assign), children[1]=cond (expr),
         * children[2]=step (assign), children[3]=corpo (block)
         *
         * Estrutura esperada:
         *   Gera código do init
         *   Emite rótulo lbegin
         *   cond = codegen_bool_expr(ctx, stmt->children[1])
         *   Preenche cond.true_list → emite lbody
         *   Gera código do corpo
         *   Gera código do step
         *   Emite goto lbegin
         *   Preenche cond.false_list → emite lend
         * --------------------------------------------------------------- */
        case AST_FOR:
            fprintf(stderr, "[CODEGEN] TODO-E3-D: for não implementado.\n");
            break;

        default:
            fprintf(stderr, "[CODEGEN] Comando desconhecido tipo=%d\n", stmt->type);
            break;
    }
}

/* -----------------------------------------------------------------------
 * codegen_expr (solução completa da Etapa 2)
 * ----------------------------------------------------------------------- */
char *codegen_expr(codegen_ctx_t *ctx, ast_node_t *expr)
{
    if (!expr) return strdup("_undef");
    switch (expr->type) {
        case AST_SYMBOL:     return strdup(expr->value);
        case AST_LIT_INT:
        case AST_LIT_FLOAT:
        case AST_LIT_CHAR:   return strdup(expr->value);
        case AST_LIT_BOOL:   return strdup(strcmp(expr->value,"true")==0 ? "1":"0");
        case AST_LIT_STRING: return strdup(expr->value);
        case AST_EXPR_UNARY: {
            char *val = codegen_expr(ctx, expr->children[0]);
            char *tmp = tac_new_temp();
            tac_op_t op = strcmp(expr->value,"-")==0 ? TAC_NEG : TAC_NOT;
            codegen_emit(ctx, op, tmp, val, NULL);
            free(val);
            return tmp;
        }
        case AST_EXPR_BINARY: {
            char *left  = codegen_expr(ctx, expr->children[0]);
            char *right = codegen_expr(ctx, expr->children[1]);
            char *tmp   = tac_new_temp();
            tac_op_t op = op_to_tac(expr->value);
            codegen_emit(ctx, op, tmp, left, right);
            free(left); free(right);
            return tmp;
        }
        case AST_EXPR_INDEX: {
            char *idx = codegen_expr(ctx, expr->children[0]);
            char *tmp = tac_new_temp();
            codegen_emit(ctx, TAC_LOAD, tmp, expr->value, idx);
            free(idx);
            return tmp;
        }
        case AST_EXPR_CALL: {
            ast_node_t *arg = expr->children[0];
            int nargs = 0;
            while (arg) { nargs++; arg = arg->next; }
            arg = expr->children[0];
            while (arg) {
                char *val = codegen_expr(ctx, arg);
                codegen_emit(ctx, TAC_PARAM, NULL, val, NULL);
                free(val);
                arg = arg->next;
            }
            char nargs_str[16];
            snprintf(nargs_str, sizeof(nargs_str), "%d", nargs);
            char *tmp = tac_new_temp();
            codegen_emit(ctx, TAC_CALL, tmp, expr->value, nargs_str);
            return tmp;
        }
        default:
            fprintf(stderr, "[CODEGEN] Expressão desconhecida tipo=%d\n", expr->type);
            return strdup("_err");
    }
}