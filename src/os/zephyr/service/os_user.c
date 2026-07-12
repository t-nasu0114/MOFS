#include <mofs_errno.h>
#include <mofs_port_user.h>

/* Zephyr stub: fixed root-like caller context. */

static mofs_user_ctx_t caller_user_ctx = {
    .uid              = 0,
    .gid              = 0,
    .pid              = 0,
    .supp_groups      = {0},
    .supp_group_count = 0,
    .valid            = MOFS_TRUE,
};

int mofs_set_caller_user(mofs_uid_t uid, mofs_gid_t gid, mofs_pid_t pid)
{
    caller_user_ctx.uid   = uid;
    caller_user_ctx.gid   = gid;
    caller_user_ctx.pid   = pid;
    caller_user_ctx.valid = MOFS_TRUE;
    return 0;
}

int mofs_set_caller_for_peer_process(mofs_uid_t uid, mofs_gid_t gid, mofs_pid_t pid)
{
    return mofs_set_caller_user(uid, gid, pid);
}

int mofs_set_caller_supp_groups(const mofs_gid_t *groups, mofs_size_t group_count)
{
    mofs_size_t i;

    if ((groups == NULL) && (group_count != 0U)) {
        return MOFS_EINVAL;
    }
    if (group_count > MOFS_SUPP_GROUP_MAX) {
        return MOFS_EINVAL;
    }

    caller_user_ctx.supp_group_count = group_count;
    for (i = 0; i < group_count; i++) {
        caller_user_ctx.supp_groups[i] = groups[i];
    }
    return 0;
}

int mofs_get_caller_user(mofs_user_ctx_t *user)
{
    if (user == NULL) {
        return MOFS_EINVAL;
    }
    *user = caller_user_ctx;
    return 0;
}

int mofs_is_caller_in_group(mofs_gid_t group_id, mofs_bool *is_member)
{
    mofs_size_t i;

    if (is_member == NULL) {
        return MOFS_EINVAL;
    }

    if (caller_user_ctx.gid == group_id) {
        *is_member = MOFS_TRUE;
        return 0;
    }

    for (i = 0; i < caller_user_ctx.supp_group_count; i++) {
        if (caller_user_ctx.supp_groups[i] == group_id) {
            *is_member = MOFS_TRUE;
            return 0;
        }
    }

    *is_member = MOFS_FALSE;
    return 0;
}

int mofs_clear_caller_user(void)
{
    caller_user_ctx.valid            = MOFS_FALSE;
    caller_user_ctx.supp_group_count = 0;
    return 0;
}
