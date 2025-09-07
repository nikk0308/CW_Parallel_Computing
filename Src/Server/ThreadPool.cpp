// ThreadPool.cpp
#include "ThreadPool.h"

using namespace std;

ThreadPool::ThreadPool(const function<void(SOCKET)>& func, const function<bool(SOCKET, int)>& notifyFunc,
                       int workersThreadCount, int clientsThreadCount, int notifyIntervalSeconds)
    : _handleClientTask(func), _notifyClientTask(notifyFunc), _notifyIntervalSeconds(notifyIntervalSeconds)
{
    Start(workersThreadCount, clientsThreadCount);
}

ThreadPool::~ThreadPool()
{
    Shutdown();
}

void ThreadPool::Start(const int workersCount, const int clientsCount)
{
    Shutdown();
    _isRunning.store(true);
    _workers.reserve(workersCount);
    _clientHolders.reserve(clientsCount);
    _clientNotifierThread = thread([&]() { ClientNotifier(); });

    for (size_t i = 0; i < workersCount; ++i)
        _workers.emplace_back([this] { WorkerLoop(); });
    for (size_t i = 0; i < clientsCount; ++i)
        _clientHolders.emplace_back([this] { ClientLoop(); });
}

void ThreadPool::Shutdown()
{
    if (!_isRunning.exchange(false))
        return;

    unique_lock lock(_mtxWorkers);
    _conVarWorkers.notify_all();
    lock.unlock();

    for (auto& t : _workers)
        if (t.joinable())
            t.join();

    _workers.clear();

    lock.lock();
    queue<function<void()>> empty;
    swap(_tasksWorkers, empty);
    lock.unlock();
}

const int ThreadPool::GetThreadCount() const
{
    return _workers.size();
}

void ThreadPool::AddNewClient(SOCKET clientSocket)
{
    if (!_isRunning.load())
        throw runtime_error("ThreadPool has been stopped");

    unique_lock lock(_mtxClients);
    _clientsIds.emplace_back(clientSocket);
    lock.unlock();
    _conVarClients.notify_one();
}

void ThreadPool::WorkerLoop()
{
    while (true)
    {
        function<void()> task;

        unique_lock lock(_mtxWorkers);
        _conVarWorkers.wait(lock, [this] { return !_isRunning || !_tasksWorkers.empty(); });

        if (!_isRunning && _tasksWorkers.empty())
            break;

        task = _tasksWorkers.front();
        _tasksWorkers.pop();
        lock.unlock();

        task();
    }
}

void ThreadPool::ClientLoop()
{
    while (true)
    {
        unique_lock lock(_mtxClients);
        _conVarClients.wait(lock, [this] { return !_isRunning || !_clientsIds.empty(); });

        if (!_isRunning && _clientsIds.empty())
            break;

        const SOCKET clientSocket = _clientsIds.front();
        _clientsIds.pop_front();
        lock.unlock();

        _handleClientTask(clientSocket);
    }
}

void ThreadPool::ClientNotifier()
{
    while (_isRunning.load())
    {
        unique_lock lock(_mtxClients);
        deque<SOCKET> aliveClients;

        int position = 1;

        for (SOCKET sock : _clientsIds)
            if (_notifyClientTask(sock, position))
            {
                aliveClients.push_back(sock);
                position++;
            }
            else
                closesocket(sock);

        _clientsIds.swap(aliveClients);
        lock.unlock();

        this_thread::sleep_for(chrono::seconds(_notifyIntervalSeconds));
    }
}