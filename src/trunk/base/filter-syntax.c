/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse yfparse
#define yylex   yflex
#define yyerror yferror
#define yylval  yflval
#define yychar  yfchar
#define yydebug yfdebug
#define yynerrs yfnerrs


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     NOT = 258,
     AND = 259,
     OR = 260,
     OPENBR = 261,
     CLOSEBR = 262,
     COLON = 263,
     ALL = 264,
     EXPORTER = 265,
     DIR = 266,
     PORTDIR = 267,
     IFACE = 268,
     LEVEL3 = 269,
     LEVEL4 = 270,
     NUMBER = 271,
     NETMASK = 272,
     IPADDR = 273
   };
#endif
/* Tokens.  */
#define NOT 258
#define AND 259
#define OR 260
#define OPENBR 261
#define CLOSEBR 262
#define COLON 263
#define ALL 264
#define EXPORTER 265
#define DIR 266
#define PORTDIR 267
#define IFACE 268
#define LEVEL3 269
#define LEVEL4 270
#define NUMBER 271
#define NETMASK 272
#define IPADDR 273




/* Copy the first part of user declarations.  */
#line 83 "filter-syntax.y"


/* C Declarations */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h> 	/* va_start */

#include "como.h"

#define YYERROR_VERBOSE

/* Node types */
#define Tnone  0
#define Tand   1
#define Tor    2
#define Tnot   3
#define Tpred  4
#define Tip    5
#define Tport  6
#define Tproto 7
#define Tiface 8
#define Texporter 9

struct _listnode
{
    char *string;
    struct _listnode *next;
    struct _listnode *prev;
};
typedef struct _listnode listnode_t;

int yflex(void);
void yferror(char *fmt, ...);

/* Variables where the results will be stored after parsing the filter */
treenode_t **filter_tree;
char **filter_cmp;


/*
 * -- parse_ip
 *
 * Dots and numbers notation -> Binary representation of an IP address
 *
 */
static int
parse_ip(char *ipstring, uint32_t *ip)
{
    struct in_addr inp;
    
    if (!inet_aton(ipstring, &inp)) {
        yferror("Invalid IP address: %s", ipstring);
        return -1;
    }
    *ip = inp.s_addr;
    return 0;
}

uint32_t netmasks[33] = 
    { 
      0x0,
      0x80000000,
      0xC0000000,
      0xE0000000,
      0xF0000000,
      0xF8000000,
      0xFC000000,
      0xFE000000,
      0xFF000000,
      0xFF800000,
      0xFFC00000,
      0xFFE00000,
      0xFFF00000,
      0xFFF80000,
      0xFFFC0000,
      0xFFFE0000,
      0xFFFF0000,
      0xFFFF8000,
      0xFFFFC000,
      0xFFFFE000,
      0xFFFFF000,
      0xFFFFF800,
      0xFFFFFC00,
      0xFFFFFE00,
      0xFFFFFF00,
      0xFFFFFF80,
      0xFFFFFFC0,
      0xFFFFFFE0,
      0xFFFFFFF0,
      0xFFFFFFF8,
      0xFFFFFFFC,
      0xFFFFFFFE,
      0xFFFFFFFF 
    };

/*
 * -- parse_nm
 *
 * CIDR notation -> integer representing the network mask
 *
 */
static int
parse_nm(int i, uint32_t *nm)
{
    if (i >= 0 && i <= 32) *nm = htonl(netmasks[i]);
    else {
        yferror("Invalid CIDR netmask: %d", i);
        return -1;
    }
    return 0;
}

/*
 * -- append_string
 *
 */
char *
append_string(char *dest, char *src)
{
    dest = (char *)safe_realloc(dest, strlen(dest) + strlen(src) + 1);
    strcat(dest, src);
    return dest;
}

/* 
 * -- tree_make
 * 
 * Create a new expression tree node
 *
 */
