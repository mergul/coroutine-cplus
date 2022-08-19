#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <future>

#include "fs_read.cpp"
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/sync_wait.hpp>


using namespace std::chrono;

int main()
{
  cppcoro::io_service ioService;
  cppcoro::sync_wait(cppcoro::when_all_ready(
    run(ioService),
    process_events(ioService)));

  return 0;
}