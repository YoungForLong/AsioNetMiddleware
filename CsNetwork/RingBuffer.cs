using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using UnityEngine;

namespace CsNetwork
{
    /// <summary>
    /// 无锁环形队列，请保证消费者单一和生产者单一
    /// 为了高效，未对同时读或同时写进行加锁
    /// </summary>
    internal class RingBuffer
    {
        Byte[] _buffer = null;

        int _readIndex = 0;

        int _writeIndex = 0;

        public int Count
        {
            get
            {
                return (_writeIndex - _readIndex) % _capacity;
            }
        }

        int _capacity = 0;

        public RingBuffer(int capacity)
        {
            _capacity = capacity;
            _buffer = new Byte[_capacity];
        }

        public bool Full(int add = 0)
        {
            return (Count + add) >= _capacity;
        }

        public bool Empty()
        {
            return Count <= 0;
        }

        /// <param name="size"></param>
        /// <param name="ret">if return false, plz wait a while</param>
        /// <param name="withoutShift">data will be checking only, wont be pop out</param>
        /// <returns></returns>
        public bool TryRead(int size, ref Byte[] ret, bool withoutShift = false)
        {
            if (Empty())
                return false;
            else
            {
                // there will be only one consumer, so it's naturally thread-safe
                if (Count - size <= 0)
                    return false;

                for (int i = 0; i < size; ++i)
                {
                    if (withoutShift)
                    {
                        ret[i] = _buffer[_readIndex + i];
                    }
                    else
                    {
                        _readIndex += i;
                        ret[i] = _buffer[_readIndex];
                    }
                }
            }

            return false;
        }

        /// <summary>
        /// using with TryRead(withoutShift)
        /// </summary>
        /// <returns></returns>
        public bool TryReadShift(int size)
        {
            if (Count - size <= 0)
                return false;

            _readIndex += size;
            return true;
        }

        /// <param name="ret">if return false, plz wait a while</param>
        /// <returns></returns>
        public bool TryWrite(Byte[] data, int length = 0)
        {
            if (Full())
                return false;
            else
            {
                int size = data.Length;
                if (length != 0)
                    size = length;
                if (size + Count >= _capacity)
                    return false;
                for (int i = 0; i < size; ++i)
                {
                    _writeIndex += i;
                    _buffer[_writeIndex] = data[i];
                }
            }

            return false;
        }

        public void Clear()
        {
            _readIndex = 0;
            _writeIndex = 0;
        }
    }
}
