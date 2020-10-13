#pragma once

namespace net_middleware
{
    enum TransferError
    {
        EC_Success = 0x0,
        EC_ServerNotFound,
        EC_CodeEnd = 0xff
    };
}