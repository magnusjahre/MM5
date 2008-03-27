/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/memory_controller.hh"

using namespace std;

TimingMemoryController::TimingMemoryController() {
  isBlockedFlag = false;
  isPrewriteBlockedFlag = false;
}

/** Frees locally allocated memory. */
TimingMemoryController::~TimingMemoryController(){
}

void
TimingMemoryController::setBlocked()
{
  assert(!isBlockedFlag);
  cout << curTick << ": blocking\n";
  startBlocking = curTick;
  isBlockedFlag = true;
}

void
TimingMemoryController::setUnBlocked()
{
  assert(isBlockedFlag);
  cout << curTick << ": unblocking\n";
  totalBlocktime += (curTick - startBlocking);
  bus->incrementBlockedCycles(curTick - startBlocking);
  isBlockedFlag = false;
}

void
TimingMemoryController::setPrewriteBlocked()
{
  assert(!isPrewriteBlockedFlag);
  isPrewriteBlockedFlag = true;
}

void
TimingMemoryController::setPrewriteUnBlocked()
{
  assert(isPrewriteBlockedFlag);
  isPrewriteBlockedFlag = false;
}

Addr
TimingMemoryController::getPage(MemReqPtr &req)
{
  return (req->paddr >> 10);
}

