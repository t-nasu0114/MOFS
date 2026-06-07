#ifndef __MOFS_PORT_USER__
#define __MOFS_PORT_USER__

#include <mofs_port_types.h>

#define MOFS_SUPP_GROUP_MAX 64U

typedef struct mofs_user_ctx
{
    mofs_uid_t  uid;
    mofs_gid_t  gid;
    mofs_pid_t  pid;
    mofs_gid_t  supp_groups[MOFS_SUPP_GROUP_MAX];
    mofs_size_t supp_group_count;
    mofs_bool   valid;
} mofs_user_ctx_t;

int mofs_set_caller_user(mofs_uid_t uid, mofs_gid_t gid, mofs_pid_t pid);
int mofs_set_caller_for_peer_process(mofs_uid_t uid, mofs_gid_t gid, mofs_pid_t pid);
int mofs_set_caller_supp_groups(const mofs_gid_t *groups, mofs_size_t group_count);
int mofs_get_caller_user(mofs_user_ctx_t *user);
int mofs_is_caller_in_group(mofs_gid_t group_id, mofs_bool *is_member);
int mofs_clear_caller_user(void);

#endif /* __MOFS_PORT_USER__ */
