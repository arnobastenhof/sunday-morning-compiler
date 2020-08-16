#include <stdio.h>

int
main()
{
  FILE *fp;

  if ((fp = fopen("tmp.s", "w")) == NULL) {
    perror("tmp.s");
  } else {
    fprintf(fp, "BITS 64\n"
      "GLOBAL _start\n"
      "SECTION .text\n"
      "_start:\n"
      "  mov  eax, 1\n"
      "  mov  ebx, 0\n"
      "  int  0x80\n");
    fclose(fp);
  }

  return 0;
}
