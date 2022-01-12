#define _GNU_SOURCE 
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


int main() {
    puts("plz input two numbers");
    size_t bufsize = 100;
    char* buf = malloc(bufsize);
    getline(&buf, &bufsize, stdin);
    double f1 = atof(buf);
    getline(&buf, &bufsize, stdin);
    double f2 = atof(buf);
    printf("%.2f + %.2f = %.2f \n", f1, f2, f1+f2);
    free(buf);
    return 0;
}