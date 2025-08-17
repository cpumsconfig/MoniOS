#include "stdio.h"

void printf_info(char *tip){
    printf("[\x1b[94mINFO\x1b[97m] %s", tip,"\n");
}
void printf_error(char *tip){
    printf("[\x1b[91mERROR\x1b[97m] %s", tip,"\n");
}
void printf_OK(char *tip){
    printf("[\x1b[92mOK\x1b[97m] %s", tip,"\n");
}
