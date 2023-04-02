#include <iostream>
#include <future>
#include <thread>
#include <chrono>

#include "PromiseStore.H"
#include "TestUtils.H"
#include "ConsensusUtils.H"
#include "TimeTravelSignal.H"
#include "OhMyRaft.H"

using namespace raft;

int main()
{
  std::cout << "hello world!" << std::endl;
  
  raft::TimeTravelSignal sig;

  auto th = std::thread([&sig]{
    // now waiting
    sig.wait();
    std::cout << "got signal" << std::endl;
  });

  std::this_thread::sleep_for(std::chrono::seconds(2));

  sig.signal();
  th.join();

  sig.signal();

  auto th2 = std::thread([&sig]{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // now waiting
    sig.wait();
    std::cout << "got signal" << std::endl;
  });

  th2.join();


}
