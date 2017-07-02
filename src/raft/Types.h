/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @author Willem Thiart himself@willemthiart.com
 */

#pragma once
#include <vector>
#include <chrono>
#include <functional>
#include <bmcl/Option.h>

enum class RaftError : int
{
    Any = -1,
    NotLeader = -2,
    OneVotiongChangeOnly = -3,
    Shutdown = -4,
    NodeUnknown,
};
#define RAFT_ERR_SHUTDOWN                    -4

enum class raft_request_vote
{
    GRANTED         = 1,
    NOT_GRANTED     = 0,
    UNKNOWN_NODE    = -1,
};

enum class raft_state_e
{
    RAFT_STATE_NONE,
    RAFT_STATE_FOLLOWER,
    RAFT_STATE_CANDIDATE,
    RAFT_STATE_LEADER
} ;

enum class raft_logtype_e{
    RAFT_LOGTYPE_NORMAL,
    RAFT_LOGTYPE_ADD_NONVOTING_NODE,
    RAFT_LOGTYPE_ADD_NODE,
    RAFT_LOGTYPE_DEMOTE_NODE,
    RAFT_LOGTYPE_REMOVE_NODE,
    RAFT_LOGTYPE_NUM,
};

enum class raft_node_status{
    RAFT_NODE_STATUS_DISCONNECTED,
    RAFT_NODE_STATUS_CONNECTED,
    RAFT_NODE_STATUS_CONNECTING,
    RAFT_NODE_STATUS_DISCONNECTING
};

/**
* Copyright (c) 2013, Willem-Hendrik Thiart
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*
* @file
* @author Willem Thiart himself@willemthiart.com
*/

enum class raft_node_id : int {};

struct raft_entry_data_t
{
    void *buf;
    unsigned int len;
};

/** Entry that is stored in the server's entry log. */
struct raft_entry_t
{
    /** the entry's term at the point it was created */
    unsigned int term;

    /** the entry's unique ID */
    unsigned int id;

    /** type of entry */
    raft_logtype_e type;

    raft_entry_data_t data;
};

/** Message sent from client to server.
 * The client sends this message to a server with the intention of having it
 * applied to the FSM. */
typedef raft_entry_t msg_entry_t;

/** Entry message response.
 * Indicates to client if entry was committed or not. */
struct msg_entry_response_t
{
    /** the entry's unique ID */
    unsigned int id;

    /** the entry's term */
    int term;

    /** the entry's index */
    std::size_t idx;
};

/** Vote request message.
 * Sent to nodes when a server wants to become leader.
 * This message could force a leader/candidate to become a follower. */
struct msg_requestvote_t
{
    /** currentTerm, to force other leader/candidate to step down */
    int term;

    /** candidate requesting vote */
    raft_node_id candidate_id;

    /** index of candidate's last log entry */
    std::size_t last_log_idx;

    /** term of candidate's last log entry */
    std::size_t last_log_term;
};

/** Vote request response message.
 * Indicates if node has accepted the server's vote request. */
struct msg_requestvote_response_t
{
    /** currentTerm, for candidate to update itself */
    int term;

    /** true means candidate received vote */
    raft_request_vote vote_granted;
};

/** Appendentries message.
 * This message is used to tell nodes if it's safe to apply entries to the FSM.
 * Can be sent without any entries as a keep alive message.
 * This message could force a leader/candidate to become a follower. */
struct msg_appendentries_t
{
    /** currentTerm, to force other leader/candidate to step down */
    int term;

    /** the index of the log just before the newest entry for the node who
     * receives this message */
    std::size_t prev_log_idx;

    /** the term of the log just before the newest entry for the node who
     * receives this message */
    int prev_log_term;

    /** the index of the entry that has been appended to the majority of the
     * cluster. Entries up to this index will be applied to the FSM */
    std::size_t leader_commit;

    /** number of entries within this message */
    std::size_t n_entries;

    /** array of entries within this message */
    const msg_entry_t* entries;
};

/** Appendentries response message.
 * Can be sent without any entries as a keep alive message.
 * This message could force a leader/candidate to become a follower. */
struct msg_appendentries_response_t
{
    /** currentTerm, to force other leader/candidate to step down */
    int term;

    /** true if follower contained entry matching prevLogidx and prevLogTerm */
    bool success;

    /* Non-Raft fields follow: */
    /* Having the following fields allows us to do less book keeping in
     * regards to full fledged RPC */

    /** This is the highest log IDX we've received and appended to our log */
    std::size_t current_idx;

    /** The first idx that we received within the appendentries message */
    std::size_t first_idx;
} ;

