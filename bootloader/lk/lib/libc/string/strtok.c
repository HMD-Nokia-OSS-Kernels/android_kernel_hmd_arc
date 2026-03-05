/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
/*
 * Copyright (c) 2008 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <string.h>
#include <sys/types.h>

static char *___strtok = NULL;

char *
strtok_r(char *s, const char *ct, char **saved_ptr) {
    char *sbegin, *send;

    sbegin  = s ? s : *saved_ptr;
    if (!sbegin) {
        return NULL;
    }
    sbegin += strspn(sbegin,ct);
    if (*sbegin == '\0') {
        *saved_ptr = NULL;
        return ( NULL );
    }
    send = strpbrk( sbegin, ct);
    if (send && *send != '\0')
        *send++ = '\0';
    *saved_ptr = send;
    return (sbegin);
}

char*
strtok(char *s, char const *ct) {
   return strtok_r(s, ct, &___strtok);
}
