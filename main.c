#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Next: "loop" */

#define GLOBAL_GET 0x23
#define GLOBAL_SET 0x24
#define LOCAL_GET  0x20
#define LOCAL_SET  0x21
#define I32_CONST  0x41
#define I32_STORE  0x36
#define I32_LOAD   0x28
#define I32_SUB    0x6B
#define I32_EQ     0x46
#define I32_EQZ    0x45
#define I32_AND    0x71
#define CALL       0x10
#define BLOCK      0x02
#define BR_IF      0x0d

#define MAXDEPTH    256
#define MAXBLEVEL   100

#define MAXPC      4096

//typedef void (*FunctionPointer)();
//FunctionPointer Funcs[10];
unsigned int  TYPES[10][2]; /* 0:# of arguments, 1:# of return vals */
unsigned int  Funcs[10];
unsigned char IMEM[1024]; /* instruction memory */
unsigned char DMEM[1024]; /* data memory */
unsigned int Stack[1024];
unsigned int Globals[256]={1024}; /* g0 = 1024 */
unsigned int Locals[256][256];
unsigned int StackPtr;
unsigned int PC, DEPTH;

unsigned int ENDINFO[MAXPC];   /* ENDINFO[start] = end */
unsigned int BLEVEL[MAXDEPTH]; /* BLEVEL[DEPTH] = 0->1->2->3 */
unsigned int BINFO[MAXDEPTH][MAXBLEVEL][3]; /* [0]:start, [1]:end, [2]:block(0) or loop(1) */

void global_set ( void )
{
  unsigned int gnum;

  gnum = IMEM[PC++];
  Globals[gnum] = Stack[StackPtr--];

  return;
}

void global_get ( void )
{
  unsigned int gnum;

  gnum = IMEM[PC++];
  Stack[++StackPtr] = Globals[gnum];

  return;
}

void local_set ( void )
{
  unsigned int lnum;

  lnum = IMEM[PC++];
  Locals[DEPTH][lnum] = Stack[StackPtr--];

  return;
}

void local_get ( void )
{
  unsigned int lnum;

  lnum = IMEM[PC++];
  Stack[++StackPtr] = Locals[DEPTH][lnum];

  return;
}

unsigned int leb128 ( void )
{
  unsigned int byte;
  unsigned int ival = 0;

  byte = IMEM[PC++];

  if ( (byte & 0x80) == 0 )
    return ival = byte;
  
  byte = IMEM[PC++];
  ival = (byte<<7) | ival;
  if ( (byte & 0x4000) == 0 )
    return ival;

  byte = IMEM[PC++];
  ival = (byte<<14) | ival;
  if ( (byte & 0x200000) == 0 )
    return ival;

  byte = IMEM[PC++];
  ival = (byte<<21) | ival;
  if ( (byte & 0x10000000) == 0 )
    return ival;

  printf ("Error in leb128\n");
  exit ( 0 );
}

void i32_const ( void )
{
  unsigned int ival;

  ival = leb128();
  Stack[++StackPtr] = ival;

  return;
}

void i32_sub ( void )
{
  int x, y;

  x = Stack[StackPtr--];
  y = Stack[StackPtr--];
  Stack[++StackPtr] = y - x;

#if 1
  printf ( "sub = %d\n", Stack[StackPtr] );
#endif

  return;
}

void i32_and ( void )
{
  int x, y;

  x = Stack[StackPtr--];
  y = Stack[StackPtr--];
  Stack[++StackPtr] = y & x;

  return;
}

void i32_eq ( void )
{
  int x, y;

  x = Stack[StackPtr--];
  y = Stack[StackPtr--];

  if ( x == y )
    Stack[++StackPtr] = 1;
  else
    Stack[++StackPtr] = 0;

  return;
}

void i32_eqz ( void )
{
  int x;

  x = Stack[StackPtr--];

  if ( x == 0 )
    Stack[++StackPtr] = 1;
  else
    Stack[++StackPtr] = 0;

  return;
}

void i32_load ( void )
{
  unsigned int ea, i, offset, align;

  align  = IMEM[PC++];
  offset = IMEM[PC++]; /* Caution: can be leb128 */

  i = Stack[StackPtr--];
  ea = i + offset;

  Stack[++StackPtr] = ((unsigned int)DMEM[ea+3] << 24) | ((unsigned int)DMEM[ea+2] << 16) | ((unsigned int)DMEM[ea+1] << 8) | (unsigned int)DMEM[ea];

#if 1
  printf ( "Stack[%d](<=DMEM[%d]): \%8X\n", StackPtr, ea, Stack[StackPtr] );
#endif

  return;
}


void i32_store ( void )
{
  unsigned int ea, c, i, offset, align;

  align  = IMEM[PC++];
  offset = IMEM[PC++]; /* Caution: can be leb128 */

  c = Stack[StackPtr--];
  i = Stack[StackPtr--];
  ea = i + offset;

  DMEM[ea]   = (unsigned char)((c >> 0)  & 0x000000FF);
  DMEM[ea+1] = (unsigned char)((c >> 8)  & 0xFF);
  DMEM[ea+2] = (unsigned char)((c >> 16) & 0xFF);
  DMEM[ea+3] = (unsigned char)((c >> 24) & 0xFF);

#if 1
  printf ( "MEM[%d]: \%2X, %2X, %2X, %2X\n", ea, DMEM[ea+3],DMEM[ea+2],DMEM[ea+1],DMEM[ea] );
#endif

  return;
}

