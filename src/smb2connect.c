/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Yuichi Nakamura
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "smb2.h"
#include "libsmb2.h"

#include "config_file.h"

//****************************************************************************
// Smb2 connection functions
//****************************************************************************

struct smb2_context *connect_smb2(const char *share)
{
    struct smb2_context *smb2;
    if ((smb2 = smb2_init_context()) == NULL)
        return NULL;

    if (strlen(config.smb2_user))
        smb2_set_user(smb2, config.smb2_user);
    if (strlen(config.smb2_passwd))
        smb2_set_password(smb2, config.smb2_passwd);
    if (strlen(config.smb2_workgroup))
        smb2_set_workstation(smb2, config.smb2_workgroup);

    smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

    printf("SMB2 connection server:%s share:%s\n", config.smb2_server, share);

    if (smb2_connect_share(smb2, config.smb2_server, share, config.smb2_user) < 0) {
        printf("smb2_connect_share failed. %s\n", smb2_get_error(smb2));
        smb2_destroy_context(smb2);
        return NULL;
    }
    printf("SMB2 connection established.\n");

    return smb2;
}

void disconnect_smb2(struct smb2_context *smb2)
{
    smb2_disconnect_share(smb2);
    smb2_destroy_context(smb2);
}

//****************************************************************************
// Smb2 share connection functions
//****************************************************************************

//----------------------------------------------------------------------------
// Private data and functions
//----------------------------------------------------------------------------

static struct smb2share {
    struct smb2share *next;
    char *share;
    struct smb2_context *smb2;
    int refcnt;
} *smb2share;

static void disconnect_smb2_internal(struct smb2share **s)
{
    disconnect_smb2((*s)->smb2);
    free((char *)(*s)->share);
    struct smb2share *next = (*s)->next;
    free(*s);
    *s = next;
}

static int path2share(const char *path, const char **shpath)
{
    const char *p = strchr(path, '/');
    if (p == NULL) {
        *shpath = NULL;
        return 0;           // invalid path
    }

    for (*shpath = p + 1; **shpath == '/'; *shpath++)
        ;                   // skip leading slashes

    return p - path;        // share length
}

static struct smb2share **findshare(const char *share, int len)
{
    for (struct smb2share **s = &smb2share; *s != NULL; s = &(*s)->next) {
        if (strlen((*s)->share) == len && strncmp((*s)->share, share, len) == 0) {
            return s;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------------
// Public functions
//----------------------------------------------------------------------------

struct smb2_context *path2smb2(const char *path, const char **shpath)
{
    int len = path2share(path, shpath);
    if (len <= 0) {
        return NULL;            // invalid path
    }

    struct smb2share **s;
    if (s = findshare(path, len)) {
        return (*s)->smb2;      // found connected share
    }

    return NULL;                // no such share
}

struct smb2_context *connect_smb2_path(const char *path, const char **shpath)
{
    int len = path2share(path, shpath);
    if (len <= 0) {
        return NULL;            // invalid path
    }

    struct smb2share **s;
    if (s = findshare(path, len)) {
        (*s)->refcnt++;
        return (*s)->smb2;      // found connected share
    }

    struct smb2_context *smb2;
    struct smb2share *t;
    t = malloc(sizeof(struct smb2share));
    t->next = NULL;
    t->share = calloc(1, len + 1);
    memcpy(t->share, path, len);

    if ((smb2 = connect_smb2(t->share)) == NULL) {
        free(t->share);
        free(t);
        return NULL;            // connection failed
    }

    t->refcnt = 1;
    t->smb2 = smb2;
    t->next = smb2share;
    smb2share = t;

    return smb2;                // new connection
}

void disconnect_smb2_smb2(struct smb2_context *smb2)
{
    for (struct smb2share **s = &smb2share; *s != NULL; s = &(*s)->next) {
        if ((*s)->smb2 == smb2) {
            if (--(*s)->refcnt == 0) {
                disconnect_smb2_internal(s);
            }
            return;
        }
    }
}

void disconnect_smb2_path(const char *path)
{
    const char *shpath;
    int len = path2share(path, &shpath);
    if (len <= 0) {
        return;
    }

    struct smb2share **s;
    if (s = findshare(path, len)) {
        if (--(*s)->refcnt == 0) {
            disconnect_smb2_internal(s);
        }
    }
}

void disconnect_smb2_all(void)
{
    struct smb2share **s = &smb2share;
    while (*s != NULL) {
        disconnect_smb2_internal(s);
    }
}
