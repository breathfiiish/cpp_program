#ifndef _THREAD_SAFE_QUEUE_HPP
#define _THREAD_SAFE_QUEUE_HPP
#include <iostream>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable> 
#include <memory>

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
    std::shared_ptr<T> WaitPop();
    bool TryPop(T &t);
    shared_ptr<T> TryPop();
    bool IsEmpty();
};
template<typename T> void ThreadSafe_Queue<T>::push(T data)
{
    lock_guard<mutex> lg(m_mut);
    m_queue.push(data);
    m_data_cond.notify_one();
}
template<typename T> void ThreadSafe_Queue<T>::WaitPop(T&t)
{
    unique_lock<mutex> ul(m_mut);
    m_data_cond.wait(ul, [this] {return !m_queue.empty(); });
    t = m_queue.front();
    m_queue.pop();
}
template<typename T> shared_ptr<T> ThreadSafe_Queue<T>::WaitPop()
{
    unique_lock<mutex> ul(m_mut);
    m_data_cond.wait(ul, [this] {return !m_queue.empty(); });

    shared_ptr<T> res(make_shared<T>(m_queue.front()));
    m_queue.pop();
    return res;
}
template<typename T> bool ThreadSafe_Queue<T>::TryPop(T &t)
{
    lock_guard<mutex> lg(m_mut);
    if (m_queue.empty())
        return false;

    t = m_queue.front();
    m_queue.pop();
    return true;
}

template<typename T> shared_ptr<T> ThreadSafe_Queue<T>::TryPop()
{
    lock_guard<mutex> lg(m_mut);
    if (m_queue.empty())
        return shared_ptr<T>();
    shared_ptr<T> res(make_shared<T>(m_queue.front()));
    m_queue.pop();
    return res;
}

template<typename T> bool ThreadSafe_Queue<T>::IsEmpty()
{
    lock_guard<mutex> lg(m_mut);
    return m_queue.empty();
}

#endif