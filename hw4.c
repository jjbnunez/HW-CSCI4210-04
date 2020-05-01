#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

int main(int argc, char** argv)
{
    //Ensure no buffered output for stdout and stderr.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    //Terminate.
    return 0;
}