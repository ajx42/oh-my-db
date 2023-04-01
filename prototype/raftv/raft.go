package raft

import (
  "fmt"
  "log"
  "math/rand"
  "sync"
  "time"
)

type LogEntry struct {
  Command interface{}
  Term    int
}

// this data will be reported to commit channel
// which is eventually used to reply to the user
// maybe we also need a call identifier to be able
// to respond to the right client
type CommitEntry struct {
  Command interface{}
  Index   int
  Term    int
}

type NodeState int

// in go this syntax defines constants, iota is to used to specify
// incrementing constants
// the following servers like a C++ enum
const (
  Follower NodeState = iota
  Candidate
  Leader
  Unknown
)

// some conversion mechanism
// stuff written before the method name is the "receiver"
// the method can be invoked on 
func (s NodeState) String() string {
  switch s {
  case Follower: 
    return "Follower"
  case Candidate:
    return "Candidate"
  case Leader:
    return "Leader"
  case Unknown:
    return "Unknown"
  default:
    panic( "unreachable" )
  }
}

type ConsensusModule struct {
  // Implementation specific state
  mu                  sync.Mutex
  id                  int
  peerIds             []int
  server              *Server
  commitChan          chan<- CommitEntry
  newCommitReadyChan  chan struct{}

  // Actually currently no state is persistent
  // Persistent RAFT State on all servers
  currentTerm         int
  votedFor            int
  log                 []LogEntry

  // Volatile
  state               NodeState
  electionResetEvent  time.Time
  commitIndex         int // entries that are okay to apply
  lastApplied         int // entries that have been applied

  // for leaders only
  nextIndex           map[int]int
  matchIndex          map[int]int
}

func NewConsensusModule(
  id int, peerIds []int, server *Server,
  ready <-chan interface{},
  commitChan chan<- CommitEntry) *ConsensusModule {
  cm := new(ConsensusModule)
  cm.id = id
  cm.peerIds = peerIds
  cm.commitChan = commitChan
  cm.newCommitReadyChan = make(chan struct{}, 16)
  cm.server = server
  cm.state = Follower // always start as a follower
  cm.votedFor = -1
  cm.commitIndex = -1
  cm.lastApplied = -1
  cm.nextIndex = make(map[int]int)
  cm.matchIndex = make(map[int]int)

  go func() {
    <-ready
    cm.mu.Lock()
    cm.electionResetEvent = time.Now()
    cm.mu.Unlock()
    cm.runElectionTimer()
  }()

  go cm.commitChanSender()
  return cm
}

func (cm *ConsensusModule) Stop() {
  cm.mu.Lock()
  defer cm.mu.Unlock()
  cm.state = Unknown
  cm.dlog("GAME OVER")
}

func (cm ConsensusModule) dlog( format string, args ...interface{} ) {
  // always print debug logs for now
  format = fmt.Sprintf( "[%d] ", cm.id ) + format
  log.Printf(format, args...)
}

func (cm *ConsensusModule) commitChanSender() {
  // we will keep iterating until channel close
  // this conveniently blocks when nothing is ready
  for range cm.newCommitReadyChan {
    cm.mu.Lock()
    savedTerm := cm.currentTerm
    savedLastApplied := cm.lastApplied
    var entries []LogEntry
    if cm.commitIndex > cm.lastApplied {
      // we have entries that can be applied and responded to
      entries = cm.log[cm.lastApplied+1 : cm.commitIndex+1]
      cm.lastApplied = cm.commitIndex
    }
    cm.mu.Unlock()
    cm.dlog("CommitChannelSender -> entries=%v, savedLastApplied=%d",
            entries, savedLastApplied )
    
    for i, entry := range entries {
      toCommit := CommitEntry {
        Command:  entry.Command,
        Index:    savedLastApplied + i + 1,
        Term:     savedTerm,
      }
      cm.dlog("CommitChannelSender -> (new commit) entry=%v", toCommit)
      cm.commitChan <-toCommit
    }
  }
}

