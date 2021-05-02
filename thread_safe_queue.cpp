#include "thread_safe_queue.h"
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

