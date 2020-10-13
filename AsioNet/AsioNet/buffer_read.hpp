#pragma once

#include <memory>
#include <cstring>
#include <string>
#include <iostream>
#include <functional>

#include "NetUtils.hpp"
#include "asio.hpp"
#include "protocol.hpp"

namespace net_middleware
{
	using asio::ip::tcp;

	typedef std::function<void(unsigned char*, size_t)> handle_entire_msg;

	void sync_read_from_socket(tcp::socket& _sock, size_t& _read_offset, once_buffer_sptr _recv_buffer, const handle_entire_msg& cb)
	{
		try
		{
			size_t avai_read = _sock.available();
			if (0 == avai_read)
			{
				return;
			}

			_read_offset += _sock.read_some(asio::buffer((unsigned char*)(&(_recv_buffer->buffer[_read_offset])), ONCE_BUFFER_SIZE - _read_offset));

			while (_read_offset > PROTO_HEAD_SIZE)
			{
				// check head structure
				auto head = protocol_head::unpack_head(_recv_buffer->buffer);
				if (head->len > (_read_offset - PROTO_HEAD_SIZE))
				{
					// things left in net link
					break;
				}
				else
				{
					cb(&(_recv_buffer->buffer[PROTO_HEAD_SIZE]), head->len);

					size_t unpack_left = _read_offset - (head->len + PROTO_HEAD_SIZE);
					std::memmove(_recv_buffer->buffer, &(_recv_buffer->buffer[(head->len + PROTO_HEAD_SIZE)]), unpack_left);

					_read_offset = unpack_left;
				}
			}
		}
		catch (asio::system_error e)
		{
			std::cout << "some error occurred during receiving: " << e.code() << "    " << e.what() << std::endl;
		}
	}
}