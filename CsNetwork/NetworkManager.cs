using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using CsNetwork.Serialization;
using LuaInterface;
using LZ4Sharp;

namespace CsNetwork
{
    // 该管理类不需要照顾线程安全
    public class NetworkManager
    {
        public static NetworkManager _instance = null;
        public static NetworkManager Instance
        {
            get
            {
                if (_instance == null)
                    _instance = new NetworkManager();
                return _instance;
            }
        }

        SocketClient _socket = new SocketClient();

        public Byte[] Compress(Byte[] data)
        {
            return LZ4.Compress(data);
        }

        public Byte[] Decomporess(Byte[] data)
        {
            return LZ4.Decompress(data);
        }

        //public Byte[] Decrypt(Byte[] key, Byte[] data)
        //{

        //}

        //public Byte[] Encrypt()
        //{

        //}

        public void Connect(string host, int port, Action onConnect)
        {
            _socket.AsyncConnect(host, port, -1, onConnect);
        }

        public void SendCheckToken(int areaId, int sid, string platform)
        {
            ServerToken svr = new ServerToken();
            svr.SvrAreaNo = (ushort)areaId;
            svr.ServerNo = (ushort)sid;
            svr.Platform = platform;
            Byte[] token = BinaryFormatter.Serialize(svr, false);
            _socket.Send(token, token.Length);
        }

        List<DataBlock> _sendWaitQueue = new List<DataBlock>();
        int _usedQueueIdx = 0; // avoid gc
        public void Send(Byte[] data, int len)
        {
            DataBlock db;
            if (_sendWaitQueue.Count > _usedQueueIdx)
            {
                db = _sendWaitQueue[_usedQueueIdx];
                db.data = data;
                db.length = len;
            }
            else
            {
                db = new DataBlock { data = data, length = len };
                _sendWaitQueue.Add(db);
            }
            _usedQueueIdx++;
        }

        public void SendAllWhenUpdateEnd()
        {
            int totalLength = 0;
            DataBlock db;
            for (int i = 0; i < _sendWaitQueue.Count; ++i)
            {
                db = _sendWaitQueue[i];
                if ((totalLength + 2 + db.length > SocketClient.MAX_FRAME_SIZE) || (i == _sendWaitQueue.Count - 1))
                {
                    Byte[] data = BlockPool.Instance.Alloc(totalLength);
                    int offset = 0;
                    for (int start = i - totalLength; start < i; ++start)
                    {
                        DataBlock frameDb = _sendWaitQueue[start];
                        // do copy
                        for (int j = 0; j < frameDb.length; ++j)
                        {
                            data[offset + j] = frameDb.data[j];
                        }
                        offset += frameDb.length;
                    }
                    _socket.Send(data, totalLength);
                    BlockPool.Instance.Free(data);
                    totalLength = 0;
                }
                else
                {
                    totalLength += 2;
                    totalLength += db.length;
                }
            }

            _usedQueueIdx = 0;
        }

        public void Update(float delta)
        {
            _socket.Tick();
        }
        
        public void SetRecvHandler(Action<Byte[], int> handler)
        {
            _socket.RecvHandler = handler;
        }

        public void Receive(LuaTable ret)
        {
            _socket.Recv();
        }

        public void CallCacheClear()
        {
            _sendWaitQueue.Clear();
        }
    }
}
