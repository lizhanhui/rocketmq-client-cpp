#include "rocketmq/DefaultMQPushConsumer.h"
#include "rocketmq/State.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace rocketmq;

namespace rocketmq {

class ExecutorImpl {
public:
  ExecutorImpl() : state_(State::CREATED) {}

  virtual ~ExecutorImpl() {
    switch (state_.load(std::memory_order_relaxed)) {
    case CREATED:
    case STOPPING:
    case STOPPED:
      break;

    case STARTING:
    case STARTED:
      state_.store(State::STOPPED);
      if (worker_.joinable()) {
        worker_.join();
      }
      break;
    }
  }

  void submit(const std::function<void(void)>& task) {
    if (State::STOPPED == state_.load(std::memory_order_relaxed)) {
      return;
    }

    {
      std::unique_lock<std::mutex> lock(task_mtx_);
      tasks_.push_back(task);
    }
    cv_.notify_one();
  }

  void start() {
    State expected = State::CREATED;
    if (state_.compare_exchange_strong(expected, State::STARTING)) {
      worker_ = std::thread(std::bind(&ExecutorImpl::loop, this));
      state_.store(State::STARTED);
    }
  }

  void stop() {
    state_.store(State::STOPPED);
    if (worker_.joinable()) {
      worker_.join();
    }
  }

private:
  void loop() {
    while (state_.load(std::memory_order_relaxed) != State::STOPPED) {
      std::function<void(void)> func;
      {
        std::unique_lock<std::mutex> lk(task_mtx_);
        if (!tasks_.empty()) {
          func = tasks_.back();
        }
      }

      if (func) {
        func();
      } else {
        std::unique_lock<std::mutex> lk(task_mtx_);
        cv_.wait_for(lk, std::chrono::seconds(3),
                     [&]() { return state_.load(std::memory_order_relaxed) == State::STOPPED || !tasks_.empty(); });
      }
    }
  }

  std::atomic<State> state_;
  std::vector<std::function<void(void)>> tasks_;
  std::mutex task_mtx_;
  std::condition_variable cv_;
  std::thread worker_;
};

} // namespace rocketmq

class SampleMQMessageListener : public StandardMessageListener {
public:
  ConsumeStatus consumeMessage(const std::vector<MQMessageExt>& msgs) override {
    std::lock_guard<std::mutex> lk(console_mtx_);
    for (const MQMessageExt& msg : msgs) {
      std::cout << "Topic=" << msg.getTopic() << ", MsgId=" << msg.getMsgId() << ", Body=" << msg.getBody()
                << std::endl;
    }
    return ConsumeStatus::CONSUME_SUCCESS;
  }

private:
  std::mutex console_mtx_;
};

int main(int argc, char* argv[]) {

  Logger& logger = getLogger();
  logger.setLevel(Level::Debug);
  logger.init();

  DefaultMQPushConsumer push_consumer("TestGroup");
  MessageListener* listener = new SampleMQMessageListener;

  auto pool = new ExecutorImpl;
  pool->start();
  push_consumer.setCustomExecutor(std::bind(&ExecutorImpl::submit, pool, std::placeholders::_1));
  push_consumer.setGroupName("TestGroup");
  push_consumer.setInstanceName("CID_sample_member_0");
  push_consumer.subscribe("TestTopic", "*");
   push_consumer.setNamesrvAddr("11.167.164.105:9876");
  push_consumer.registerMessageListener(listener);
  push_consumer.start();

  std::this_thread::sleep_for(std::chrono::minutes(30));
  pool->stop();
  delete pool;

  push_consumer.shutdown();
  return EXIT_SUCCESS;
}
