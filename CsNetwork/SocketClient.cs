using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;

namespace CsNetwork
{
    public class SocketClient
    {
        const int MAX_SEND_BUFFER_SIZE = 10 * 1024 * 1024;
        const int MAX_RECV_BUFFER_SIZE = 10 * 1024 * 1024;
        const int DEFAULT_SEND_TIME_OUT = 60 * 1000;
        const int DEFAULT_RECV_TIME_OUT = 60 * 1000;
        const int MAX_TCP_CONTENT_SIZE = 1460;
        public const int MAX_FRAME_SIZE = 1460 - 11;

        // 服务器定义的包头
        // 包结构：
        // tcp stream:
        // | ip head | tcp head | -- frames -- |
        // 由于在流中，frame可能被截断
        // one frame:
        // |                   frame head:11                   |           body        |
        // |fk:1:0x2e|    len:4    |    cmd:2    |    seq:4    |size:2|pb|size:2|pb|...|
        struct TcpContentHead
        {
            Byte fk;
            UInt32 len;
            UInt16 cmd;
            UInt32 seq;

            public const int HEAD_SIZE = 11;
            public const Byte STATIC_FRAME_KEY = 0x2e;
            public const UInt16 COMMON_CMD = 0;
        }

        public enum SocketState
        {
            StateInit,
            StateConnecting,
            StateConnected,
            StateClosed,
            StateTimeout,
            StateOtherErr
        }

        object _sockMutex = new object();
        Socket _tcpSocket = null;
        Thread _sendThread = null;
        Thread _recvThread = null;
        int _socketId = 0;
        int _currentState = (int)SocketState.StateInit;
        int _lastState = (int)SocketState.StateInit;
        UInt32 _sendSeq = 0;
        UInt32 _recvSeq = 0;
        public Action<int> OnNetworkCmd = null;

        public SocketState State
        {
            get
            {
                return (SocketState)_currentState;
            }
            set
            {
                Interlocked.Exchange(ref _currentState, (int)value);
            }
        }
        int _connectTimeout = 0;
        int _startTimeMS = 0;

        // containers
        RingBuffer _sendBuffer = new RingBuffer(MAX_SEND_BUFFER_SIZE);
        RingBuffer _recvBuffer = new RingBuffer(MAX_RECV_BUFFER_SIZE);
        
        public static int GetHighResolutionTimeMS()
        {
            return Environment.TickCount;
        }

        Action _connectDoneAction;
        public void AsyncConnect(string host, int port, int timeout = -1, Action act = null)
        {
            _connectDoneAction = act;
            _connectTimeout = timeout > 0 ? timeout : DEFAULT_SEND_TIME_OUT;
            try
            {
                _startTimeMS = GetHighResolutionTimeMS();
                State = SocketState.StateConnecting;
                AddressFamily af = AddressFamily.InterNetwork;
                Socket sock = new Socket(af, SocketType.Stream, ProtocolType.Tcp);
                sock.SendTimeout = _connectTimeout;
                sock.ReceiveTimeout = DEFAULT_RECV_TIME_OUT;
                
                lock (_sockMutex)
                {
                    _tcpSocket = sock;
                }
                sock.BeginConnect(host, port, onAsyncConnectDone, sock);
            }
            catch (Exception e)
            {
                UnityEngine.Debug.LogError("SocketClient" + e.ToString());
            }
        }

        void onAsyncConnectDone(IAsyncResult ret)
        {
            Socket refSocket;

            lock(_sockMutex)
            {
                refSocket = _tcpSocket;
            }

            if (refSocket != null && refSocket == ret.AsyncState)
            {

                if (ret.IsCompleted)
                {
                    try
                    {
                        refSocket.EndConnect(ret);
                    }
                    catch (Exception e)
                    {
                        UnityEngine.Debug.LogError("SocketClient " + e.ToString());
                        State = SocketState.StateClosed;
                    }
                }

                if (refSocket.Connected)
                    State = SocketState.StateConnected;
                else
                    State = SocketState.StateClosed;

            }
        }

        void closeSocket()
        {
            Socket refSocket;
            lock(_sockMutex)
            {
                refSocket = _tcpSocket;
            }

            if (refSocket == null)
                return;

            try
            {
                refSocket.Shutdown(SocketShutdown.Both);
            }
            catch (Exception e)
            {

            }
            finally
            {
                refSocket.Close(1);
            }
        }