func (cm *ConsensusModule) Submit(command interface{}) bool {
  cm.mu.Lock()
  defer cm.mu.Unlock()
  cm.dlog("Submit Received -> state=%v, command=%v", cm.state, command)
  if cm.state == Leader {
    // if we are the leader, we will process otherwise ignore
    cm.log = append(cm.log, LogEntry{ Command: command, Term: cm.currentTerm })
    cm.dlog("... log=%v", cm.log)
    return true
  }
  return false
}

type RequestVoteArgs struct {
  Term          int
  CandidateId   int
  LastLogIndex  int
  LastLogTerm   int
}

type RequestVoteReply struct {
  Term          int
  VoteGranted   bool
}

func (cm *ConsensusModule) RequestVote( 
  args RequestVoteArgs, reply *RequestVoteReply) error {
  cm.mu.Lock()
  defer cm.mu.Unlock()
  if cm.state == Unknown {
    return nil
  }

  lastLogIndex, lastLogTerm := -1, -1
  if len(cm.log) > 0 {
    lastLogIndex = len(cm.log) - 1
    lastLogTerm = cm.log[lastLogIndex].Term
  }

  cm.dlog("RequestVote: %+v [currentTerm=%d, votedFor=%d, lastLogIdx=%d, lastLogTerm=%d]", args, cm.currentTerm, cm.votedFor, lastLogIndex, lastLogTerm)

  if args.Term > cm.currentTerm {
    cm.dlog("... term out of date in RequestVote")
    cm.becomeFollower(args.Term)
  }

  if cm.currentTerm == args.Term && (cm.votedFor == -1 || cm.votedFor == args.CandidateId) &&
    (args.LastLogTerm > lastLogTerm || 
        (args.LastLogTerm == lastLogTerm && args.LastLogIndex >= lastLogIndex ) ) {
    reply.VoteGranted = true
    cm.votedFor = args.CandidateId
    cm.electionResetEvent = time.Now()
  } else {
    reply.VoteGranted = false
  }
  reply.Term = cm.currentTerm
	cm.dlog("... RequestVote reply: %+v", reply)
	return nil  
}

type AppendEntriesArgs struct {
  Term          int
  LeaderId      int
  PrevLogIndex  int
  PrevLogTerm   int
  Entries       []LogEntry
  LeaderCommit  int
}

type AppendEntriesReply struct {
  Term          int
  Success       bool
}

func (cm *ConsensusModule) AppendEntries(
  args AppendEntriesArgs, reply *AppendEntriesReply ) error {
  cm.dlog("AppendEntries args=[%+v]", args)
  cm.mu.Lock()
  defer cm.mu.Unlock()
  if cm.state == Unknown {
    return nil
  }
  
  if args.Term > cm.currentTerm {
    cm.dlog("... term out of date in AppendEntries")
    cm.becomeFollower( args.Term )
  }

  reply.Success = false
  if args.Term == cm.currentTerm {
    if cm.state != Follower {
      cm.becomeFollower( args.Term )
    }
    cm.electionResetEvent = time.Now()
    
    // make sure prev entry matches, this ensures that the prefix is good and in sync
    if args.PrevLogIndex == -1 ||
      ( args.PrevLogIndex < len( cm.log ) && args.PrevLogTerm == cm.log[args.PrevLogIndex].Term ) {
      reply.Success = true // prev entry matches so we are good to populate this one
      logInsertIndex := args.PrevLogIndex + 1
      newEntriesIndex := 0
      for {
        if logInsertIndex >= len(cm.log) || newEntriesIndex >= len(args.Entries) {
          break
        }
        if cm.log[logInsertIndex].Term != args.Entries[newEntriesIndex].Term {
          break
        }
        logInsertIndex++
        newEntriesIndex++
      }
      if newEntriesIndex < len(args.Entries) {
        cm.dlog("... inserting entries %v from index %d", args.Entries[newEntriesIndex:])
        cm.log = append(cm.log[:logInsertIndex], args.Entries[newEntriesIndex:]...)
      }
    }
  }
  reply.Term = cm.currentTerm
  cm.dlog("AppendEntries reply: %+v", *reply)
  return nil
}