class Raft;
class RaftNode;

/** Callback for sending request vote messages.
 * @param[in] raft The Raft server making this callback
 * @param[in] node The node's ID that we are sending this message to
 * @param[in] msg The request vote message to be sent
 * @return 0 on success */
using func_send_requestvote_f = std::function<bmcl::Option<RaftError>(Raft* raft, const RaftNode& node, const msg_requestvote_t& msg)>;

/** Callback for sending append entries messages.
 * @param[in] raft The Raft server making this callback
 * @param[in] node The node's ID that we are sending this message to
 * @param[in] msg The appendentries message to be sent
 * @return 0 on success */
using func_send_appendentries_f = std::function<bmcl::Option<RaftError>(Raft* raft, const RaftNode& node, const msg_appendentries_t& msg)>;

/** Callback for detecting when non-voting nodes have obtained enough logs.
 * This triggers only when there are no pending configuration changes.
 * @param[in] raft The Raft server making this callback
 * @param[in] node The node
 * @return 0 does not want to be notified again; otherwise -1 */
using func_node_has_sufficient_logs_f = std::function<bool(Raft* raft, const RaftNode& node)>;

#ifndef HAVE_FUNC_LOG
#define HAVE_FUNC_LOG

/** Callback for providing debug logging information.
 * This callback is optional
 * @param[in] raft The Raft server making this callback
 * @param[in] node The node that is the subject of this log. Could be NULL.
 * @param[in] buf The buffer that was logged */
using func_log_f = std::function<void(const Raft* raft, const bmcl::Option<const RaftNode&> node, const char *buf)>;
#endif

/** Callback for saving who we voted for to disk.
 * For safety reasons this callback MUST flush the change to disk.
 * @param[in] raft The Raft server making this callback
 * @param[in] voted_for The node we voted for
 * @return 0 on success */
using func_persist_int_f = std::function<bmcl::Option<RaftError>(Raft* raft, int node)>;

/** Callback for saving log entry changes.
 *
 * This callback is used for:
 * <ul>
 *      <li>Adding entries to the log (ie. offer)</li>
 *      <li>Removing the first entry from the log (ie. polling)</li>
 *      <li>Removing the last entry from the log (ie. popping)</li>
 *      <li>Applying entries</li>
 * </ul>
 *
 * For safety reasons this callback MUST flush the change to disk.
 *
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] entry The entry that the event is happening to.
 *    For offering, polling, and popping, the user is allowed to change the
 *    memory pointed to in the raft_entry_data_t struct. This MUST be done if
 *    the memory is temporary.
 * @param[in] entry_idx The entries index in the log
 * @return 0 on success */
using func_logentry_event_f = std::function<int(const Raft* raft, const raft_entry_t& entry, int entry_idx)>;

struct raft_cbs_t
{
    /** Callback for sending request vote messages */
    func_send_requestvote_f send_requestvote;

    /** Callback for sending appendentries messages */
    func_send_appendentries_f send_appendentries;

    /** Callback for finite state machine application
     * Return 0 on success.
     * Return RAFT_ERR_SHUTDOWN if you want the server to shutdown. */
    func_logentry_event_f applylog;

    /** Callback for persisting vote data
     * For safety reasons this callback MUST flush the change to disk. */
    func_persist_int_f persist_vote;

    /** Callback for persisting term data
     * For safety reasons this callback MUST flush the change to disk. */
    func_persist_int_f persist_term;

    /** Callback for adding an entry to the log
     * For safety reasons this callback MUST flush the change to disk.
     * Return 0 on success.
     * Return RAFT_ERR_SHUTDOWN if you want the server to shutdown. */
    func_logentry_event_f log_offer;

    /** Callback for removing the oldest entry from the log
     * For safety reasons this callback MUST flush the change to disk.
     * @note If memory was malloc'd in log_offer then this should be the right
     *  time to free the memory. */
    func_logentry_event_f log_poll;

    /** Callback for removing the youngest entry from the log
     * For safety reasons this callback MUST flush the change to disk.
     * @note If memory was malloc'd in log_offer then this should be the right
     *  time to free the memory. */
    func_logentry_event_f log_pop;

    /** Callback for determining which node this configuration log entry
     * affects. This call only applies to configuration change log entries.
     * @return the node ID of the node */
    func_logentry_event_f log_get_node_id;

    /** Callback for detecting when a non-voting node has sufficient logs. */
    func_node_has_sufficient_logs_f node_has_sufficient_logs;

    /** Callback for catching debugging log messages
     * This callback is optional */
    func_log_f log;
};
