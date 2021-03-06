#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Various compile-time constants */
enum {
  kNumKw = 3,                 /* Number of keywords */
  kIdLen = 11,                /* Identifier length (incl. terminating '\0') */
};

/* Symbol sets */
typedef uint32_t symset_t;

/* Individual symbols are defined as singleton symbol sets */
typedef enum {
  kNul     = 0x0,   /* Invalid character */
  kProgram = 0x1,   /* Keyword "Program" */
  kBegin   = 0x2,   /* Keyword "Begin" */
  kEnd     = 0x4,   /* Keyword "End" */
  kIdent   = 0x8,   /* Identifier */
  kNumber  = 0x10,  /* Number (integer) */
  kSemicln = 0x20,  /* ";" */
  kPeriod  = 0x40,  /* "." */
  kEof     = 0x80   /* end of file */
} symbol_t;

/* Character kinds; defined not to overlap with symbols */
enum {
  kAlpha   = 0x10000000,      /* Letters a..z and A..Z */
  kDigit   = 0x20000000,      /* Digits 0..9 */
  kWhite   = 0x40000000       /* Whitespace */
};

/* Reserved words */
const char * const g_kw[kNumKw] = {
  "Begin", "End", "Program"
};

/* Keyword symbols */
const symbol_t g_kw_sym[kNumKw] = {
  kBegin, kEnd, kProgram
};

/* Association of (ASCII) characters with their symbol and kind */
const uint32_t g_ch_traits[] = {
  kNul,    kNul,    kNul,    kNul,     kNul,    kNul,     kNul,    /*   0-6   */
  kNul,    kNul,    kWhite,  kWhite,   kWhite,  kWhite,   kWhite,  /*   7-13  */
  kNul,    kNul,    kNul,    kNul,     kNul,    kNul,     kNul,    /*  14-20  */
  kNul,    kNul,    kNul,    kNul,     kNul,    kNul,     kNul,    /*  21-27  */
  kNul,    kNul,    kNul,    kNul,     kWhite,  kNul,     kNul,    /*  28-34  */
  kNul,    kNul,    kNul,    kNul,     kNul,    kNul,     kNul,    /*  35-41  */
  kNul,    kNul,    kNul,    kNul,     kPeriod, kNul,     kDigit,  /*  42-48  */
  kDigit,  kDigit,  kDigit,  kDigit,   kDigit,  kDigit,   kDigit,  /*  49-55  */
  kDigit,  kDigit,  kNul,    kSemicln, kNul,    kNul,     kNul,    /*  56-62  */
  kNul,    kNul,    kAlpha,  kAlpha,   kAlpha,  kAlpha,   kAlpha,  /*  63-69  */
  kAlpha,  kAlpha,  kAlpha,  kAlpha,   kAlpha,  kAlpha,   kAlpha,  /*  70-76  */
  kAlpha,  kAlpha,  kAlpha,  kAlpha,   kAlpha,  kAlpha,   kAlpha,  /*  77-83  */
  kAlpha,  kAlpha,  kAlpha,  kAlpha,   kAlpha,  kAlpha,   kAlpha,  /*  84-90  */
  kNul,    kNul,    kNul,    kNul,     kNul,    kNul,     kAlpha,  /*  91-97  */
  kAlpha,  kAlpha,  kAlpha,  kAlpha,   kAlpha,  kAlpha,   kAlpha,  /*  98-104 */
  kAlpha,  kAlpha,  kAlpha,  kAlpha,   kAlpha,  kAlpha,   kAlpha,  /* 105-111 */
  kAlpha,  kAlpha,  kAlpha,  kAlpha,   kAlpha,  kAlpha,   kAlpha,  /* 112-118 */
  kAlpha,  kAlpha,  kAlpha,  kAlpha,   kNul,    kNul,     kNul,    /* 119-125 */
  kNul,    kNul                                                    /* 126-127 */
};

/* Global state */
static FILE *   g_src;          /* Source file */
static FILE *   g_dest;         /* Destination file */
static int      g_ch = ' ';     /* Lookahead character */
static symbol_t g_sym = kNul;   /* Lookahead symbol */
static char     g_id[kIdLen];   /* Last parsed identifier */
static int32_t  g_num;          /* Last parsed number */
static char     g_line[81];     /* Line buffer (incl. terminating '\0') */
static int      g_cc = 0;       /* Character count */
static int      g_ll = 0;       /* Line length */
static uint32_t g_errs = 0;     /* Encountered errors (used as a bitset) */

/* Error marks an error in the source file */
static void
Error(const int errno)
{
  fprintf(stderr, "     %s", g_line);
  fprintf(stderr, " ****%*c^%d\n", g_cc-1, ' ', errno);
  g_errs |= (1 << (errno-1));
}

/* PrintErrors writes descriptive messages for encountered compilation errors */
static void
PrintErrors(void)
{
  /* Error messages */
  static const char * const g_errmsg[] = {
    /*  0,1  */ "",                             "maximum line size exceeded",
    /*  2,3  */ "symbol deleted",               "\"Program\" inserted",
    /*  4,5  */ "identifier inserted",          "\";\" inserted",
    /*  6,7  */ "\"Begin\" inserted",           "\"End\" inserted",
    /*  8,9  */ "\".\" inserted",               "overflow",
    /* 10,11 */ "EOF inserted",                 "symbol deleted",
    /* 12    */ "number inserted",
  };
  int i;

  fprintf(stderr, "  errors in source file\n  key words\n");
  for (i = 1; i <= 11; ++i) {
    if (g_errs & (1 << (i-1))) {
      fprintf(stderr, "%*c%2d  %s\n", 9, ' ', i, g_errmsg[i]);
    }
  }
}

