#ifndef __MOFS_USER__
#define __MOFS_USER__

#include <mofs_type.h>

#define MOFS_SUPP_GROUP_MAX 64U

typedef struct mofs_user_ctx
{
    uid_t  uid;
    gid_t  gid;
    pid_t  pid;
    gid_t  supp_groups[MOFS_SUPP_GROUP_MAX];
    size_t supp_group_count;
    bool   valid;
} mofs_user_ctx_t;

int mofs_set_caller_user(uid_t uid, gid_t gid, pid_t pid);
/* Bind per-request uid/gid/pid (e.g. FUSE). On Linux, also load supplementary groups from /proc/<pid>/status. */
int mofs_set_caller_for_peer_process(uid_t uid, gid_t gid, pid_t pid);
int mofs_set_caller_supp_groups(const gid_t *groups, size_t group_count);
int mofs_get_caller_user(mofs_user_ctx_t *user);
int mofs_is_caller_in_group(gid_t group_id, bool *is_member);
int mofs_clear_caller_user(void);

#endif /* __MOFS_USER__ */