func (cm ConsensusModule) electionTimeout() time.Duration {
  return time.Duration( 150 + rand.Intn( 150 ) ) * time.Millisecond
}

func (cm *ConsensusModule) runElectionTimer() {
  timeoutDuration := cm.electionTimeout()
  cm.mu.Lock()
  termStarted := cm.currentTerm
  cm.mu.Unlock()
  cm.dlog("election timer started (%v), term=%d", timeoutDuration, termStarted)

  // ticker ticks once in every 10 milliseconds
  ticker := time.NewTicker( 10 * time.Millisecond )
  defer ticker.Stop() // defer stop invocation till we exit this method

  for {
    <-ticker.C
    cm.mu.Lock()
    if cm.state != Candidate && cm.state != Follower {
      // either we are already a leader or we have messed up, we need to get out
      cm.mu.Unlock()
      return
    }
    if termStarted != cm.currentTerm {
      // this may mean we already have a leader with a higher term
      cm.mu.Unlock()
      return
    }
    if elapsed := time.Since(cm.electionResetEvent); elapsed >= timeoutDuration {
      cm.startElection()
      cm.mu.Unlock()
      return
    }
    cm.mu.Unlock()
  }
}

func (cm *ConsensusModule) startElection() {
  // we have started the election, transition to Candidate
  // send requestVote RPCs to all peers, asking them to vote for us in this election
  // wait for replies
  // note that if we are here, the CM state is already locked (we are good to assume
  // that we are the sole users)
  cm.state = Candidate
  cm.currentTerm += 1
  savedCurrentTerm := cm.currentTerm
  cm.electionResetEvent = time.Now()
  cm.votedFor = cm.id // vote for self
  cm.dlog("now Candidate (currentTerm=%d); log=%v", savedCurrentTerm, cm.log)

  votesReceived := 1 // self vote
  for _, peerId := range cm.peerIds {
    go func( peerId int ) {
      // election safety related
      cm.mu.Lock()
      sendLastLogIndex, sendLastLogTerm := -1, -1
      if len(cm.log) > 0 {
        sendLastLogIndex = len(cm.log) - 1
        sendLastLogTerm = cm.log[sendLastLogIndex].Term
      }
      cm.mu.Unlock()

      // concurrently send out i
      args := RequestVoteArgs{
        Term: savedCurrentTerm,
        CandidateId: cm.id,
        LastLogIndex: sendLastLogIndex,
        LastLogTerm: sendLastLogTerm,
      }

      var reply RequestVoteReply

      cm.dlog( "sending requestVote to peerid=%d", peerId )
      if err := cm.server.Call( peerId, "ConsensusModule.RequestVote", args, &reply ); err == nil {
        cm.mu.Lock()
        defer cm.mu.Unlock()
        cm.dlog( "received RequestVoteReply %+v", reply )

        if cm.state != Candidate {
          cm.dlog( "while waiting for reply, state=%v", cm.state )
          return // we got demoted back to follower -> looks like someone else took over
                 // or we have already won the election -> :D 
        }
        
        // looks like our term is outdated, turn into a follower again
        if reply.Term > savedCurrentTerm {
          cm.dlog("term out of date in RequestVoteReply")
          cm.becomeFollower( reply.Term )
          return
        } else if reply.Term == savedCurrentTerm {
          if reply.VoteGranted {
            votesReceived += 1
            // check if we have won the election
            if votesReceived * 2 > len( cm.peerIds ) + 1 {
              cm.dlog( "wins election with %d votes", votesReceived )
              cm.startLeader()
              return
            }
          }
        }
      }
    }( peerId )
  }

  // launch another election timer in case this election falls apart
  go cm.runElectionTimer()
}

