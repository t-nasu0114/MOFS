#ifndef __MOFS_PORT_USER__
#define __MOFS_PORT_USER__

#include <mofs_port_types.h>

/**
 * @file mofs_port_user.h
 * @brief Contract for the identity used by MOFS permission checks.
 */

/** Maximum number of supplementary groups retained for one caller. */
#define MOFS_SUPP_GROUP_MAX 64U

/**
 * @brief Snapshot of one caller's credentials.
 *
 * A platform may store this context per thread or as one fixed global identity.
 * `valid` distinguishes an initialized identity from an unavailable one.
 */
typedef struct mofs_user_ctx
{
    /** Effective user identifier used for owner permission checks. */
    mofs_uid_t  uid;
    /** Effective primary group identifier. */
    mofs_gid_t  gid;
    /** Platform process or task identifier, or zero when unsupported. */
    mofs_pid_t  pid;
    /** Supplementary groups used for group membership checks. */
    mofs_gid_t  supp_groups[MOFS_SUPP_GROUP_MAX];
    /** Number of valid entries in `supp_groups`. */
    mofs_size_t supp_group_count;
    /** `MOFS_TRUE` when this context contains usable credentials. */
    mofs_bool   valid;
} mofs_user_ctx_t;

/**
 * @brief Set the caller's primary credentials.
 *
 * Function behavior:
 * - Stores `uid`, `gid`, and `pid` in the platform caller context.
 * - Marks the resulting context valid.
 *
 * @param[in] uid Effective user identifier.
 * @param[in] gid Effective primary group identifier.
 * @param[in] pid Platform process or task identifier.
 * @return 0 on success.
 * @return A positive `MOFS_E*` value on failure.
 */
int mofs_set_caller_user(mofs_uid_t uid, mofs_gid_t gid, mofs_pid_t pid);

/**
 * @brief Set credentials for an external peer process or task.
 *
 * A platform that can inspect the peer should also populate supplementary
 * groups. Other platforms may implement this as `mofs_set_caller_user()`.
 *
 * @param[in] uid Effective user identifier reported for the peer.
 * @param[in] gid Effective primary group identifier reported for the peer.
 * @param[in] pid Platform process or task identifier reported for the peer.
 * @return 0 on success.
 * @return A positive `MOFS_E*` value on failure.
 */
int mofs_set_caller_for_peer_process(mofs_uid_t uid, mofs_gid_t gid, mofs_pid_t pid);

/**
 * @brief Retrieve a snapshot of the current caller's credentials.
 *
 * The implementation may return an explicitly stored context, query the
 * platform's current caller, or return a configured fixed identity.
 *
 * @param[out] user Destination for the credential snapshot.
 * @return 0 on success. Callers must still inspect `user->valid`.
 * @return MOFS_EINVAL when `user` is NULL.
 * @return Another positive `MOFS_E*` value on failure.
 */
int mofs_get_caller_user(mofs_user_ctx_t *user);

/**
 * @brief Test whether the current caller belongs to a group.
 *
 * Both the primary group and supplementary groups participate in the test.
 *
 * @param[in] group_id Group identifier to test.
 * @param[out] is_member Set to `MOFS_TRUE` for membership, otherwise
 *                       `MOFS_FALSE`.
 * @return 0 when the membership result was produced.
 * @return MOFS_EINVAL when `is_member` is NULL.
 * @return Another positive `MOFS_E*` value on failure.
 */
int mofs_is_caller_in_group(mofs_gid_t group_id, mofs_bool *is_member);

/**
 * @brief Invalidate explicitly stored caller credentials.
 *
 * A later `mofs_get_caller_user()` may obtain fresh platform credentials or
 * return an invalid/fixed context according to the platform policy.
 *
 * @return 0 on success.
 * @return A positive `MOFS_E*` value on failure.
 */
int mofs_clear_caller_user(void);

#endif /* __MOFS_PORT_USER__ */
