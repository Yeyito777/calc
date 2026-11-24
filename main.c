#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *src, *p;
static jmp_buf fail;
static int depth;

static void die(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "calc: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  longjmp(fail, 1);
}

static void unexpected(void) {
  if (*p) die("unexpected '%c' at %ld", *p, (long)(p - src) + 1);
  die("unexpected end");
}

static void skip(void) { while (isspace((unsigned char)*p)) p++; }
static int eat(char c) { skip(); if (*p == c) { p++; return 1; } return 0; }

static const struct { const char *name; double (*fn)(double); } funcs[] = {
  {"sqrt", sqrt}, {"cbrt", cbrt}, {"abs", fabs}, {"exp", exp},
  {"ln", log}, {"log", log10}, {"log2", log2},
  {"sin", sin}, {"cos", cos}, {"tan", tan},
  {"asin", asin}, {"acos", acos}, {"atan", atan},
  {"sinh", sinh}, {"cosh", cosh}, {"tanh", tanh},
  {"floor", floor}, {"ceil", ceil}, {"round", round},
};

static double expr(void);

static double primary(void) {
  skip();
  const char *start = p;
  char c = *p;
  if (c == '(' || c == '[' || c == '{') {
    char close = c == '(' ? ')' : c == '[' ? ']' : '}';
    if (++depth > 1000) die("too deeply nested");
    p++;
    double v = expr();
    if (!eat(close)) unexpected();
    depth--;
    return v;
  }
  if (isdigit((unsigned char)c) || c == '.') {
    char *end;
    double v = strtod(p, &end);
    if (end == p) unexpected();
    p = end;
    return v;
  }
  if (isalpha((unsigned char)c)) {
    while (isalnum((unsigned char)*p)) p++;
    size_t n = p - start;
    if (n == 2 && !strncmp(start, "pi", 2)) return M_PI;
    if (n == 1 && *start == 'e') return M_E;
    for (size_t i = 0; i < sizeof funcs / sizeof *funcs; i++)
      if (strlen(funcs[i].name) == n && !strncmp(start, funcs[i].name, n))
        return funcs[i].fn(primary());
    p = start;
    die("unknown name '%.*s' at %ld", (int)n, start, (long)(start - src) + 1);
  }
  unexpected();
  return 0;
}

static double postfix(void) {
  double v = primary();
  while (eat('!')) v = tgamma(v + 1);
  return v;
}

static double unary(void);

/* Right-associative: 2^3^2 = 2^9. Exponent may be signed: 2^-1. */
static double power(void) {
  double b = postfix();
  skip();
  if (*p == '^' || (p[0] == '*' && p[1] == '*')) {
    p += *p == '^' ? 1 : 2;
    return pow(b, unary());
  }
  if (*p == 'r' && !isalpha((unsigned char)p[1])) { /* 27r3 = cube root of 27 */
    p++;
    return pow(b, 1 / unary());
  }
  return b;
}

/* -2^2 = -4, as in written math. */
static double unary(void) {
  if (eat('-')) return -unary();
  if (eat('+')) return unary();
  return power();
}

static double term(void) {
  double v = unary();
  for (;;) {
    skip();
    if (eat('*')) v *= unary();
    else if (eat('/')) v /= unary();
    else if (eat('%')) v = fmod(v, unary());
    else if (*p == '(' || *p == '[' || *p == '{' || isalpha((unsigned char)*p))
      v *= power(); /* 2pi, 3(4+1) */
    else return v;
  }
}

static double expr(void) {
  double v = term();
  for (;;) {
    if (eat('+')) v += term();
    else if (eat('-')) v -= term();
    else return v;
  }
}

static int run(const char *s) {
  src = p = s;
  depth = 0;
  if (setjmp(fail)) return 1;
  double v = expr();
  skip();
  if (*p) unexpected();
  if (v == 0) v = 0; /* no -0 */
  if (v == floor(v) && fabs(v) < 1e15) printf("%.0f\n", v);
  else printf("%.12g\n", v);
  return 0;
}

int main(int argc, char **argv) {
  if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
    puts("usage: calc EXPR    (no args: one expression per line from stdin)\n"
         "  + - * / % ^ ** r(root) ! () [] {}    2pi  3(4+1)  1e3  0xff\n"
         "  pi e sqrt cbrt abs exp ln log log2 sin cos tan asin acos atan\n"
         "  sinh cosh tanh floor ceil round");
    return 0;
  }
  if (argc > 1) {
    size_t n = 1;
    for (int i = 1; i < argc; i++) n += strlen(argv[i]) + 1;
    char *s = malloc(n), *q = s;
    for (int i = 1; i < argc; i++) q += sprintf(q, i > 1 ? " %s" : "%s", argv[i]);
    return run(s);
  }
  char *line = NULL;
  size_t cap = 0;
  int rc = 0;
  while (getline(&line, &cap, stdin) > 0) {
    const char *s = line;
    while (isspace((unsigned char)*s)) s++;
    if (!*s) continue;
    rc |= run(line);
    fflush(stdout);
  }
  return rc;
}
