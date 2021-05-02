#include<iostream>
#include<mutex>
#include<thread>
#include<queue>
#include <condition_variable> 
using namespace std;
template<typename T> class ThreadSafe_Queue
{
private:
    mutable mutex m_mut;
    queue<T> m_queue;
    condition_variable m_data_cond;
public:
    ThreadSafe_Queue() {}
    ThreadSafe_Queue(const ThreadSafe_Queue&) = delete;
    void push(T data);
    void WaitPop(T&t);
    shared_ptr<T> WaitPop();
    bool TryPop(T &t);
    shared_ptr<T> TryPop();
    bool IsEmpty();
};
