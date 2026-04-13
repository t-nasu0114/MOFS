#include <mofs_errno.h>
#include <mofs_mem.h>
#include <mofs_user.h>
#include <unistd.h>

static __thread mofs_user_ctx_t caller_user_ctx = {
    .uid             = 0,
    .gid             = 0,
    .pid             = 0,
    .supp_groups     = {0},
    .supp_group_count = 0,
    .valid           = false,
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

    groups = (gid_t *)mofs_malloc(sizeof(gid_t) * (size_t)group_count);
    if (groups == NULL) {
        return MOFS_ENOMEM;
    }

    group_count = getgroups(group_count, groups);
    if (group_count < 0) {
        mofs_free(groups);
        return get_errno();
    }

    user->supp_group_count = (size_t)group_count;
    if (user->supp_group_count > MOFS_SUPP_GROUP_MAX) {
        user->supp_group_count = MOFS_SUPP_GROUP_MAX;
    }

    for (size_t i = 0; i < user->supp_group_count; i++) {
        user->supp_groups[i] = groups[i];
    }
    mofs_free(groups);
    return 0;
}

int mofs_set_caller_user(uid_t uid, gid_t gid, pid_t pid)
{
    caller_user_ctx.uid   = uid;
    caller_user_ctx.gid   = gid;
    caller_user_ctx.pid   = pid;
    caller_user_ctx.supp_groups[0]  = gid;
    caller_user_ctx.supp_group_count = 1;
    caller_user_ctx.valid = true;

    return 0;
}

int mofs_set_caller_supp_groups(const gid_t *groups, size_t group_count)
{
    if ((group_count > 0U) && (groups == NULL)) {
        return MOFS_EINVAL;
    }

    caller_user_ctx.supp_group_count = group_count;
    if (caller_user_ctx.supp_group_count > MOFS_SUPP_GROUP_MAX) {
        caller_user_ctx.supp_group_count = MOFS_SUPP_GROUP_MAX;
    }

    for (size_t i = 0; i < caller_user_ctx.supp_group_count; i++) {
        caller_user_ctx.supp_groups[i] = groups[i];
    }

    if (caller_user_ctx.supp_group_count == 0U) {
        caller_user_ctx.supp_groups[0]  = caller_user_ctx.gid;
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

    user->uid   = geteuid();
    user->gid   = getegid();
    user->pid   = getpid();
    user->valid = true;
    return read_supp_groups_from_os(user);
}

int mofs_is_caller_in_group(gid_t group_id, bool *is_member)
{
    int             ret = 0;
    mofs_user_ctx_t user;

    if (is_member == NULL) {
        return MOFS_EINVAL;
    }

    *is_member = false;
    ret        = mofs_get_caller_user(&user);
    if (ret != 0) {
        return ret;
    }

    if (user.gid == group_id) {
        *is_member = true;
        return 0;
    }

    for (size_t i = 0; i < user.supp_group_count; i++) {
        if (user.supp_groups[i] == group_id) {
            *is_member = true;
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
    caller_user_ctx.valid            = false;
    return 0;
}
