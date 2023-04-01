package raft

import (
  "fmt"
  "log"
  "net"
  "net/rpc"
  "sync"
)

type Server struct {
  mu sync.Mutex

  serverId int
  peerIds []int
  
  cm *ConsensusModule
  rpcProxy *RPCProxy

  rpcServer *rpc.Server
  listener net.Listener

  peerClients map[int]*rpc.Client
  
  ready <-chan interface{}
  quit  chan interface{}
  commitChan chan<- CommitEntry
  wg    sync.WaitGroup
}

func NewServer( serverId int, peerIds []int,
  ready <-chan interface{}, commitChan chan<- CommitEntry ) *Server {
  s := new(Server)
  s.serverId = serverId
  s.peerIds = peerIds
  s.peerClients = make(map[int]*rpc.Client)
  s.ready = ready
  s.commitChan = commitChan
  s.quit = make(chan interface{})
  return s
}

func (s *Server) Serve() {
  s.mu.Lock()
  s.cm = NewConsensusModule( s.serverId, s.peerIds, s, s.ready, s.commitChan )

  s.rpcServer = rpc.NewServer()
  s.rpcProxy = &RPCProxy{cm : s.cm}
  s.rpcServer.RegisterName("ConsensusModule", s.rpcProxy)

  var err error
  s.listener, err = net.Listen("tcp", ":0")
  if err != nil {
    log.Fatal(err)
  }
  log.Printf("[%v] listening at %s", s.serverId, s.listener.Addr())
  s.mu.Unlock()

  s.wg.Add(1)
  go func() {
    defer s.wg.Done()
    for {
      conn, err := s.listener.Accept()
      if err != nil {
        select {
        case <-s.quit:
          return
        default:
          log.Fatal("accept error:", err)
        }
      }
      s.wg.Add(1)
      go func() {
        s.rpcServer.ServeConn( conn )
        s.wg.Done()
      }()
    }
  }()
}

func (s *Server) Shutdown() {
  s.cm.Stop()
  close(s.quit)
  s.listener.Close()
  s.wg.Wait()
}

func (s *Server) GetListenAddr() net.Addr {
  s.mu.Lock()
  defer s.mu.Unlock()
  return s.listener.Addr()
}

func (s *Server) ConnectToPeer( peerId int, addr net.Addr ) error {
  s.mu.Lock()
  defer s.mu.Unlock()
  if s.peerClients[peerId] == nil {
    client, err := rpc.Dial(addr.Network(), addr.String())
    if err != nil {
      return err
    }
    s.peerClients[peerId] = client
  }
  return nil
}

func (s *Server) Call( id int, serviceMethod string, args interface{}, reply interface{} ) error {
  s.mu.Lock()
  peer := s.peerClients[id]
  s.mu.Unlock()

  if peer == nil {
    return fmt.Errorf("called client %d but found closed", id)
  } else {
    return peer.Call(serviceMethod, args, reply)
  }
}

type MyCommand struct {
  C byte
}


func (s *Server) Submit(command MyCommand) bool {
  return s.cm.Submit(command)
}

type RPCProxy struct {
  cm *ConsensusModule
}

// Just forward the RPCs to underlying ConsensusModule
func (prx *RPCProxy) RequestVote(args RequestVoteArgs, reply *RequestVoteReply) error {
  return prx.cm.RequestVote(args, reply )
}

func (prx *RPCProxy) AppendEntries(args AppendEntriesArgs, reply *AppendEntriesReply) error {
  return prx.cm.AppendEntries( args, reply )
}
