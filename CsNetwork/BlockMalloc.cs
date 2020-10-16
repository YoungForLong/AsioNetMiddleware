using System;
using System.Collections.Generic;
using System.Threading;

namespace CsNetwork
{
    // thread-safe none-gc blocking pool

    public class DataBlock
    {
        public Byte[] data;
        public int length;
    }

    public class BlockPool
    {
        static object _instanceMutex = new object();
        static int _flag = 0;
        static BlockPool _instance = null;
        public static BlockPool Instance
        {
            get
            {
                while (_flag == 1) ;
                Interlocked.Exchange(ref _flag, 1);
                BlockPool ret = null;
                lock (_instanceMutex)
                {
                    if (_instance == null)
                        _instance = new BlockPool();
                    ret = _instance;
                }
                Interlocked.Exchange(ref _flag, 0);
                return ret;
            }
        }

        const int K_MIN_BLOCK_SIZE = 8;
        const int K_LARGE_BLOCK_SIZE = 1024;
        const int K_MAX_BLOCK_SIZE = 8 * 1024;

        public BlockPool()
        {
            _cacheSpinlock = new SpinLock<Stack<Byte[]>[]>(_cacheMutex, _chacheMap);
            _instance.initSizeArray();
        }

        public Byte[] Alloc(int size)
        {
            int idx = 0;
            if (sizePoolIndexMaybe(size, ref idx))
            {
                Byte[] ret = null;
                _cacheSpinlock.SafeAction((Stack<Byte[]>[] container) =>
                {
                    Stack<Byte[]> pool = container[idx];
                    int alignSize = _sizeArray[idx];
                    if (pool.Count != 0)
                    {
                        ret = new Byte[alignSize];
                    }
                    else
                    {
                        ret = pool.Pop();
                    }
                });
                return ret;
            }
            else
            {
                return new Byte[size];
            }
        }

        public void Free(Byte[] data)
        {
            int idx = 0;
            if (sizePoolIndexMaybe(data.Length, ref idx))
            {
                _cacheSpinlock.SafeAction((Stack<Byte[]>[] container) =>
                {
                    Stack<Byte[]> pool = container[idx];
                    pool.Push(data);
                });
            }
        }

        // tc malloc strategy
        // Examples:
        //   Size       Expression                      Index       
        //   -----------------------------------------------------
        //   0          (0 + 7) / 8                     0           
        //   1          (1 + 7) / 8                     1          
        //   ...
        //   1024       (1024 + 7) / 8                  128
        //   1025       (1025 + 127 + (120<<7)) / 128   129
        //   ...
        //   8192       (8192 + 127 + (120<<7)) / 128   184
        const int TOTAL_ARRAY_COUNT = (K_MAX_BLOCK_SIZE + 127 + (120 << 7)) >> 7;
        int[] _sizeArray = new int[TOTAL_ARRAY_COUNT];

        object _cacheMutex = new object();
        SpinLock<Stack<Byte[]>[]> _cacheSpinlock;
        Stack<Byte[]>[] _chacheMap = new Stack<Byte[]>[TOTAL_ARRAY_COUNT];
        bool sizePoolIndexMaybe(int size, ref int idx)
        {
            if (size <= K_LARGE_BLOCK_SIZE)
            {
                idx = (size + 7) >> 3;
                return true;
            }
            else if (size <= K_MAX_BLOCK_SIZE)
            {
                idx = (size + 127 + (120 << 7)) >> 7;
                return true;
            }
            else
            {
                return false;
            }
        }
        
        void initSizeArray()
        {
            for(int i = 0; i <= K_MAX_BLOCK_SIZE; ++i)
            {
                if (i <= K_LARGE_BLOCK_SIZE)
                {
                    _sizeArray[(i + 7) >> 3] = i;
                }
                else
                {
                    _sizeArray[(i + 127 + (120 << 7)) >> 7] = i;
                }
            }
        }
        
    }
}
