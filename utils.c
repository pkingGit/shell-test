/*
    utils.c -- Various useful shell utility functions.

    Copyright (C) 2016 Lawrence Sebald
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>   // For bool data type
#include <unistd.h>    // For getuid()
#include <pwd.h>       // For getpwuid() and struct passwd
#include <sys/types.h> // For uid_t

#include "utils.h"

size_t count_spaces(const char *str) {
    size_t rv = 0;

    while(*str) {
        if(isspace(*str++))
            ++rv;
    }

    return rv;
}

void flush_input(FILE *fp) {
    int c;

    /* Read until we get an EOF character or a newline. */
    while(1) {
        c = fgetc(fp);

        if(c == '\n' || c == EOF)
            return;
    }
}

char *unescape(const char *str, FILE *errf) {
    size_t len = strlen(str), i;
    char *unesc, *rv;
    char cur;
    char quoted = 0;

    /* Allocate space for the new string. Since only potentially removing
       characters, it will be no larger than the original string... */
    if(!(rv = (char *)malloc(len + 1))) {
        fprintf(errf, "shell error: %s\n", strerror(errno));
        return NULL;
    }

    unesc = rv;

    /* Scan through the string... */
    while(*str) {
        cur = *str++;

        /* Is this the beginning of an escape sequence? */
        if(!quoted && cur == '\\') {
            cur = *str++;

            switch(cur) {
                case '\0':
                    fprintf(errf, "shell error: illegal escape sequence\n");
                    free(rv);
                    return NULL;

                case 'n':
                    *unesc++ = '\n';
                    continue;
                case 'a':
                    *unesc++ = '\a';
                    continue;
                case 'b':
                    *unesc++ = '\b';
                    continue;
                case 'r':
                    *unesc++ = '\r';
                    continue;
                case '\\':
                    *unesc++ = '\\';
                    continue;
                case 'f':
                    *unesc++ = '\f';
                    continue;
                case 'v':
                    *unesc++ = '\v';
                    continue;
                case '\'':
                    *unesc++ = '\'';
                    continue;
                case '"':
                    *unesc++ = '"';
                    continue;
                case '?':
                    *unesc++ = '?';
                    continue;
                case '*':
                    *unesc++ = '*';
                    continue;
                case '$':
                    *unesc++ = '$';
                    continue;
                case 't':
                    *unesc++ = '\t';
                    continue;
                case ' ':
                    *unesc++ = ' ';
                    continue;
                case '!':
                    *unesc++ = '!';
                    continue;

                /* Ugh... Octal. */
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                {
                    int tmp = (cur - '0') << 6;

                    cur = *str++;
                    if(cur < '0' || cur > '7') {
                        free(rv);
                        return NULL;
                    }

                    tmp |= (cur - '0') << 3;

                    cur = *str++;
                    if(cur < '0' || cur > '7') {
                        free(rv);
                        return NULL;
                    }

                    tmp |= (cur - '0');
                    *unesc++ = (char)tmp;
                    continue;
                }

                /* And, for more fun, hex! */
                case 'x':
                case 'X':
                {
                    int tmp;

                    cur = *str++;
                    if(cur >= '0' && cur <= '9')
                        tmp = (cur - '0') << 4;
                    else if(cur >= 'a' && cur <= 'f')
                        tmp = (cur - 'a' + 10) << 4;
                    else if(cur >= 'A' && cur <= 'F')
                        tmp = (cur - 'A' + 10) << 4;
                    else {
                        free(rv);
                        return NULL;
                    }

                    cur = *str++;
                    if(cur >= '0' && cur <= '9')
                        tmp |= (cur - '0');
                    else if(cur >= 'a' && cur <= 'f')
                        tmp |= (cur - 'a' + 10);
                    else if(cur >= 'A' && cur <= 'F')
                        tmp |= (cur - 'A' + 10);
                    else {
                        free(rv);
                        return NULL;
                    }

                    *unesc++ = (char)tmp;
                    continue;
                }

                default:
                    *unesc++ = cur;
            }

            continue;
        }
        /* Escape sequences in quoted strings are very limited... */
        else if(quoted && cur == '\\') {
            cur = *str++;

            if(!cur) {
                fprintf(errf, "shell error: invalid escape sequence\n");
                free(rv);
                return NULL;
            }

            if(cur != quoted)
                *unesc++ = '\\';
        }
        /* Is this the beginning of a quoted string? */
        else if(!quoted && (cur == '\'' || cur == '"')) {
            quoted = cur;
            continue;
        }
        else if(quoted && cur == quoted) {
            quoted = 0;
            continue;
        }

        /* If we get here, it's not part of an escape, so copy it directly. */
        *unesc++ = cur;
    }

    /* Did we terminate any quotes that we started? */
    if(quoted) {
        fprintf(errf, "shell error: unterminated quote\n");
        free(rv);
        return NULL;
    }

    *unesc = 0;

    return rv;
}

