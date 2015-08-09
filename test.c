#include <stdio.h>





void baz(void)
{
    int x;
    
    /* Comment1 */
    /* Comment2 */
    bar();

    // This
    // Blah blah
    x = 123;
    // Foo

    // Bar
    x = 123 + 2;
    x += 200;
    x += 201;

    // Fooo
    // Bar
    // Baz
    x += 202;

    /* This */
    foo();
}



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

    foo();// Inline comment
    bar();/* Inline Comment */ foo(); /*What more comments?*/
    baz();
}

int main(void)
{
    foo(); /* Call foo */
    bar();
    return 0;
}
