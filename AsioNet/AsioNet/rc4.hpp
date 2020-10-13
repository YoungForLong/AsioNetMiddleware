#pragma once

namespace net_middleware
{
	class rc4
	{
	public:
		inline static void rc4_sbox(int mod, unsigned char* box, unsigned char* key, size_t keylen)
		{
			if (NULL == box || NULL == key || keylen <= 0)
				return;

			for (int i = 0; i < mod; i++)
				box[i] = (unsigned char)i;

			for (int i = 0, j = 0; i < mod; i++) {
				j = (j + box[i] + key[i % keylen]) % mod;
				unsigned char b = box[i];
				box[i] = box[j];
				box[j] = b;
			}
		}

		static void rc4_crypt(int mod, unsigned char* key, int keylen, unsigned char* data, size_t datalen, int subtract, int E)
		{
			if (NULL == key || keylen <= 0 || NULL == data || datalen <= 0)
				return;

			unsigned char box[255];
			rc4_sbox(mod, box, key, keylen);

			unsigned char x = (unsigned char)(E ? subtract : -subtract);
			for (int i = 0, low = 0, high = 0, mid; i < datalen; i++)
			{
				low = low % mod;
				high = (high + box[i % mod]) % mod;

				unsigned char b = box[low];
				box[low] = box[high];
				box[high] = b;

				mid = (box[low] + box[high]) % mod;
				if (E)
					data[i] = (unsigned char)((data[i] ^ box[mid]) - x);
				else
					data[i] = (unsigned char)((data[i] - x) ^ box[mid]);
			}
		}
	};
}

