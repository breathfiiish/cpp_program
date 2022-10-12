class RingBuffer {
public:
    struct  Item
    {
      Item() : written(0)
      , logline("")
      { }
      mutable std::mutex flag;
      //std::atomic_flag flag;
      char written;
      std::string logline;
    };

    // struct SpinLock {
    //   SpinLock(std::atomic_flag & flag) : m_flag(flag)
    //   {
    //     while (m_flag.test_and_set(std::memory_order_acquire)); 
    //   }
    //   ~SpinLock()
    //   {
    //     m_flag.clear(std::memory_order_release);
    //   }
    // }

    RingBuffer(size_t const size) :
      m_size(size)
    , m_ring(new Item[m_size])
    , m_write_index(0)
    , m_read_index(0)
    {

    }

    ~RingBuffer()
    {
      delete[] m_ring;
    }

    void push(std::string && logline)
    {
      // 需要加锁，否则多个线程获取位置有问题
      m_rw_mutex.lock();
      size_t write_index = m_write_index % m_size;
      if((m_read_index % m_size == m_write_index % m_size) && (m_ring[write_index].written==1)) {
        m_read_index++;
      }
      m_write_index++;
      m_rw_mutex.unlock();

      // 拿到确定的slot,对该slot操作 需要加锁
      Item & item = m_ring[write_index];
      // 只对特定的slot加锁，有没有其他的方式只对一个slot加锁
      // 在item中设置mutex量，在这里对item的互斥量加锁
      // lock_guard 创建时加锁，作用域到了自动析构解锁

      // 也可以用自旋锁
      std::lock_guard<std::mutex> lg(item.flag);
      item.logline = std::move(logline);
      item.written = 1;
    }

    bool tryPop(std::string& logline) 
    {
      Item & item = m_ring[m_read_index % m_size];
      std::lock_guard<std::mutex> lg(item.flag);
      if(item.written == 1)
      {
        logline = std::move(item.logline);
        item.written = 0;
        ++m_read_index;
        return true;
      }
      return false;
    }

private:
    size_t const m_size;
    Item * m_ring;
    mutable std::mutex m_rw_mutex;
    unsigned int m_write_index;
    unsigned int m_read_index;
};


