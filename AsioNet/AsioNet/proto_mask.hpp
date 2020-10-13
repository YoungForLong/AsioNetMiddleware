#pragma once
#include <cstdint>

namespace net_middleware
{
	class proto_mask
	{
    public:
        static uint16_t get_mask(unsigned char* head, size_t head_len, unsigned char* data, size_t data_len, uint32_t acc = 0)
        {
            uint32_t inverseMask = acc;
			inverseMask = mask_off_32(head, head_len, inverseMask);
			inverseMask = mask_off_32(data, data_len, inverseMask);
			return (uint16_t)~(inverseMask & 0xffff);
        }

        static uint32_t mask_off_32(void* dataptr, size_t len, uint32_t acc)
        {
            uint16_t src;
            unsigned char* octetptr;

            /* dataptr may be at odd or even addresses */
            octetptr = (unsigned char*)dataptr;
            while (len > 1)
            {
                /* declare first octet as most significant
                   thus assume network order, ignoring host order */
                src = (uint16_t)((*octetptr) << 8);
                octetptr++;
                /* declare second octet as least significant */
                src |= (*octetptr);
                octetptr++;
                acc += src;
                len -= 2;
            }
            if (len > 0)
            {
                /* accumulate remaining octet */
                src = (uint16_t)((*octetptr) << 8);
                acc += src;
            }
            /* add deferred carry bits */
            acc = (uint32_t)((acc >> 16) + (acc & 0x0000ffff));
            if ((acc & 0xffff0000) != 0)
            {
                acc = (uint32_t)((acc >> 16) + (acc & 0x0000ffff));
            }
            return acc;
        }

	};
}