treenode_t *
tree_make(uint8_t type, uint8_t pred_type, treenode_t *left,
          treenode_t *right, nodedata_t *data)
{
    treenode_t *t;
    
    t = (treenode_t *)safe_malloc(sizeof(treenode_t));
    t->type = type;
    if (t->type == Tpred) {
        t->pred_type = pred_type;
        t->data = (nodedata_t *)safe_malloc(sizeof(nodedata_t));
        switch(t->pred_type) {
        case Tip:
            asprintf(&(t->string), "%d ip %d/%d",
                     data->ipaddr.direction,
                     data->ipaddr.ip,
                     data->ipaddr.nm);
            t->data->ipaddr.direction = data->ipaddr.direction;
            t->data->ipaddr.ip = data->ipaddr.ip & data->ipaddr.nm;
            t->data->ipaddr.nm = data->ipaddr.nm;
            break;
        case Tport:
            asprintf(&(t->string), "%d port %d:%d",
                     data->ports.direction,
                     data->ports.lowport,
                     data->ports.highport);
            t->data->ports.direction = data->ports.direction;
            t->data->ports.lowport = data->ports.lowport;
            t->data->ports.highport = data->ports.highport;
            break;
        case Tproto:
            asprintf(&(t->string), "proto %d", data->proto);
            t->data->proto = data->proto;
            break;
        case Tiface:
            asprintf(&(t->string), "iface %d %d", 
		    data->iface.direction, data->iface.index);
            t->data->iface.direction = data->iface.direction;
            t->data->iface.index = data->iface.index;
            break;
	case Texporter:
            asprintf(&(t->string), "exporter %d",
                     data->exaddr.ip);
            t->data->exaddr.ip = data->exaddr.ip;
            break;
        }
    }
    t->right = right;
    t->left = left;
    
    return(t);
}

/*
 * -- list_add
 *
 * Add an element to a sorted list
 * XXX Insertion sort, quite inefficient
 *
 */
listnode_t *
list_add(listnode_t *list, char *s)
{
    listnode_t *laux;
    
    if (!list) {
        list = (listnode_t *)safe_malloc(sizeof(listnode_t));
        list->next = NULL;
        list->prev = NULL;
        list->string = safe_strdup(s);
    }
    else {
        if (strcmp(s, list->string) <= 0) {
            laux = (listnode_t *)safe_malloc(sizeof(listnode_t));
            laux->string = safe_strdup(s);
            laux->next = list;
            laux->prev = list->prev;
            list->prev = laux;
            list = laux;
        }
        else {
            laux = list_add(list->next, s);
            list->next = laux;
            laux->prev = list;
        }
    }

    return list;
}

/*
 * -- list_merge
 *
 * Insert the elements of a list into another list
 *
 */
listnode_t *list_merge(listnode_t *l1, listnode_t *l2)
{
    listnode_t *laux;

    for (laux = l2; laux; laux = laux->next)
        l1 = list_add(l1, laux->string);
    
    return l1;
}

char *tree_to_string(treenode_t *);

/*
 * -- list_make
 *
 * Create a sorted list from an expression tree
 *
 */
listnode_t *
list_make(listnode_t *list, uint8_t type, treenode_t *tree)
{
    char *s;
    
    if (!tree) return NULL;

    switch (tree->type) {
    case Tnot:
    case Tpred:
        s = tree_to_string(tree);
        list = list_add(list, s);
        free(s);
        break;
    case Tand:
    case Tor:
        if (tree->type == type) {
            list = list_make(list, type, tree->left);
            list = list_make(list, type, tree->right);
        } else {
            s = tree_to_string(tree);
            list = list_add(list, s);
            free(s);
        }
        break;
    }

    return list;
}

/*
 * -- tree_to_string
 *
 */
