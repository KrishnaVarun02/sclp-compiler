#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void sclp_print(int64_t value) { printf("%" PRId64 "\n",value); }
static void validate_division(int64_t a,int64_t b) {
  if(b==0) { fputs("sclp runtime error: division by zero\n",stderr); exit(2); }
  if(a==INT64_MIN && b==-1) { fputs("sclp runtime error: signed division overflow\n",stderr); exit(2); }
}
int64_t sclp_div(int64_t a,int64_t b) { validate_division(a,b); return a/b; }
int64_t sclp_mod(int64_t a,int64_t b) { validate_division(a,b); return a%b; }
