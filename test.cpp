#include "thread_safe_queue.cpp"
#include <unistd.h>
ThreadSafe_Queue<int> g_queue;
int g_index = 10;

void thread_Fuc()
{
    cout << "thread_fuc1 start\n";
    while (true)
    {
        int value=0;
        g_queue.WaitPop(value);
        //printf("wait_and_pop done! value=%d  thread id:%d\n",value,GetCurrentThreadId());
        printf("value = %d",value);
    }
}

void thread_Fuc2()
{
    cout << "thread_fuc2 start\n";
    while (true)
    {
        sleep(10);
        g_index++;
        g_queue.push(g_index);
    }
}

int main()
{
    thread thd(thread_Fuc);
    thd.detach();

    thread thd2(thread_Fuc2);
    thd2.detach();

    int a;
    while (cin >> a){;}
    return 0;
}