/* GetChar reads the lookahead character from the source file */
static void
GetChar()
{
  int ch;

  /* Once the EOF is reached, we don't expect requests for more characters */
  assert(g_ch != EOF);

  /* Time to refill the line buffer? */
  if (g_cc == g_ll) {
    assert(g_ch_traits[g_ch] & kWhite);

    /* Append each char in this line to the line buffer */
    g_ll = 0;
    while ((ch = fgetc(g_src)) != '\n' && ch != EOF && g_ll < 80) {
      g_line[g_ll++] = ch;
    }

    /* Validate line length */
    if (ch != '\n' && ch != EOF && g_ll == 80) {
      Error(1);
      ungetc(ch, stdin);
      ch = '\n';
    }

    /* Append newline or EOF and reset character count to start of line */
    assert(ch == '\n' || ch == EOF);
    g_line[g_ll++] = ch;
    g_line[g_ll] = 0;
    g_cc = 0;
  }

  /* Return next character from the current line */
  g_ch = g_line[g_cc++];
}

/* GetSymbol returns the next symbol to the parser */
static void
GetSymbol(void)
{
  int   i;    /* Index variable */

  /* Once the EOF is reached, we don't expect requests for more symbols */
  assert(g_sym != kEof);

  /* Skip whitespace */
  for (; g_ch_traits[g_ch] & kWhite; GetChar())
    ;

  if (g_ch_traits[g_ch] & kAlpha) { /* Identifiers */
    /* Read identifier into g_id */
    i = 0;
    do {
      /* Only the first kIdLen-1 characters are significant */
      if (i != kIdLen - 1) {
        g_id[i++] = g_ch;
      }
      GetChar();
    } while (g_ch_traits[g_ch] & (kAlpha | kDigit));
    g_id[i] = '\0';

    /* Keyword table lookup */
    for (i = 0; i != kNumKw && strcmp(g_kw[i], g_id); ++i)
      ;
    g_sym = (i == kNumKw) ? kIdent : g_kw_sym[i];
  } else if (g_ch_traits[g_ch] & kDigit) { /* Numbers */
    g_num = 0;
    while (g_ch_traits[g_ch] & kDigit) {
      /* Guard against overflow */
      if (g_num > (INT32_MAX - (g_ch - '0'))/10) {
        Error(9);
        g_num = 0;
      } else {
        g_num = (10 * g_num) + (g_ch - '0');
      }
      GetChar();
    }
    g_sym = kNumber;
  } else if (g_ch == EOF) {
    g_sym = kEof;
  } else {
    /* Lookup symbol for single character */
    g_sym = g_ch_traits[g_ch] & ~(kAlpha | kDigit | kWhite);
    GetChar();
  }
}

/* Test brings the parser back in step after an unexpected lookahead symbol */
static void
Test(const symset_t fset)
{
  assert(kEof & fset);

  for (; !(g_sym & fset); GetSymbol()) {
    Error(11);
  }
}

/* Expect tests the last read symbol against an expected value */
static inline void
Expect(const symbol_t exp, const int errno)
{
  if (exp == g_sym) {
    GetSymbol();
  } else {
    Error(errno);
  }
}

/* Expression = number . */
static void
Expression(const symset_t fset)
{
  Test(kNumber | fset);
  if (g_sym == kNumber) {
    GetSymbol();
    fprintf(g_dest, "  mov ecx, %d\n", g_num);
    Test(fset);
  } else {
    Error(12);
  }
}

static void
Block(const symset_t fset)
{
  /* For now we expect >= 0 expressions, each simply a number */
  Test(kBegin | kEnd | fset);
  Expect(kBegin, 6);
  for (;;) {
    Test(kNumber | kEnd | fset);
    if (g_sym != kNumber) {
      break;
    }
    Expression(kSemicln | kEnd | fset);
    Expect(kSemicln, 5);
  }
  Expect(kEnd, 7);
  Test(fset);
}

static void
ProgramDeclaration(const symset_t fset)
{
  Test(kProgram | fset);
  Expect(kProgram, 3);
  Expect(kIdent, 4);
  Expect(kSemicln, 5);
  Test(fset);
}

static void
SourceFile(void)
{
  /* Print assembly header */
  fprintf(g_dest, "BITS 64\n"
    "GLOBAL _start\n"
    "SECTION .text\n");

  GetSymbol();
  ProgramDeclaration(kBegin | kEof);
  fprintf(g_dest, "_start:\n");
  Block(kPeriod | kEof);
  Expect(kPeriod, 8);
  if (g_sym != kEof) {
    Error(10);
  }

  /* _exit(0); */
  fprintf(g_dest,
      "  mov  eax, 1\n"
      "  mov  ebx, 0\n"
      "  int  0x80\n");
}

static int
Compile(void)
{
  int sc;

  assert(g_src != NULL);

  /* At present only writing to stdout is supported */
  g_dest = stdout;

  /* Compile */
  SourceFile();

  /* Check for errors */
  if ((sc = g_errs != 0)) {
    PrintErrors();
  }

  /* 0 indicates success, 1 failure */
  return sc;
}

int
main(int argc, char *argv[])
{
  int sc = 1;

  if (argc == 1) {
    g_src = stdin;
    sc = Compile();
  } else if (argc == 2) {
    if ((g_src = fopen(argv[1], "r")) != NULL) {
      sc = Compile();
      if (fclose(g_src) == EOF) {
        perror(argv[0]);
      }
    } else {
      perror(argv[0]);
    }
  } else {
    fprintf(stderr, "%s: too many arguments\n", argv[0]);
  }

  return sc;
}