char *
tree_to_string(treenode_t *tree)
{
    char *s = NULL;
    listnode_t *list, *laux;
    
    if (!tree) return NULL;
    
    switch (tree->type) {
    case Tand:
        list = list_make(NULL, Tand, tree->left);
        list = list_merge(list, list_make(NULL, Tand, tree->right));
        s = safe_strdup("(");
        for (laux = list; laux->next; laux = laux->next) {
            s = append_string(s, laux->string);
            s = append_string(s, " && ");
        }
        s = append_string(s, laux->string);
        s = append_string(s, ")");
        /* free the list */
        do {
            laux = list->next;
            free(list->string);
            free(list);
            list = laux;
        } while (laux);
        break;
	case Tor:
        list = list_make(NULL, Tor, tree->left);
        list = list_merge(list, list_make(NULL, Tor, tree->right));
        s = safe_strdup("(");
        for (laux = list; laux->next; laux = laux->next) {
            s = append_string(s, laux->string);
            s = append_string(s, " || ");
        }
        s = append_string(s, laux->string);
        s = append_string(s, ")");
        /* free the list */
        do {
            laux = list->next;
            free(list->string);
            free(list);
            list = laux;
        } while (laux);
        break;
	case Tnot:
        s = safe_strdup("!");
        s = append_string(s, tree_to_string(tree->left));
        break;
    case Tpred:
        s = safe_strdup(tree->string);
        break;
    }
    
    return s;
}

/*
 * -- tree_print_indent
 *
 * Print an expression tree with indentation
 * (only used for debug purposes)
 *
 */
void
tree_print_indent(treenode_t *tree, int indent)
{
    int i;

    if (!tree) return;
    printf("\n");
    printf("     ");
    for (i = 1; i <= indent; i++) printf("   ");
    switch (tree->type) {
    case Tand:
        printf("and");
        break;
	case Tor:
        printf("or");
        break;
	case Tnot:
        printf("not");
        break;
    case Tpred:
        printf("%s", tree->string);
        break;
    }
    tree_print_indent(tree->left, indent + 1);
    tree_print_indent(tree->right, indent + 1);
}

/*
 * -- tree_print
 *
 * Print an expression tree
 * (only used for debug purposes)
 *
 */
void
tree_print(treenode_t *tree)
{
    if (tree) {
        printf("\n\n\nLogical Expression Tree: \n\n");
        tree_print_indent(tree, 0);
        printf("\n\n\n\n");
    } else
        printf("\n\n\nLogical Expression Tree is empty!\n\n");
}

treenode_t *negate(treenode_t *);

/*
 * -- prop_negs
 *
 * Propagate the negations of a tree inward with the following rules:
 *      not(not(A)) => A
 *      not(A and B) => not(A) or not(B)
 *      not(A or B) => not(A) and not(B)
 *
 */
treenode_t *
prop_negs(treenode_t *t)
{
    switch(t->type) {
    case Tnot:
        t = negate(t->left);
        break;
    case Tand:
    case Tor:
        prop_negs(t->left);
        prop_negs(t->right);
        break;
    }

    return t;
}

/*
 * -- negate
 *
 * Negate a node of a tree
 *
 */
treenode_t *
negate(treenode_t *t)
{
    switch(t->type) {
    case Tpred:
        /* Negate the node */
        t = tree_make(Tnot, Tnone, t, NULL, NULL);
        break;
    case Tnot:
        /* Double negation, get rid of it */
        t = prop_negs(t->left);
        break;
    case Tand:
        /* not(A and B) => not(A) or not(B) */
        t->type = Tor;
        t->left = negate(t->left);
        t->right = negate(t->right);
        break;
    case Tor:
        /* not(A or B) => not(A) and not(B) */
        t->type = Tand;
        t->left = negate(t->left);
        t->right = negate(t->right);
        break;
    }

    return t;
}

/*
 * -- tree_copy
 *
 * Duplicate a tree and return a pointer to the new copy
 *
 */
