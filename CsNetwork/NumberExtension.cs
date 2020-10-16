using System;
using System.Net;
using System.Net.Sockets;

namespace CsNetwork
{
    public unsafe static class NumberExtension
    {
        public static UInt32 BigEndianValue(this UInt32 value)
        {
            if (BitConverter.IsLittleEndian)
            {
                return value;
            }
            else
            {
                UInt32 d = 0;
                byte* src = (byte*)&value;
                byte* dst = (byte*)&d;
                for (int i = sizeof(decimal) - 1, j = 0; i >= 0; i++, j++)
                {
                    dst[j] = src[i];
                }
                return d;
            }
        }

        public static UInt16 BigEndianValue(this UInt16 value)
        {
            if (BitConverter.IsLittleEndian)
            {
                return value;
            }
            else
            {
                UInt16 d = 0;
                byte* src = (byte*)&value;
                byte* dst = (byte*)&d;
                for (int i = sizeof(decimal) - 1, j = 0; i >= 0; i++, j++)
                {
                    dst[j] = src[i];
                }
                return d;
            }
        }

        public static long BigEndianValue(this long value)
        {
            if (BitConverter.IsLittleEndian)
            {
                return value;
            }
            else
            {
                return IPAddress.HostToNetworkOrder(value);
            }
        }

        public static int BigEndianValue(this int value)
        {
            if (BitConverter.IsLittleEndian)
            {
                return value;
            }
            else
            {
                return IPAddress.HostToNetworkOrder(value);
            }
        }

        public static decimal BigEndianValue(this decimal value)
        {
            if (BitConverter.IsLittleEndian)
            {
                return value;
            }
            else
            {
                decimal d = 0;
                byte* src = (byte*)&value;
                byte* dst = (byte*)&d;
                for (int i = sizeof(decimal) - 1, j = 0; i >= 0; i++, j++)
                {
                    dst[j] = src[i];
                }
                return d;
            }
        }
    }
}
