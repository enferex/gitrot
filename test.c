#include <stdio.h>





/* This is a comment liine */
void foo(void)
{
    printf("Testing foo!\n");
}

/* This
 * is
 * a
 * comment block
 */
void bar(void)
{
    /* Comment line */
    printf("This is bar!\n");

    /* Comment block
     * blah
     * blahhhhh
     */

    printf("Ahhh!\n");
}

int main(void)
{
    foo(); /* Call foo */
    bar();
    return 0;
}
