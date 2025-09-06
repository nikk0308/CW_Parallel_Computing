#ifndef CW_PARALLEL_COMPUTING_THREADPOOL_H
#define CW_PARALLEL_COMPUTING_THREADPOOL_H

#include <vector>
#include <thread>
#include <queue>
#include <future>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <winsock2.h>

using namespace std;

class ThreadPool
{
private:
    vector<thread> _workers;
    queue<function<void()>> _tasksWorkers;

    vector<thread> _clientHolders;
    deque<SOCKET> _clientsIds;
    function<void(SOCKET)> _handleClientTask;
    function<void(SOCKET, int)> _notifyClientTask;
    thread _clientNotifierThread;
    const int _notifyIntervalSeconds;

    condition_variable _conVarWorkers;
    condition_variable _conVarClients;
    mutex _mtxWorkers;
    mutex _mtxClients;
    atomic<bool> _isRunning{false};

    void WorkerLoop();
    void ClientLoop();
    void ClientNotifier();
public:
    ThreadPool(const function<void(SOCKET)>& func, const function<void(SOCKET, int)>& notifyFunc,
        int workersThreadCount, int clientsThreadCount, int notifyIntervalSeconds);
    ~ThreadPool();

    void Start(int workersCount, int clientsCount);
    void Shutdown();
    const int GetThreadCount() const;
    void AddNewClient(SOCKET clientSocket);

    template <class F, class... Args>
    auto Submit(F&& f, Args&&... args)
        -> future<invoke_result_t<F, Args...>>
    {
        using R = invoke_result_t<F, Args...>;

        shared_ptr taskPtr = make_shared<packaged_task<R()>>(
            bind(forward<F>(f), forward<Args>(args)...)
        );

        future<R> result = taskPtr->get_future();

        if (!_isRunning)
            throw runtime_error("ThreadPool has been stopped");

        unique_lock lock(_mtxWorkers);
        _tasksWorkers.emplace([taskPtr]() { (*taskPtr)(); });
        lock.unlock();
        _conVarWorkers.notify_one();

        return result;
    }
};

#endif //CW_PARALLEL_COMPUTING_THREADPOOL_H
