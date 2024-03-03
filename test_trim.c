#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

char *trim_whitespaces(char *str){
    if(str == NULL) return NULL;

    char *start = str, *end = str + strlen(str) - 1;
    char *new_str;
    while(isspace((unsigned char)*start)) start++;
    while(end > start && isspace((unsigned char)*end)) end--;

    if(end == start) return strdup(""); // if the string is all spaces, return an empty string
    new_str = strndup(start, end - start + 1);
    if(new_str == NULL){
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(1);
    }

    return new_str;
}

void test_trim_whitespaces() {
    char test1[] = "   Hello, World!   ";
    char test2[] = "NoSpacesHere";
    char test3[] = "   LeadingSpaces";
    char test4[] = "TrailingSpaces   ";
    char test5[] = "   ";
    char test6[] = " ";
    char test7[] = "";

    char *result1 = trim_whitespaces(test1);
    char *result2 = trim_whitespaces(test2);
    char *result3 = trim_whitespaces(test3);
    char *result4 = trim_whitespaces(test4);
    char *result5 = trim_whitespaces(test5);
    char *result6 = trim_whitespaces(test6);
    char *result7 = trim_whitespaces(test7);

    assert(strcmp(result1, "Hello, World!") == 0);
    assert(strcmp(result2, "NoSpacesHere") == 0);
    assert(strcmp(result3, "LeadingSpaces") == 0);
    assert(strcmp(result4, "TrailingSpaces") == 0);
    assert(strcmp(result5, "") == 0);
    assert(strcmp(result6, "") == 0);
    assert(strcmp(result7, "") == 0);


    free(result1);
    free(result2);
    free(result3);
    free(result4);
    free(result5);
    free(result6);
    free(result7);
    printf("All tests passed!\n");
}

int main() {
    test_trim_whitespaces();
    return 0;
}