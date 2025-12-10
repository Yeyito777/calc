#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define EQLEN 2048

typedef struct {
  char symbol;
  int bp;
} Operator;

typedef struct {
  char symbol;
  int bp;
} Grouper;

Grouper groupers[] = {
  {'(',10},
  {')',-10},
  {'[',1000},
  {']',-1000},
  {'{',100000},
  {'}',-100000},
};

Operator operators[] = {
  {'+',1},
  {'-',1},
  {'*',2},
  {'/',2},
  {'^',3},
  {'r',3}, // Root
};

typedef enum {
  OPERATOR,
  NUMBER,
} Tokentype;

typedef struct {
  union {
    Operator op;
    double num;
  } val;
  Tokentype type;
} Token;
#define MAXTOKENS 1024

int getbp(char op) {
  for (int i = 0; i < sizeof(operators)/sizeof(Operator); i++) {
    if (operators[i].symbol == op) {
      return operators[i].bp;
    }
  }
  return -1;
}

int getgbp(char gp) {
  for (int i = 0; i < sizeof(groupers)/sizeof(Grouper); i++) {
    if (groupers[i].symbol == gp) {
      return groupers[i].bp;
    }
  }
  return -1;
}

void eval(Token *tokens,int *tokensc) {
  Token *mostbptoken = NULL;
  int mostbp = 0;
  for (int i = 0; i < *tokensc; i++)
    if (tokens[i].type == OPERATOR && tokens[i].val.op.bp > mostbp) {
      mostbptoken = tokens+i;
      mostbp = tokens[i].val.op.bp;
    }
  
  if (mostbptoken == NULL) {
    fprintf(stderr,"Unable to find the most bp token.\n");
    exit(1);
  }
  
  double result = 0;
  double num1 = (*(mostbptoken-1)).val.num;
  double num2 = (*(mostbptoken+1)).val.num;
  switch ((*mostbptoken).val.op.symbol) {
    case '+':
      result = num1+num2;
      break;
    case '-':
      result = num1-num2;
      break;
    case '*':
      result = num1*num2;
      break;
    case '/':
      result = num1/num2;
      break;
    case '^':
      result = pow(num1,num2);
      break;
    case 'r':
      result = pow(num1,1.0/num2);
      break;
    default:
      fprintf(stderr,"Symbol not found in eval. Symbol: %c\n",(*mostbptoken).val.op.symbol);
      exit(1);
      break;
  }
  (*(mostbptoken-1)).val.num = result;
  while (mostbptoken-tokens+2 < *tokensc) {
    *mostbptoken = *(mostbptoken+2);
    mostbptoken++;
  }
  *tokensc -= 2;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr,"Incorrect Usage! Usage: calc <equation>\n");
    exit(1);
  }

  char equation[EQLEN] = {'\0'};
  for (int i = 1; i < argc; i++) {
    strcat(equation,argv[i]);
  }
  
  Token tokens[MAXTOKENS]; int tokensc;
  char *eqptr = equation;

  double num1;
  int consumed;
  char symbol;
  int extrabp = 0;
  Tokentype lookingfor = NUMBER;
  for (tokensc = 0; strlen(eqptr) > 0; eqptr += consumed, tokensc++) {
    if (lookingfor == NUMBER) {
      int gbp;
      while ((gbp = getgbp(*eqptr)) != -1 && strlen(eqptr) > 0) {
        if (gbp < 0) {
          fprintf(stderr,"We only allow opening groupers at the start of a num. Char: %ld, grouper: %c\n", eqptr-equation,*eqptr);
          exit(1);
        }
        extrabp += gbp;
        if (extrabp < 0) {
          fprintf(stderr,"Extra closing grouper on char %ld, grouper: %c\n",eqptr-equation,*eqptr);
        }
        eqptr++;
      }

      int valid = sscanf(eqptr,"%lf%n",&num1,&consumed);
      if (valid != 1) {
        fprintf(stderr,"Parsing error on char %ld, char: %c",eqptr-equation,*eqptr);
        exit(1);
      }
      tokens[tokensc] = (Token) {.val.num = num1, .type = NUMBER};

      while ((gbp = getgbp(*(eqptr+consumed))) != -1 && strlen(eqptr+consumed) > 0) {
        if (gbp > 0) {
          fprintf(stderr,"We only allow closing groupers at the end of a num. Char: %ld, grouper: %c\n", eqptr-equation,*eqptr);
          exit(1);
        }
        extrabp += gbp;
        eqptr++;
      }
      lookingfor = OPERATOR;
    } else {
      int valid = sscanf(eqptr,"%c%n",&symbol,&consumed);
      if (valid != 1) {
        fprintf(stderr,"Parsing error near char %ld, char: %c",eqptr-equation,*eqptr);
        exit(1);
      }
      int bp = getbp(symbol);
      if (bp < 0) {
        fprintf(stderr,"Invalid operator near char %ld, char: %c, symbol: %c\n",eqptr-equation,*eqptr,symbol);
        exit(1);
      }
      tokens[tokensc] = (Token) {.val.op = (Operator) {symbol,bp+extrabp},.type = OPERATOR};
      lookingfor = NUMBER;
    }
  }

  if (extrabp > 0) {
    fprintf(stderr,"Unclosed groupers.\n");
    exit(1);
  }

  if (lookingfor != OPERATOR) {
    fprintf(stderr, "Invalid syntax! Must end with expecting an operator not a number.\n");
    exit(1);
  }
  
  #ifdef DEBUG
  printf("Valid equation!\n");
  for (int i = 0; i < tokensc; i++) {
    if (tokens[i].type == OPERATOR) {
      printf("Operator: %c\t Precedence: %d\n",tokens[i].val.op.symbol,tokens[i].val.op.bp);
    } else {
      printf("Number: %lf\n", tokens[i].val.num);
    }
  }
  #endif
  
  while (tokensc > 1) {
    eval(tokens,&tokensc);
  }
  
  double result = tokens[0].val.num;
  double intpart;
  if (modf(result,&intpart) < 0.01) {
    printf("%ld\n", (long) result);
  } else {
    printf("%.2lf\n",result);
  }
  return 0;
}
