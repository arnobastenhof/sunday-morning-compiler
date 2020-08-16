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
  kSemicln = 0x10,   /* ";" */
  kPeriod  = 0x20,  /* "." */
  kEof     = 0x40   /* end of file */
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
static char     g_line[81];     /* Line buffer (incl. terminating '\0') */
static int      g_cc = 0;       /* Character count */
static int      g_ll = 0;       /* Line length */
static uint32_t g_errs = 0;     /* Encountered errors (used as a bitset) */

/* Error marks an error in the source file */
static void
Error(const int errno)
{
  assert(errno >= 1 && errno <= 22);
  printf(" ****%*c^%d\n", g_cc-1, ' ', errno);
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
    /*  8    */ "\".\" inserted",
  };
  int i;

  printf("  key words\n");
  for (i = 1; i <= 22; ++i) {
    if (g_errs & (1 << (i-1))) {
      printf("%*c%2d  %s\n", 9, ' ', i, g_errmsg[i]);
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
    printf("      ");

    /* Echo each char in this line to stdout and append it to the line buffer */
    g_ll = 0;
    while ((ch = fgetc(g_src)) != '\n' && ch != EOF && g_ll < 80) {
      putchar(ch);
      g_line[g_ll++] = ch;
    }
    putchar('\n');

    /* Validate line length */
    if (ch != '\n' && ch != EOF && g_ll == 80) {
      Error(1);
      ungetc(ch, stdin);
      ch = '\n';
    }

    /* Append newline or EOF and reset character count to start of line */
    assert(ch == '\n' || ch == EOF);
    g_line[g_ll++] = ch;
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
  } else {
    /* Lookup symbol for single character */
    g_sym = g_ch_traits[g_ch] & ~(kAlpha | kDigit | kWhite);
    GetChar();
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

static void
SourceFile(void)
{
  fprintf(g_dest, "_start:\n");

  GetSymbol();
  Expect(kProgram, 3);
  Expect(kIdent, 4);
  Expect(kSemicln, 5);
  Expect(kBegin, 6);
  Expect(kEnd, 7);
  Expect(kPeriod, 8);

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

  if ((g_dest = fopen("out.s", "w"))) {
    /* Print assembly header */
    fprintf(g_dest, "BITS 64\n"
      "GLOBAL _start\n"
      "SECTION .text\n");

    /* Compile */
    SourceFile();
    fclose(g_dest);
  } else {
    perror("out.s");
  }

  /* Check for errors */
  if ((sc = g_errs != 0)) {
    printf("  errors in source file\n");
    PrintErrors();
  }

  /* 0 indicates success, 1 failure */
  return sc;
}

int
main(int argc, char *argv[])
{
  int sc = 1;

  if (argc != 2) {
    fprintf(stderr, "%s: expected one argument\n", argv[0]);
  } else if (argc == 2) {
    if ((g_src = fopen(argv[1], "r")) != NULL) {
      sc = Compile();
      if (fclose(g_src) == EOF) {
        perror(argv[0]);
      }
    } else {
      perror(argv[0]);
    }
  }

  return sc;
}
