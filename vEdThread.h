#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>

namespace vEdCard
{
typedef std::function<void (void)> Task;
class Thread
{
public:
    Thread();
    ~Thread();
    void setTask();
    void start();
    void stop();
    void join();

private:
    std::shared_ptr<std::thread> pthread_;
    void run();
    Task task_;

    std::mutex mtx_;
    std::condition_variable cv_;
    bool work_;
    bool stop_;
};
}// end namespace
