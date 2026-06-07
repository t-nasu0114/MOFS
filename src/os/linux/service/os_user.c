#define _GNU_SOURCE
#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <mofs_port_mem.h>
#include <mofs_port_user.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static __thread mofs_user_ctx_t caller_user_ctx = {
    .uid              = 0,
    .gid              = 0,
    .pid              = 0,
    .supp_groups      = {0},
    .supp_group_count = 0,
    .valid            = MOFS_FALSE,
};

static int read_supp_groups_from_os(mofs_user_ctx_t *user)
{
    int    group_count = 0;
    gid_t *groups      = NULL;

    group_count = getgroups(0, NULL);
    if (group_count < 0) {
        return get_errno();
    }

    user->supp_group_count = 0;
    if (group_count == 0) {
        return 0;
    }

    groups = (gid_t *)mofs_malloc(sizeof(gid_t) * (mofs_size_t)group_count);
    if (groups == NULL) {
        return MOFS_ENOMEM;
    }

    group_count = getgroups(group_count, groups);
    if (group_count < 0) {
        mofs_free(groups);
        return get_errno();
    }

    user->supp_group_count = (mofs_size_t)group_count;
    if (user->supp_group_count > MOFS_SUPP_GROUP_MAX) {
        user->supp_group_count = MOFS_SUPP_GROUP_MAX;
    }

    for (mofs_size_t i = 0; i < user->supp_group_count; i++) {
        user->supp_groups[i] = (mofs_gid_t)groups[i];
    }
    mofs_free(groups);
    return 0;
}

int mofs_set_caller_user(mofs_uid_t uid, mofs_gid_t gid, mofs_pid_t pid)
{
    caller_user_ctx.uid                = uid;
    caller_user_ctx.gid                = gid;
    caller_user_ctx.pid                = pid;
    caller_user_ctx.supp_groups[0]     = gid;
    caller_user_ctx.supp_group_count   = 1;
    caller_user_ctx.valid              = MOFS_TRUE;

    return 0;
}

#if defined(__linux__)
static int parse_proc_status_groups(mofs_pid_t pid, mofs_gid_t *out, mofs_size_t max_out, mofs_size_t *out_count)
{
    char    path[64];
    FILE   *fp    = NULL;
    char   *line  = NULL;
    size_t  cap   = 0;
    ssize_t nread;

    if ((pid <= 0) || (out == NULL) || (out_count == NULL)) {
        return MOFS_EINVAL;
    }

    *out_count = 0;
    (void)snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    fp = fopen(path, "re");
    if (fp == NULL) {
        return get_errno();
    }

    while ((nread = getline(&line, &cap, fp)) != -1) {
        char *p       = NULL;
        char *saveptr = NULL;
        char *tok     = NULL;

        if (strncmp(line, "Groups:", 7) != 0) {
            continue;
        }
        p = line + 7;
        while ((*p == ' ') || (*p == '\t')) {
            p++;
        }
        if ((*p == '\n') || (*p == '\0')) {
            break;
        }
        for (tok = strtok_r(p, " \t\n", &saveptr); tok != NULL; tok = strtok_r(NULL, " \t\n", &saveptr)) {
            unsigned long v = strtoul(tok, NULL, 10);
            if (*out_count < max_out) {
                out[*out_count] = (mofs_gid_t)v;
                (*out_count)++;
            }
        }
        break;
    }

    free(line);
    (void)fclose(fp);
    return 0;
}
#endif /* defined(__linux__) */

int mofs_set_caller_for_peer_process(mofs_uid_t uid, mofs_gid_t gid, mofs_pid_t pid)
{
    int          ret;
    mofs_gid_t   groups[MOFS_SUPP_GROUP_MAX];
    mofs_size_t  n = 0;

    ret = mofs_set_caller_user(uid, gid, pid);
    if (ret != 0) {
        return ret;
    }
#if defined(__linux__)
    ret = parse_proc_status_groups(pid, groups, MOFS_SUPP_GROUP_MAX, &n);
    if ((ret == 0) && (n > 0U)) {
        (void)mofs_set_caller_supp_groups(groups, n);
    }
#endif
    return 0;
}

int mofs_set_caller_supp_groups(const mofs_gid_t *groups, mofs_size_t group_count)
{
    if ((group_count > 0U) && (groups == NULL)) {
        return MOFS_EINVAL;
    }

    caller_user_ctx.supp_group_count = group_count;
    if (caller_user_ctx.supp_group_count > MOFS_SUPP_GROUP_MAX) {
        caller_user_ctx.supp_group_count = MOFS_SUPP_GROUP_MAX;
    }

    for (mofs_size_t i = 0; i < caller_user_ctx.supp_group_count; i++) {
        caller_user_ctx.supp_groups[i] = groups[i];
    }

    if (caller_user_ctx.supp_group_count == 0U) {
        caller_user_ctx.supp_groups[0]   = caller_user_ctx.gid;
        caller_user_ctx.supp_group_count = 1U;
    }

    return 0;
}

int mofs_get_caller_user(mofs_user_ctx_t *user)
{
    if (user == NULL) {
        return MOFS_EINVAL;
    }

    if (caller_user_ctx.valid) {
        *user = caller_user_ctx;
        return 0;
    }

    user->uid   = (mofs_uid_t)geteuid();
    user->gid   = (mofs_gid_t)getegid();
    user->pid   = (mofs_pid_t)getpid();
    user->valid = MOFS_TRUE;
    return read_supp_groups_from_os(user);
}

int mofs_is_caller_in_group(mofs_gid_t group_id, mofs_bool *is_member)
{
    int             ret = 0;
    mofs_user_ctx_t user;

    if (is_member == NULL) {
        return MOFS_EINVAL;
    }

    *is_member = MOFS_FALSE;
    ret        = mofs_get_caller_user(&user);
    if (ret != 0) {
        return ret;
    }

    if (user.gid == group_id) {
        *is_member = MOFS_TRUE;
        return 0;
    }

    for (mofs_size_t i = 0; i < user.supp_group_count; i++) {
        if (user.supp_groups[i] == group_id) {
            *is_member = MOFS_TRUE;
            break;
        }
    }
    return 0;
}

int mofs_clear_caller_user(void)
{
    caller_user_ctx.uid              = 0;
    caller_user_ctx.gid              = 0;
    caller_user_ctx.pid              = 0;
    caller_user_ctx.supp_groups[0]   = 0;
    caller_user_ctx.supp_group_count = 0;
    caller_user_ctx.valid            = MOFS_FALSE;
    return 0;
}
