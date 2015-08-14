#include <stdio.h>





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
