#include <ctype.h>
#include <float.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *src, *p;
static jmp_buf fail;
static int depth;

/* A value with a bound on its accumulated rounding error (e) and the largest
   magnitude seen while computing it (m). A result within its own error of
   zero is zero: sin(pi) = 0, 1-0.9-0.1 = 0. */
typedef struct { double v, e, m; } Val;

static double scaled(double c, double e) { return e == 0 ? 0 : fabs(c) * e; }

static Val mk(double v, double e, double m) {
  e += fabs(v) * DBL_EPSILON;
  m = fmax(m, fabs(v));
  if (isfinite(e) && fabs(v) <= 8 * e && e <= 1e-9 * m) v = 0;
  return (Val){v, e, m};
}

static Val lit(double v, int exact) { return (Val){v, exact ? 0 : fabs(v) * DBL_EPSILON, fabs(v)}; }

static Val add(Val a, Val b) { return mk(a.v + b.v, a.e + b.e, fmax(a.m, b.m)); }
static Val neg(Val a) { a.v = -a.v; return a; }
static Val mul(Val a, Val b) {
  return mk(a.v * b.v, scaled(a.v, b.e) + scaled(b.v, a.e) + a.e * b.e, fmax(a.m, b.m));
}
static Val dvd(Val a, Val b) {
  double r = a.v / b.v;
  return mk(r, (a.e + scaled(r, b.e)) / fabs(b.v), fmax(a.m, b.m));
}
static Val mod(Val a, Val b) {
  return mk(fmod(a.v, b.v), a.e + scaled(trunc(a.v / b.v), b.e), fmax(a.m, b.m));
}
static Val pw(Val a, Val b) {
  double r = pow(a.v, b.v);
  double e = scaled(b.v * pow(a.v, b.v - 1), a.e) + scaled(r * log(fabs(a.v)), b.e);
  return mk(r, e, fmax(a.m, b.m));
}
static Val recip(Val a) { return (Val){1 / a.v, scaled(1 / (a.v * a.v), a.e), a.m}; }

/* Error through f is estimated from its slope around x. */
static Val apply(double (*f)(double), int smooth, Val x) {
  double r = f(x.v), slope = 0;
  if (smooth && x.e > 0) {
    double h = fmax(x.e, fabs(x.v) * DBL_EPSILON);
    slope = fabs(f(x.v + h) - f(x.v - h)) / (2 * h);
  }
  return mk(r, scaled(slope, x.e), slope);
}

static double fact(double x) { return tgamma(x + 1); }

/* Whether the literal s..end is a whole number that v holds exactly (1e22 is, 1e23 isn't). */
static int exact(const char *s, const char *end, double v) {
  if (v != floor(v) || !isfinite(v)) return 0;
  if (v < 0x1p53) return 1;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return 0;
  char want[400], got[400];
  int n = 0, shift = 0, seen = 0;
  for (; s < end && *s != 'e' && *s != 'E'; s++) {
    if (*s == '.') { seen = 1; continue; }
    if (n || *s != '0') { if (n >= 380) return 0; want[n++] = *s; }
    if (seen) shift--;
  }
  if (s < end) shift += atoi(s + 1);
  while (shift < 0 && n && want[n - 1] == '0') n--, shift++;
  if (shift < 0 || n + shift >= 400) return 0;
  while (shift-- > 0) want[n++] = '0';
  want[n] = 0;
  snprintf(got, sizeof got, "%.0f", v);
  return !strcmp(want, got);
}

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

static const struct { const char *name; double (*fn)(double); int smooth; } funcs[] = {
  {"sqrt", sqrt, 1}, {"cbrt", cbrt, 1}, {"abs", fabs, 1}, {"exp", exp, 1},
  {"ln", log, 1}, {"log", log10, 1}, {"log2", log2, 1},
  {"sin", sin, 1}, {"cos", cos, 1}, {"tan", tan, 1},
  {"asin", asin, 1}, {"acos", acos, 1}, {"atan", atan, 1},
  {"sinh", sinh, 1}, {"cosh", cosh, 1}, {"tanh", tanh, 1},
  {"floor", floor, 0}, {"ceil", ceil, 0}, {"round", round, 0},
};

static Val expr(void);

static Val primary(void) {
  skip();
  const char *start = p;
  char c = *p;
  if (c == '(' || c == '[' || c == '{') {
    char close = c == '(' ? ')' : c == '[' ? ']' : '}';
    if (++depth > 1000) die("too deeply nested");
    p++;
    Val v = expr();
    if (!eat(close)) unexpected();
    depth--;
    return v;
  }
  if (isdigit((unsigned char)c) || c == '.') {
    char *end;
    double v = strtod(p, &end);
    if (end == p) unexpected();
    Val r = lit(v, exact(p, end, v));
    p = end;
    return r;
  }
  if (isalpha((unsigned char)c)) {
    while (isalnum((unsigned char)*p)) p++;
    size_t n = p - start;
    if (n == 2 && !strncmp(start, "pi", 2)) return lit(M_PI, 0);
    if (n == 1 && *start == 'e') return lit(M_E, 0);
    for (size_t i = 0; i < sizeof funcs / sizeof *funcs; i++)
      if (strlen(funcs[i].name) == n && !strncmp(start, funcs[i].name, n))
        return apply(funcs[i].fn, funcs[i].smooth, primary());
    p = start;
    die("unknown name '%.*s' at %ld", (int)n, start, (long)(start - src) + 1);
  }
  unexpected();
  return lit(0, 1);
}

static Val postfix(void) {
  Val v = primary();
  while (eat('!')) v = apply(fact, 1, v);
  return v;
}

static Val unary(void);

/* Right-associative: 2^3^2 = 2^9. Exponent may be signed: 2^-1. */
static Val power(void) {
  Val b = postfix();
  skip();
  if (*p == '^' || (p[0] == '*' && p[1] == '*')) {
    p += *p == '^' ? 1 : 2;
    return pw(b, unary());
  }
  if (*p == 'r' && !isalpha((unsigned char)p[1])) { /* 27r3 = cube root of 27 */
    p++;
    return pw(b, recip(unary()));
  }
  return b;
}

/* -2^2 = -4, as in written math. */
static Val unary(void) {
  if (eat('-')) return neg(unary());
  if (eat('+')) return unary();
  return power();
}

static Val term(void) {
  Val v = unary();
  for (;;) {
    skip();
    if (eat('*')) v = mul(v, unary());
    else if (eat('/')) v = dvd(v, unary());
    else if (eat('%')) v = mod(v, unary());
    else if (*p == '(' || *p == '[' || *p == '{' || isalpha((unsigned char)*p))
      v = mul(v, power()); /* 2pi, 3(4+1) */
    else return v;
  }
}

static Val expr(void) {
  Val v = term();
  for (;;) {
    if (eat('+')) v = add(v, term());
    else if (eat('-')) v = add(v, neg(term()));
    else return v;
  }
}

static int run(const char *s) {
  src = p = s;
  depth = 0;
  if (setjmp(fail)) return 1;
  double v = expr().v;
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