int first_unquoted_space(const char *str) {
    int pos = 0;
    const char *tmp = str;
    char quoted = 0, last = 0, cur = 0;

    while(*tmp) {
        last = cur;
        cur = *tmp;

        if(last != '\\') {
            if(!quoted && (cur == '\'' || cur == '"'))
                quoted = cur;
            else if(quoted && cur == quoted)
                quoted = 0;
            if(!quoted && isspace(cur))
                return pos;
        }

        ++pos;
        ++tmp;
    }

    return -1;
}

/**
 * Gets host name.
 * Caller is responsible for deallocating result.
 * @return host name.
 */
char* getHostName(){
    size_t len = 256;
    char* hostname = malloc(len);

    if (gethostname(hostname, len) == 0){
        return hostname;
    } else{
        perror("getHostName: failed to retrieve host name!");
        return NULL;
    }
}

/**
 * Gets user name.
 * Caller is responsible for deallocating result.
 * @return user name.
 */
char* getUserName(){
    uid_t user_id = getuid();
    struct passwd *pwd_entry = getpwuid(user_id);
    if (pwd_entry != NULL) {
        char *username = pwd_entry->pw_name;
        return username;
    } else {
        perror("getUserName: failed to retrieve user name!");
        return NULL;
    }
}

/**
 * Removes leading whitespace from string.
 * @pparam s	a string.
 */
void trimLeading(char* s){
        int i = 0;
	// Find first non-whitespace character
        while (isspace((unsigned char)s[i])) {
                i++;
        }
        if (i > 0) {
		// Shift string left
                memmove(s, s + i, strlen(s + i) + 1);
        }
}

/**
 * Removes trailing whitespace from string.
 * @param s	a string.
 */
void trimTrailing(char* s){
	int i = strlen(s) - 1;
	// Find last non-whitespace character
	while (i >= 0 && isspace((unsigned char)s[i])) {
		i--;
	}
	// Null terminate string
	s[i + 1] = '\0';
}

/**
 * Removes leading and trailing whitespace from string.
 * @param s	a string.
 */
void trim(char* s){
	trimLeading(s);
	trimTrailing(s);
}

/**
 * [Generated by Google AI]
 * Parses a string into an array of tokens, handling quoted strings.
 * @param str The string to parse. This string will be modified.
 * @param tokens An array of char* pointers to store the tokens.
 * @param max_tokens The maximum number of tokens to store.
 * @return The number of tokens found.
 */
int parse_quoted_tokens(char *str, char **tokens, int max_tokens) {
    int token_count = 0;
    bool in_quote = false;
    char *p = str;
    
    // Skip leading whitespace
    while (*p && (*p == ' ' || *p == '\t')) {
        p++;
    }

    if (*p) { // If there's more string, capture first token
        tokens[token_count++] = p;
    }
    while (*p && token_count < max_tokens) {
        if (*p == '"') {
            in_quote = !in_quote;
            p++; // Skip the quote character
        }
        
        if (!in_quote && (*p == ' ' || *p == '\t')) {
            // End of an unquoted token
            *p = '\0'; // Null-terminate the current token
            p++;
            
            // Find the start of the next token
            while (*p && (*p == ' ' || *p == '\t')) {
                p++;
            }
            if (*p) { // If there's more string, set up for the next token
                tokens[token_count++] = p;
            }
        } else {
            p++;
        }
    }
    
    // If the loop finished inside a quote, the last token is what's left
    if (in_quote) {
        // Find the closing quote and null-terminate
        char *end_quote = strchr(tokens[token_count-1], '"');
        if (end_quote) {
            *end_quote = '\0';
        }
    }
    
    return token_count;
}

/**
 * Read possibly quoted tokens from string, returning array of tokens
 * and number of tokens parsed. Modifies original string.
 * Caller is responsible for deallocating result which contains
 * pointers to locations in original (modified) string.
 * @param string	a string.
 * @param numTokens	pointer to number of tokens parsed.
 * @return array of tokens.
 */
char** getTokens(char* string, int* numTokens){
	const int TOKEN_LIMIT = 1000;	
	// Allocate spaces for tokens
	char** result = malloc(sizeof(char*) * TOKEN_LIMIT);

	// Parse tokens
	*numTokens = parse_quoted_tokens(string, result, TOKEN_LIMIT);
	result[*numTokens] = NULL;

	// Return array of tokens
	return result;
}

