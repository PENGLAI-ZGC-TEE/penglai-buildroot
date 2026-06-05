#include "eapp.h"
#include "print.h"

int prime_loop(int num)
{
  unsigned long count = 0;
  int i;
  for(i = 2; i < num; i++)
  {
    if (num % i ==0)
      count++;
  }
  return count;
}

int prime(unsigned long * args)
{
  eapp_print("%s is running\n", "Prime");
  unsigned long ret = prime_loop(111);
  EAPP_RETURN(ret);
}

int EAPP_ENTRY main(){
  unsigned long * args;
  EAPP_RESERVE_REG;
  prime(args);
}
