// gcc -fPIC -shared -Wl,-soname,libabi_library.so -o libabi_library.so abi_library.c
#include <stdio.h>
void test(char *ab)
{
    printf("%s", ab);
}
