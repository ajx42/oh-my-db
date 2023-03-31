#include <iostream>
#include <future>
#include <thread>
#include <chrono>

#include "PromiseStore.H"
#include "TestUtils.H"
#include "ConsensusUtils.H"

#include "OhMyRaft.H"

using namespace raft;

int main()
{
  std::cout << "hello world!" << std::endl;

  std::promise<int> myPromise;
  auto future = myPromise.get_future();
  auto it = PromiseStore<int>::Instance().insert( std::move( myPromise ) );
 
  std::thread t([&it]{
    std::cout << PromiseStore<int>::Instance().size() << std::endl;
    auto promiseAgain = PromiseStore<int>::Instance().getAndRemove( it );

    std::cout << PromiseStore<int>::Instance().size() << std::endl;
    promiseAgain.set_value(10);
  });

  t.join();
 
  std::cout << future.get() << std::endl;

  using OpII = Operation<int, int>;

  std::promise<OpII::res_t> putp;
  std::promise<OpII::res_t> getp;
  auto putf = putp.get_future();
  auto getf = getp.get_future();

  auto puti = PromiseStore<OpII::res_t>::Instance().insert( std::move( putp ) );
  auto geti = PromiseStore<OpII::res_t>::Instance().insert( std::move( getp ) );

  OpII putop { .kind = OpII::PUT, .args = std::pair<int,int>{2, 3}, .promiseHandle = {puti} };
  OpII getop { .kind = OpII::GET, .args = {2}, .promiseHandle = {geti} };

  RaftManager<RaftClientProxy> rmgr(0);

  rmgr.start();
  
  RaftOp op = putop;
  rmgr.submit( putop );
  std::cout << std::get<bool>(putf.get()) << std::endl;
  rmgr.stop();

  std::thread getth([&getop] {
    getop.execute();
  });


  std::cout << std::get<std::optional<int>>(getf.get()).value_or(-1) << std::endl;

  getth.join();
}