treenode_t *
tree_copy(treenode_t *t)
{
    treenode_t *taux, *taux_left, *taux_right;

    if (!t)
        return NULL;
    
    taux_left = tree_copy(t->left);
    taux_right = tree_copy(t->right);
    taux = tree_make(t->type, t->pred_type, taux_left, taux_right, t->data);

    return taux;
}

/*
 * -- or_and
 *
 * Propagate conjunctions outward with the following rule:
 *      A or (B and C) => (A or B) and (A or C)
 *
 */
treenode_t *
or_and(treenode_t *t)
{
    treenode_t *taux;
    uint8_t type;
    
    switch(t->left->type) {
        case Tor:
            type = t->left->type;
            t->left = or_and(t->left);
            if (t->left->type != type) {
                t = or_and(t);
                /* No need to check the right child */
                return t;
            }
            break;
        case Tand:
            /* Apply the rule ... */
            t->type = Tand;
            t->left->type = Tor;
            taux = t->left->right;
            t->left->right = t->right;
            t->right = tree_make(Tor, Tnone, taux,
                                 tree_copy(t->left->right), NULL);
            
            t->left = or_and(t->left);
            t->right = or_and(t->right);
            
            /* No need to check the right child */
            return t;
            
            break;
    }

    switch(t->right->type) {
        case Tor:
            type = t->right->type;
            t->right = or_and(t->right);
            if (t->right->type != type)
                t = or_and(t);
            break;
        case Tand:
            /* Apply the rule ... */
            t->type = Tand;
            t->right->type = Tor;
            taux = t->right->left;
            t->right->left = t->left;
            t->left = tree_make(Tor, Tnone, tree_copy(t->right->left),
                                taux, NULL);
            
            t->left = or_and(t->left);
            t->right = or_and(t->right);
            
            break;
    }    
    
    return t;
}

/*
 * -- prop_conjs
 *
 * Propagate conjunctions outward with the following rule:
 *      A or (B and C) => (A or B) and (A or C)
 *
 */
treenode_t *
prop_conjs(treenode_t *t)
{
    switch(t->type) {
    case Tnot:
        prop_conjs(t->left);
        break;
    case Tand:
        prop_conjs(t->left);
        prop_conjs(t->right);
        break;
    case Tor:
        t = or_and(t);
        break;
    }

    return t;
}

/*
 * -- cnf
 *
 * Transform an expression tree to Conjunctive Normal Form
 *
 */
treenode_t *
cnf(treenode_t *t)
{
    /* 1. Propagate negations inward */
    t = prop_negs(t);
    /* 2. Propagate conjunctions outward */
    t = prop_conjs(t);
    
    return t;
}

/*
 * -- evaluate_pred
 *
 * Evaluate a predicate expression
 *
 */
int evaluate_pred(treenode_t *t, pkt_t *pkt)
{
    int z = 0;

    if (t != NULL) {
        switch(t->pred_type) {
        case Tip:
            if (!isIP) return 0;
            if (t->data->ipaddr.direction == 0)
                z = ((N32(IP(src_ip)) & t->data->ipaddr.nm) ==
                     t->data->ipaddr.ip);
            else
                z = ((N32(IP(dst_ip)) & t->data->ipaddr.nm) ==
                     t->data->ipaddr.ip);
            break;
        case Tport:
            if (!isTCP && !isUDP)
                return 0;
            if (t->data->ports.direction == 0) {
                if (isTCP)
                    z = (H16(TCP(src_port)) >= t->data->ports.lowport &&
                         H16(TCP(src_port)) <= t->data->ports.highport);
                else /* udp */
                    z = (H16(UDP(src_port)) >= t->data->ports.lowport &&
                         H16(UDP(src_port)) <= t->data->ports.highport);
            } else {
                if (isTCP)
                    z = (H16(TCP(dst_port)) >= t->data->ports.lowport &&
                         H16(TCP(dst_port)) <= t->data->ports.highport);
                else /* udp */
                    z = (H16(UDP(dst_port)) >= t->data->ports.lowport &&
                         H16(UDP(dst_port)) <= t->data->ports.highport);
            }
            break;
   	case Tiface: 
	    if (COMO(type) != COMOTYPE_NF) 
		return 0; 
	    if (t->data->iface.direction == 0) 
		z = (H16(NF(input)) == t->data->iface.index); 
	    else 
		z = (H16(NF(output)) == t->data->iface.index); 
            break;
	case Texporter:
	    if (COMO(type) != COMOTYPE_NF) 
		return 0; 
	    z = (N32(NF(exaddr)) == t->data->exaddr.ip);
            break;
        case Tproto:
            switch(t->data->proto) {
            case ETHERTYPE_IP:
                z = isIP;
                break;
            case IPPROTO_TCP:
                z = isTCP;
                break;
            case IPPROTO_UDP:
                z = isUDP;
                break;
            case IPPROTO_ICMP:
                z = isICMP;
                break;
            }
            break;
        }
    }

    return z;
}