func (cm *ConsensusModule) becomeFollower( term int ) {
  cm.dlog( "now a Follower with term=%d, log=%d", term, cm.log )
  cm.state = Follower
  cm.currentTerm = term
  cm.votedFor = -1
  cm.electionResetEvent = time.Now()
  go cm.runElectionTimer()
}

func (cm *ConsensusModule) startLeader() {
  cm.state = Leader
  cm.dlog("now the Leader with term=%d, log=%v", cm.currentTerm, cm.log)
  go func() {
    ticker := time.NewTicker( 50 * time.Millisecond )
    defer ticker.Stop()
    for {
      cm.leaderSendHeartbeats()
      <-ticker.C
      cm.mu.Lock()
      if cm.state != Leader {
        cm.mu.Unlock()
        return
      }
      cm.mu.Unlock()
    }
  }()
}

// send heartbeats, also replicate the log
func (cm *ConsensusModule) leaderSendHeartbeats() {
  cm.mu.Lock()
  if cm.state != Leader {
    cm.mu.Unlock()
    return
  }
  savedCurrentTerm := cm.currentTerm
  cm.mu.Unlock()
  for _, peerId := range cm.peerIds {
    go func( peerId int ) {
      cm.mu.Lock()
      ni := cm.nextIndex[peerId]
      prevLogIndex := ni-1
      prevLogTerm := -1
      if prevLogIndex >= 0 {
        prevLogTerm = cm.log[prevLogIndex].Term
      }
      entries := cm.log[ni:]
      args := AppendEntriesArgs{
        Term:         savedCurrentTerm,
        LeaderId:     cm.id,
        PrevLogIndex: prevLogIndex,
        PrevLogTerm:  prevLogTerm,
        Entries:      entries,
        LeaderCommit: cm.commitIndex,
      }
      cm.mu.Unlock()
      cm.dlog("sending append entries to %v: ni=%d, args=%+v", peerId, ni, args)
      var reply AppendEntriesReply
      err := cm.server.Call( peerId, "ConsensusModule.AppendEntries", args, &reply );
      if  err == nil {
        cm.mu.Lock()
        defer cm.mu.Unlock()
        if reply.Term > savedCurrentTerm {
          cm.dlog("term out of date in heartbeat reply")
          cm.becomeFollower(reply.Term)
          return
        }
        if cm.state == Leader && savedCurrentTerm == reply.Term {
          // correct execution path
          if reply.Success {
            cm.nextIndex[peerId] = ni + len(entries)
            cm.matchIndex[peerId] = cm.nextIndex[peerId]-1
            cm.dlog("AppendEntries reply from %d success: nextIndex=%v, matchIndex=%v",
                    peerId, cm.nextIndex[peerId], cm.matchIndex[peerId])
            savedCommitIndex := cm.commitIndex
            for i := cm.commitIndex + 1; i < len(cm.log); i++ {
              // check if we can commit any more
              if cm.log[i].Term == cm.currentTerm {
                matchCount := 1
                for _, peerId := range cm.peerIds {
                  if cm.matchIndex[peerId] >= i {
                    matchCount++
                  }
                }
                if matchCount*2 > len(cm.peerIds) + 1 {
                  // if majority peers have this entry, then it can be committed
                  cm.commitIndex = i
                }
              }
            }
            if cm.commitIndex != savedCommitIndex {
              // this means we have more entries to commit
              cm.dlog("leader sets commitIndex=%d", cm.commitIndex)
              cm.newCommitReadyChan <- struct{}{}
            }
          } else {
            // if the reply wasn't successful, go back one more step
            cm.nextIndex[peerId] = ni - 1
            cm.dlog("AppendEntries reply from %d !success: nextIndex=%d", peerId, ni-1)
          }
        }
      } else {
        cm.dlog("failed to call %v", err)
      }
    }( peerId )
  }
}