        void tickStateMachine()
        {
            if ((SocketState)_lastState != State)
            {
                stateLeave((SocketState)_lastState);
                stateEnter(State);
                _lastState = (int)State;
            }

            stateExec(State);
        }
        
        void frameHeadReset()
        {
            _sendSeq = 0;
            _recvSeq = 0;
        }

        UInt32 nextSeq(UInt32 seq)
        {
            return seq + 1;
        }

        void stateLeave(SocketState oldState)
        {

        }

        void stateEnter(SocketState newState)
        {
            switch (newState)
            {
                case SocketState.StateTimeout:
                    UnityEngine.Debug.LogError("SocketClient time out");
                    closeSocket();
                    break;
                case SocketState.StateClosed:
                    closeSocket();
                    break;
                case SocketState.StateConnected:
                    if (_connectDoneAction != null)
                        _connectDoneAction();
                    startThread();
                    break;
            }
        }

        void stateExec(SocketState state)
        {
            switch (state)
            {
                case SocketState.StateConnecting:
                    {
                        int curTick = GetHighResolutionTimeMS();
                        int delta = curTick - _startTimeMS;
                        if (delta > _connectTimeout)
                            State = SocketState.StateTimeout;
                        break;
                    }
            }
        }
        
        void startThread()
        {
            lock(this)
            {
                if (_sendThread == null)
                {
                    _sendThread = new Thread(sendThreadFunc);
                    _sendThread.Start();
                }
                if (_recvThread == null)
                {
                    _recvThread = new Thread(recvThreadFunc);
                    _recvThread.Start();
                }
            }
        }

        void sendThreadFunc()
        {
            for(; ; )
            {
                if (_sendBuffer.Empty())
                {
                    Thread.Sleep(1);
                }
                else
                {
                    int readAll = _sendBuffer.Count;
                    if (readAll > MAX_TCP_CONTENT_SIZE)
                    {
                        readAll = MAX_TCP_CONTENT_SIZE;
                    }
                    Byte[] data = BlockPool.Instance.Alloc(readAll);
                    if (_sendBuffer.TryRead(readAll, ref data))
                    {
                        Socket refSocket;

                        lock (_sockMutex)
                        {
                            refSocket = _tcpSocket;
                        }

                        try
                        {
                            if (State == SocketState.StateConnected)
                                refSocket.Send(data, readAll, SocketFlags.None);
                        }
                        catch (Exception e)
                        {
                            UnityEngine.Debug.LogError("SocketClient " + e.ToString());
                        }
                    }
                    else
                    {
                        UnityEngine.Debug.Assert(false);
                    }
                    BlockPool.Instance.Free(data);
                }
            }
        }

        Byte[] _socketRecvBuffer = new Byte[MAX_TCP_CONTENT_SIZE];
        void recvThreadFunc()
        {
            for(; ; )
            {
                if (_recvBuffer.Full())
                {
                    Thread.Sleep(1);
                }
                else
                {
                    Socket socketRef;
                    lock (_sockMutex)
                    {
                        socketRef = _tcpSocket;
                    }

                    try
                    {
                        if (State == SocketState.StateConnected)
                        {
                            int lenth = socketRef.Receive(_socketRecvBuffer);
                            _recvBuffer.TryWrite(_socketRecvBuffer, lenth);
                        }
                    }
                    catch (Exception e)
                    {
                        UnityEngine.Debug.LogError("SocketClient " + e.ToString());
                    }
                }
            }
        }

        public void Tick()
        {
            tickStateMachine();
        }

        Byte[] _sendFrameHead = new Byte[TcpContentHead.HEAD_SIZE];
        // 此处得到的data应该是每一帧收集的所有数据，在帧的末尾处理
        public void Send(Byte[] data, int len)
        {
            while(true)
            {
                if (_sendBuffer.Full(len + TcpContentHead.HEAD_SIZE))
                {
                    UnityEngine.Debug.LogError("SocketClient" + "Send buffer is Full");
                    Thread.Sleep(1);
                }
                else
                {
                    DataBlock wrappedData = WrapSendData(data, len);
                    if (!_sendBuffer.TryWrite(wrappedData.data, wrappedData.length))
                    {
                        // "Some Err Uknown happend in multi thread"
                        UnityEngine.Debug.Assert(false);
                    }
                    BlockPool.Instance.Free(wrappedData.data);
                    break;
                }
            }
        }