void register_info ( void )
{
  Funcs[1] = 0;
  TYPES[1][0] = 1;
  TYPES[1][1] = 1;

  Funcs[2] = 303;
  TYPES[2][0] = 1;
  TYPES[2][1] = 1;

  Funcs[3] = 443;
  TYPES[3][0] = 0;
  TYPES[3][1] = 1;

  ENDINFO[33] = 51;
  ENDINFO[90] = 109;
  ENDINFO[146] = 275;
  ENDINFO[144] = 277;
  ENDINFO[31] = 292;  /* inst at PC=29 */
  ENDINFO[340] = 430;
  ENDINFO[338] = 432;
} 

void call ( void )
{
  unsigned int funcidx;
  int i;

  /* TODO: record the return address */
  DEPTH += 1;

  BLEVEL[DEPTH] = 0; /* unnecessary? */

  funcidx = IMEM[PC++];

  for ( i=0; i<TYPES[funcidx][0]; i++ ) {
    Locals[DEPTH][i] = Stack[StackPtr--];
  }

  PC = Funcs[funcidx];

  printf ( "call func %d\n", funcidx );

  return;
}

void block ( void )
{
  unsigned char blocktype;

  blocktype = IMEM[PC++];

  BLEVEL[DEPTH] += 1; /* 0->1->2->3 */
  BINFO[DEPTH][BLEVEL[DEPTH]][0] = PC; /* [0]:start */
  BINFO[DEPTH][BLEVEL[DEPTH]][1] = ENDINFO[PC]; /* [1]:end */
  BINFO[DEPTH][BLEVEL[DEPTH]][2] = 0; /* [2]:block(0) */

  return;
}

void br_if ( void )
{
  unsigned int c, l;

  l = IMEM[PC++];
  c = Stack[StackPtr--];

  if ( c != 0 ) {
    if ( BINFO[DEPTH][BLEVEL[DEPTH] - l][2] == 0 /* block */ )
      PC = BINFO[DEPTH][BLEVEL[DEPTH] - l][1];
    else /* loop */
      PC = BINFO[DEPTH][BLEVEL[DEPTH] - l][0];

    BLEVEL[DEPTH] -= (l+1);
  }

  return;
}

void execute ( void )
{
  unsigned char inst;

  while(1) {

    printf ( "PC=%3d\t", PC );

    inst = IMEM[PC++];

    switch ( inst ) {
    case GLOBAL_GET:
      printf ("global.get\n");
      global_get ( );
      break;
    case GLOBAL_SET:
      printf ("global.set\n");
      global_set ( );
      break;
    case LOCAL_GET:
      printf ("local.get\n");
      local_get ( );
      break;
    case LOCAL_SET:
      printf ("local.set\n");
      local_set ( );
      break;
    case I32_CONST:
      printf ("i32.const\n");
      i32_const ( );
      break;
    case I32_STORE:
      printf ("i32.store\n");
      i32_store ( );
      break;
    case I32_LOAD:
      printf ("i32.load\n");
      i32_load ( );
      break;
    case I32_SUB:
      printf ("i32.sub\n");
      i32_sub ( );
      break;
    case I32_AND:
      printf ("i32.and\n");
      i32_and ( );
      break;
    case CALL:
      printf ("call\n");
      call ( );
      break;
    case BLOCK:
      printf ("block\n");
      block ( );
      break;
    case BR_IF:
      printf ("br_if\n");
      br_if ( );
      break;
    case I32_EQ:
      printf ("i32.eq\n");
      i32_eq ( );
      break;
    case I32_EQZ:
      printf ("i32.eqz\n");
      i32_eqz ( );
      break;
    default:
      printf ( "Undefined Instruction: 0x%2x\n", inst );
      exit (0);
    }
  }

  return;
}

int main ( int argc, char *argv[] )
{
  FILE *fp;
  char buf[512];
  unsigned int c;
  int i;

  if ( argc != 2 ) { /* 実行時の入力check */
    printf ( "Usage: wsim [filename]\n" );
    exit ( 0 );
  }

  if ( (fp = fopen ( argv[1], "r" )) == NULL ) {
    printf ( "\"%s\" does not exist.\n", argv[1] );
    exit ( 0 );
  }

  for ( i=0; fgets ( buf, 512, fp ) != NULL; i++ ) {
    while ( buf[0] == '#' )
      fgets ( buf, 512, fp );

    buf[2] = 0;
    sscanf ( buf, "%x", &c );
    IMEM[i] = (unsigned char)c;

  }

  /* func 1(size:306-3=303):   0 - 302 */
  /* func 2(size:143-3=140): 303 - 442 */
  /* func 3(size:110-3=107): 443 - 549 */

  register_info ( );

  PC = Funcs[3];

  execute ();

  fclose ( fp );
  exit ( 0 );
}