/*
 * -- evaluate
 *
 * Evaluate an expression tree
 *
 */
int evaluate(treenode_t *t, pkt_t *pkt)
{
    int x,y,z = 0;
    
    if (t == NULL) return 1;
    else {
        if (t->type != Tpred) {
            x = evaluate(t->left, pkt);
            /* Shortcuts */
            if ((!x && t->type == Tand) || (x && t->type == Tor))
                return x;
            y = evaluate(t->right, pkt);
            switch(t->type) {
            case Tand:
                z = x && y;
                break;
            case Tor:
                z = x || y;
                break;
            case Tnot:
                z = (x == 0)? 1 : 0;
                break;
            }
        } else {
            z = evaluate_pred(t, pkt);
        }
    }

    return z;
}



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 779 "filter-syntax.y"
typedef union YYSTYPE {
    char *string;
    uint8_t byte;
    uint16_t word;
    uint32_t dword;
    treenode_t *tree;
    ipaddr_t ipaddr;
    portrange_t portrange;
    iface_t iface;
    ipaddr_t exaddr;
} YYSTYPE;
/* Line 196 of yacc.c.  */
#line 837 "filter-syntax.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 219 of yacc.c.  */
#line 849 "filter-syntax.c"

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T) && (defined (__STDC__) || defined (__cplusplus))
# include <stddef.h> /* INFRINGES ON USER NAME SPACE */
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if defined (__STDC__) || defined (__cplusplus)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     define YYINCLUDED_STDLIB_H
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2005 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM ((YYSIZE_T) -1)
#  endif
#  ifdef __cplusplus
extern "C" {
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if (! defined (malloc) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if (! defined (free) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifdef __cplusplus
}
#  endif
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE))			\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  25
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   53

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  19
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  8
/* YYNRULES -- Number of rules. */
#define YYNRULES  25
/* YYNRULES -- Number of states. */
#define YYNSTATES  43

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   273

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned char yyprhs[] =
{
       0,     0,     3,     5,     7,    11,    17,    21,    27,    30,
      35,    39,    41,    43,    45,    47,    49,    52,    56,    59,
      63,    67,    72,    74,    76,    79
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      20,     0,    -1,    21,    -1,     9,    -1,    21,     4,    21,
      -1,     6,    21,     4,    21,     7,    -1,    21,     5,    21,
      -1,     6,    21,     5,    21,     7,    -1,     3,    21,    -1,
       3,     6,    21,     7,    -1,     6,    21,     7,    -1,    22,
      -1,    23,    -1,    24,    -1,    25,    -1,    26,    -1,    11,
      18,    -1,    11,    18,    17,    -1,    12,    16,    -1,    12,
      16,     8,    -1,    12,     8,    16,    -1,    12,    16,     8,
      16,    -1,    14,    -1,    15,    -1,    13,    16,    -1,    10,
      18,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   811,   811,   819,   827,   831,   835,   839,   843,   848,
     852,   856,   860,   864,   868,   872,   877,   885,   894,   900,
     906,   912,   919,   923,   928,   934
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NOT", "AND", "OR", "OPENBR", "CLOSEBR",
  "COLON", "ALL", "EXPORTER", "DIR", "PORTDIR", "IFACE", "LEVEL3",
  "LEVEL4", "NUMBER", "NETMASK", "IPADDR", "$accept", "filter", "expr",
  "ip", "port", "proto", "iface", "exporter", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    19,    20,    20,    21,    21,    21,    21,    21,    21,
      21,    21,    21,    21,    21,    21,    22,    22,    23,    23,
      23,    23,    24,    24,    25,    26
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     1,     3,     5,     3,     5,     2,     4,
       3,     1,     1,     1,     1,     1,     2,     3,     2,     3,
       3,     4,     1,     1,     2,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       0,     0,     0,     3,     0,     0,     0,     0,    22,    23,
       0,     2,    11,    12,    13,    14,    15,     0,     8,     0,
      25,    16,     0,    18,    24,     1,     0,     0,     0,     0,
       0,    10,    17,    20,    19,     4,     6,     9,     4,     6,
      21,     5,     7
};

