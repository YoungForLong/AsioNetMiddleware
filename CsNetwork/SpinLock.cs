using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace CsNetwork
{
    // thread-safe
    public class SpinLock<T>
    {
        public SpinLock(object mut, T data)
        {
            _mut = mut;
            _data = data;
        }
        public delegate void SpinLockAction(T data);
        private object _mut;
        private int _flag;
        private T _data;
        public void SafeAction(SpinLockAction act)
        {
            while (_flag == 1) ;
            Interlocked.Exchange(ref _flag, 1);
            lock (_mut)
            {
                try
                {
                    act(_data);
                }
                catch (Exception e)
                {
                    UnityEngine.Debug.LogError(e.ToString());
                }
            }
            Interlocked.Exchange(ref _flag, 0);
        }
    }
}
