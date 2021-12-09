#include "stdio.h"

int main() {
    int userInfo = 0;
    printf("Hello world! \n");
    printf("Current val of var = %d \n", userInfo);

    scanf("%d", &userInfo);
    printf("Current val of var = %d \n", userInfo);

    userInfo++;
    userInfo*=2;
    printf("Current val of var = %d \n", userInfo);
}