        DataBlock WrapSendData(Byte[] data, int length = 0)
        {
            UInt32 len = (UInt32)(length > 0 ? length : data.Length);
            UInt16 cmd = TcpContentHead.COMMON_CMD;
            UInt32 seq = nextSeq(_sendSeq);
            _sendSeq = seq;
            if (!BitConverter.IsLittleEndian)
            {
                len = len.BigEndianValue();
                cmd = cmd.BigEndianValue();
                seq = seq.BigEndianValue();
            }

            // fk
            _sendFrameHead[0] = TcpContentHead.STATIC_FRAME_KEY;

            // len
            _sendFrameHead[1] = (Byte)((len >> 24) & 0xFF);
            _sendFrameHead[2] = (Byte)((len >> 16) & 0xFF);
            _sendFrameHead[3] = (Byte)((len >> 8) & 0xFF);
            _sendFrameHead[4] = (Byte)(len & 0xFF);

            // cmd
            _sendFrameHead[5] = (Byte)((cmd >> 8) & 0xFF);
            _sendFrameHead[6] = (Byte)(cmd & 0xFF);

            // seq
            _sendFrameHead[7] = (Byte)((seq >> 24) & 0xFF);
            _sendFrameHead[8] = (Byte)((seq >> 16) & 0xFF);
            _sendFrameHead[9] = (Byte)((seq >> 8) & 0xFF);
            _sendFrameHead[10] = (Byte)(seq & 0xFF);

            int arrLength = TcpContentHead.HEAD_SIZE + (int)len;
            Byte[] newRet = BlockPool.Instance.Alloc(arrLength);
            for(int i = 0; i < arrLength; i ++)
            {
                if (i < TcpContentHead.HEAD_SIZE)
                    newRet[i] = _sendFrameHead[i];
                else
                    newRet[i] = data[i - TcpContentHead.HEAD_SIZE];
            }
            DataBlock db = new DataBlock();
            db.data = newRet;
            db.length = arrLength;
            return db;
        }

        public Action<Byte[], int> RecvHandler { set; get; }

        Byte[] _recvFrameHead = new Byte[TcpContentHead.HEAD_SIZE];
        // 此处收到的数据应该是服务器包裹起来的协议
        public bool Recv(Action<Byte[], int> handler = null)
        {
            if (!_recvBuffer.TryRead(TcpContentHead.HEAD_SIZE, ref _recvFrameHead, true))
            {
                return false;
            }

            Byte fk = _recvFrameHead[0];
            // 客户端不处理fk

            // 此处避免大小端问题，尽量避免位移
            UInt32 len = System.BitConverter.ToUInt32(_recvFrameHead, 1);
            UInt16 cmd = System.BitConverter.ToUInt16(_recvFrameHead, 5);
            _recvSeq = System.BitConverter.ToUInt32(_recvFrameHead, 7);
            
            if (cmd != TcpContentHead.COMMON_CMD && OnNetworkCmd != null)
            {
                OnNetworkCmd((int)cmd);
            }

            Byte[] frameBody = BlockPool.Instance.Alloc((int)len);
            if (!_recvBuffer.TryRead((int)len, ref frameBody))
            {
                BlockPool.Instance.Free(frameBody);
                return false;
            }

            if (!_recvBuffer.TryReadShift(TcpContentHead.HEAD_SIZE))
            {
                UnityEngine.Debug.LogError("SocketClient: some err known occured");
            }

            int pbIndex = 0;
            for (; ; )
            {
                if (pbIndex >= len)
                    break;
                UInt16 size = System.BitConverter.ToUInt16(frameBody, pbIndex);
                pbIndex += 2;
#if UNITY_EDITOR
                UnityEngine.Debug.Assert(pbIndex < len && (pbIndex + size) < len);
#endif
                Byte[] pb = BlockPool.Instance.Alloc(size);
                for (int i = 0; i < size; ++i)
                {
                    pb[i] = frameBody[pbIndex + i];
                }
                pbIndex += size;

                if (handler != null)
                    handler(pb, size);
                else
                    RecvHandler(pb, size);
            }

            BlockPool.Instance.Free(frameBody);

            return true;
        }
    }
}
