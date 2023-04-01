package main

import "fmt"
import "time"
import "raft/raftv"
import "encoding/gob"

func startUp( n int ) {
  gob.Register(raft.MyCommand{})

  ns := make([]*raft.Server, n)
  connected := make([]bool, n)
  ready := make(chan interface{})
  commitChans := make([]chan raft.CommitEntry, n)

  // the above just allocates the container i.e. the slice of channels
  // additionally we need to initialise each channel individually
  for i, _ := range commitChans {
    commitChans[i] = make(chan raft.CommitEntry, 16)
  }

  // Create servers for each peer
  for i := 0; i < n; i++ {
    peerIds := make([]int, 0)
    for j := 0; j < n; j++ {
      if j != i {
        peerIds = append( peerIds, j )
      }
    }
    ns[i] = raft.NewServer(i, peerIds, ready, commitChans[i])
    ns[i].Serve()
  }

  // Connect all peers to each other
  for i := 0; i < n; i++ {
    for j := 0; j < n; j++ {
      if i != j {
        ns[i].ConnectToPeer(j, ns[j].GetListenAddr())
      }
    }
    connected[i] = true
  }
  close(ready)
  
  for i := 0; i < n; i++ {
    go func( id int ) {
      fmt.Println("starting monitoring committed entries")
      for val := range commitChans[id] {
        fmt.Println("new committed call found for peer", val)
      }
      fmt.Println("done monitoring committed entries")
    }(i)
  }

  time.Sleep(time.Duration(10) * time.Second)

  // I don't know who is the leader so just sending to all
  a := raft.MyCommand { C: 'A' }
  b := raft.MyCommand { C: 'B' }
  
  ns[0].Submit( a )
  ns[1].Submit( a )
  ns[2].Submit( a )
  ns[3].Submit( a )
  ns[4].Submit( a )

  ns[0].Submit( b )
  ns[1].Submit( b )
  ns[2].Submit( b )
  ns[3].Submit( b )
  ns[4].Submit( b )
}



func main() {
  fmt.Println( "hello world" )
  startUp(5)

  time.Sleep(time.Duration(100) * time.Second)
}
