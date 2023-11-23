#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLOBAL_SET 0x23
#define LOCAL_GET  0x20
#define LOCAL_SET  0x21
#define I32_CONST  0x41
#define I32_SUB    0x6B

unsigned char IMEM[1024]; /* instruction memory */
unsigned int Stack[1024];
unsigned int Globals[256];
unsigned int Locals[256][256];
unsigned int StackPtr;
unsigned int PC, DEPTH;

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

  return;
}


void execute ( void )
{
  unsigned char inst;

  while(1) {

  inst = IMEM[PC++];

  switch ( inst ) {
  case GLOBAL_SET:
    printf ("global.get\n");
    global_get ( );
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
  case I32_SUB:
    printf ("i32.sub\n");
    i32_sub ( );
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

  execute ();

  fclose ( fp );
  exit ( 0 );
}