/* YYDEFGOTO[NTERM-NUM]. */
static const yysigned_char yydefgoto[] =
{
      -1,    10,    11,    12,    13,    14,    15,    16
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -17
static const yysigned_char yypact[] =
{
       0,    21,    34,   -17,   -16,   -14,    14,   -11,   -17,   -17,
      19,     3,   -17,   -17,   -17,   -17,   -17,    34,   -17,    13,
     -17,     4,     7,    30,   -17,   -17,    34,    34,    46,    34,
      34,   -17,   -17,   -17,    23,   -17,   -17,   -17,    35,    36,
     -17,   -17,   -17
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -17,   -17,    -1,   -17,   -17,   -17,   -17,   -17
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
      18,    19,    20,     1,    21,    24,     2,    26,    27,     3,
       4,     5,     6,     7,     8,     9,    28,    29,    30,    25,
      31,    32,    22,    33,     1,    35,    36,    17,    38,    39,
      23,     4,     5,     6,     7,     8,     9,     1,    34,    40,
       2,     0,    41,    42,     4,     5,     6,     7,     8,     9,
      29,    30,     0,    37
};

static const yysigned_char yycheck[] =
{
       1,     2,    18,     3,    18,    16,     6,     4,     5,     9,
      10,    11,    12,    13,    14,    15,    17,     4,     5,     0,
       7,    17,     8,    16,     3,    26,    27,     6,    29,    30,
      16,    10,    11,    12,    13,    14,    15,     3,     8,    16,
       6,    -1,     7,     7,    10,    11,    12,    13,    14,    15,
       4,     5,    -1,     7
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     3,     6,     9,    10,    11,    12,    13,    14,    15,
      20,    21,    22,    23,    24,    25,    26,     6,    21,    21,
      18,    18,     8,    16,    16,     0,     4,     5,    21,     4,
       5,     7,    17,    16,     8,    21,    21,     7,    21,    21,
      16,     7,     7
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (0)


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (0)
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
              (Loc).first_line, (Loc).first_column,	\
              (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr,					\
                  Type, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname[yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      size_t yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

#endif /* YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);


# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short int *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a look-ahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to look-ahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 812 "filter-syntax.y"
    {
        if (filter_tree != NULL)
            *filter_tree = tree_copy((yyvsp[0].tree));
        (yyvsp[0].tree) = cnf((yyvsp[0].tree));
        if (filter_cmp != NULL)
	    *filter_cmp = tree_to_string((yyvsp[0].tree));
        ;}
    break;

  case 3:
#line 820 "filter-syntax.y"
    {
        if (filter_tree != NULL)
            *filter_tree = NULL;
        if (filter_cmp != NULL)
	    asprintf(filter_cmp, "all");
        ;}
    break;

  case 4:
#line 828 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tand, Tnone, (yyvsp[-2].tree), (yyvsp[0].tree), NULL);
      ;}
    break;

  case 5:
#line 832 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tand, Tnone, (yyvsp[-3].tree), (yyvsp[-1].tree), NULL);
      ;}
    break;

  case 6:
#line 836 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tor, Tnone, (yyvsp[-2].tree), (yyvsp[0].tree), NULL);
      ;}
    break;

  case 7:
#line 840 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tor, Tnone, (yyvsp[-3].tree), (yyvsp[-1].tree), NULL);
      ;}
    break;

  case 8:
#line 844 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tnot, Tnone, (yyvsp[0].tree), NULL, NULL);
        
      ;}
    break;

  case 9:
#line 849 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tnot, Tnone, (yyvsp[-1].tree), NULL, NULL);
      ;}
    break;

  case 10:
#line 853 "filter-syntax.y"
    {
        (yyval.tree) = tree_copy((yyvsp[-1].tree));
      ;}
    break;

  case 11:
#line 857 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tpred, Tip, NULL, NULL, (nodedata_t *)&(yyvsp[0].ipaddr));
      ;}
    break;

  case 12:
#line 861 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tpred, Tport, NULL, NULL, (nodedata_t *)&(yyvsp[0].portrange));
      ;}
    break;

  case 13:
#line 865 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tpred, Tproto, NULL, NULL, (nodedata_t *)&(yyvsp[0].word));
      ;}
    break;

  case 14:
#line 869 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tpred, Tiface, NULL, NULL, (nodedata_t *)&(yyvsp[0].iface));
      ;}
    break;

  case 15:
#line 873 "filter-syntax.y"
    {
        (yyval.tree) = tree_make(Tpred, Texporter, NULL, NULL, (nodedata_t *)&(yyvsp[0].exaddr));
      ;}
    break;

  case 16:
#line 878 "filter-syntax.y"
    {
        (yyval.ipaddr).direction = (yyvsp[-1].byte);
        if (parse_ip((yyvsp[0].string), &((yyval.ipaddr).ip)) == -1)
            YYABORT;
        /* Assume it's a host IP address if we don't have a netmask */
        (yyval.ipaddr).nm = htonl(netmasks[32]);
    ;}
    break;

  case 17:
#line 886 "filter-syntax.y"
    {
        (yyval.ipaddr).direction = (yyvsp[-2].byte);
        if (parse_ip((yyvsp[-1].string), &((yyval.ipaddr).ip)) == -1)
            YYABORT;
        if (parse_nm((yyvsp[0].dword), &((yyval.ipaddr).nm)) == -1)
            YYABORT;
    ;}
    break;

  case 18:
#line 895 "filter-syntax.y"
    {
        (yyval.portrange).direction = (yyvsp[-1].byte);
        (yyval.portrange).lowport = (yyvsp[0].word);
        (yyval.portrange).highport = (yyvsp[0].word);
      ;}
    break;

  case 19:
#line 901 "filter-syntax.y"
    {
        (yyval.portrange).direction = (yyvsp[-2].byte);
        (yyval.portrange).lowport = (yyvsp[-1].word);
        (yyval.portrange).highport = 65535;
      ;}
    break;

  case 20:
#line 907 "filter-syntax.y"
    {
        (yyval.portrange).direction = (yyvsp[-2].byte);
        (yyval.portrange).lowport = 1;
        (yyval.portrange).highport = (yyvsp[0].word);
      ;}
    break;

  case 21:
#line 913 "filter-syntax.y"
    {
        (yyval.portrange).direction = (yyvsp[-3].byte);
        (yyval.portrange).lowport = (yyvsp[-2].word);
        (yyval.portrange).highport = (yyvsp[0].word);
      ;}
    break;

  case 22:
#line 920 "filter-syntax.y"
    {
        (yyval.word) = (yyvsp[0].word);
       ;}
    break;

  case 23:
#line 924 "filter-syntax.y"
    {
        (yyval.word) = (yyvsp[0].word);
       ;}
    break;

  case 24:
#line 929 "filter-syntax.y"
    {
        (yyval.iface).direction = (yyvsp[-1].byte);
	(yyval.iface).index = (yyvsp[0].word);
       ;}
    break;

  case 25:
#line 935 "filter-syntax.y"
    {
           if (parse_ip((yyvsp[0].string), &((yyval.exaddr).ip)) == -1)
               YYABORT;
          ;}
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 2059 "filter-syntax.c"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  int yytype = YYTRANSLATE (yychar);
	  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
	  YYSIZE_T yysize = yysize0;
	  YYSIZE_T yysize1;
	  int yysize_overflow = 0;
	  char *yymsg = 0;
#	  define YYERROR_VERBOSE_ARGS_MAXIMUM 5
	  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
	  int yyx;

#if 0
	  /* This is so xgettext sees the translatable formats that are
	     constructed on the fly.  */
	  YY_("syntax error, unexpected %s");
	  YY_("syntax error, unexpected %s, expecting %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
#endif
	  char *yyfmt;
	  char const *yyf;
	  static char const yyunexpected[] = "syntax error, unexpected %s";
	  static char const yyexpecting[] = ", expecting %s";
	  static char const yyor[] = " or %s";
	  char yyformat[sizeof yyunexpected
			+ sizeof yyexpecting - 1
			+ ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
			   * (sizeof yyor - 1))];
	  char const *yyprefix = yyexpecting;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 1;

	  yyarg[0] = yytname[yytype];
	  yyfmt = yystpcpy (yyformat, yyunexpected);

	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
		  {
		    yycount = 1;
		    yysize = yysize0;
		    yyformat[sizeof yyunexpected - 1] = '\0';
		    break;
		  }
		yyarg[yycount++] = yytname[yyx];
		yysize1 = yysize + yytnamerr (0, yytname[yyx]);
		yysize_overflow |= yysize1 < yysize;
		yysize = yysize1;
		yyfmt = yystpcpy (yyfmt, yyprefix);
		yyprefix = yyor;
	      }

	  yyf = YY_(yyformat);
	  yysize1 = yysize + yystrlen (yyf);
	  yysize_overflow |= yysize1 < yysize;
	  yysize = yysize1;

	  if (!yysize_overflow && yysize <= YYSTACK_ALLOC_MAXIMUM)
	    yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg)
	    {
	      /* Avoid sprintf, as that infringes on the user's name space.
		 Don't have undefined behavior even if the translation
		 produced a string with the wrong number of "%s"s.  */
	      char *yyp = yymsg;
	      int yyi = 0;
	      while ((*yyp = *yyf))
		{
		  if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		    {
		      yyp += yytnamerr (yyp, yyarg[yyi++]);
		      yyf += 2;
		    }
		  else
		    {
		      yyp++;
		      yyf++;
		    }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    {
	      yyerror (YY_("syntax error"));
	      goto yyexhaustedlab;
	    }
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror (YY_("syntax error"));
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
        {
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
        }
      else
	{
	  yydestruct ("Error: discarding", yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (0)
     goto yyerrorlab;

yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping", yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token. */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK;
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 940 "filter-syntax.y"


#include "filter-lexic.c"

void yferror(char *fmt, ...)
{ 
    va_list ap;
    char error[255];
    
    va_start(ap, fmt);
    vsnprintf(error, sizeof(error), fmt, ap);
    logmsg(LOGWARN, "Filter parser error: %s\n", error);
    va_end(ap);
}

int 
parse_filter(char *f, treenode_t **result_tree, char **result_cmp)
{
    filter_tree = result_tree;
    filter_cmp = result_cmp;
    yf_scan_string(f);
    return yfparse();
}


