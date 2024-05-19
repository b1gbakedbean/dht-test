#include <time.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct timezone
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};

prng_state _prng_state{};
sockaddr_in good4[500]{};
sockaddr_in6 good6[500]{};

const unsigned char hash[20] = {
	0x54, 0x57, 0x87, 0x89, 0xdf, 0xc4, 0x23, 0xee, 0xf6, 0x03,
	0x1f, 0x81, 0x94, 0xa9, 0x3a, 0x16, 0x98, 0x8b, 0x72, 0x7b
};

std::list<std::pair<const std::string, short>> bootstrap_node_list
{
	std::make_pair("router.utorrent.com", 6881),
	std::make_pair("router.bittorrent.com", 6881),
	std::make_pair("dht.transmissionbt.com", 6881),
	std::make_pair("dht.aelitis.com", 6881)
};

static void dht_callback(void* closure, int event_type, const unsigned char* info_hash, const void* data, size_t data_len)
{
	switch (event_type)
	{
	case DHT_EVENT_SEARCH_DONE:
		fmt::print("Search done.\n");
		break;

	case DHT_EVENT_SEARCH_DONE6:
		fmt::print("IPv6 search done.\n");
		break;

	case DHT_EVENT_VALUES:
		fmt::print("Received {} values.\n", (int)(data_len / 6));
		break;

	case DHT_EVENT_VALUES6:
		fmt::print("Received {} IPv6 values.\n", (int)(data_len / 18));
		break;

	default:
		fmt::print("Unknown DHT event {}.\n", event_type);
	}
}

int main(int argc, char* argv[])
{
	dht_debug = stdout;

	register_prng(&yarrow_desc);
	rng_make_prng(128, find_prng("yarrow"), &_prng_state, nullptr);

	WSAData wsa_data;

	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
	{
		fmt::print("[Error] WSAStartup() failed: {}\n", WSAGetLastError());
		return 1;
	}

	SOCKET s4;

	if ((s4 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		fmt::print("[Error] socket() failed: {}\n", WSAGetLastError());
		return 2;
	}

	{
		u_long iocl_mode = 1;

		if (ioctlsocket(s4, FIONBIO, &iocl_mode) == SOCKET_ERROR)
		{
			fmt::print("[Error] ioctlsocket() failed: {}\n", WSAGetLastError());
			return 3;
		}
	}

	sockaddr_in sin{};

	sin.sin_family = AF_INET;
	sin.sin_port = htons(1337);

	inet_pton(AF_INET, "0.0.0.0", &sin.sin_addr);

	if (bind(s4, reinterpret_cast<const sockaddr*>(&sin), sizeof(sin)))
	{
		fmt::print("[Error] bind() failed: {}\n", WSAGetLastError());
		return 4;
	}

	SOCKET s6;

	if ((s6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		fmt::print("[Error] socket() failed: {}\n", WSAGetLastError());
		return 5;
	}

	{
		u_long iocl_mode = 1;

		if (ioctlsocket(s6, FIONBIO, &iocl_mode) == SOCKET_ERROR)
		{
			fmt::print("[Error] ioctlsocket() failed: {}\n", WSAGetLastError());
			return 6;
		}
	}

	sockaddr_in6 sin6{};

	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(1337);

	inet_pton(AF_INET6, "::", &sin6.sin6_addr);

	if (bind(s6, reinterpret_cast<const sockaddr*>(&sin6), sizeof(sin6)))
	{
		fmt::print("[Error] bind() failed: {}\n", WSAGetLastError());
		return 7;
	}

	unsigned char dht_id[20]{};
	bool write_id_to_disk = true;

	if (std::filesystem::exists("dht_id"))
	{
		FILE* file{};

		write_id_to_disk = false;

		if (fopen_s(&file, "dht_id", "rb") == 0)
		{
			fread(dht_id, 1, sizeof(dht_id), file);
			fclose(file);
		}
		else
		{
			fmt::print("[Error] fopen_s() failed: {}\n", errno);
			return 8;
		}
	}
	else
	{
		dht_random_bytes(dht_id, sizeof(dht_id));
	}

	if (dht_init(s4, s6, dht_id, (unsigned char*)"") < 0)
	{
		fmt::print("[Error] dht_init() failed\n");
		return 9;
	}

	fmt::print("Running...\n");

	for (const auto& bootstrap_info : bootstrap_node_list)
	{
		addrinfo hints{};

		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;

		auto& host = bootstrap_info.first;
		auto port = bootstrap_info.second;
		addrinfo* results{};

		if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &results) != 0)
		{
			auto error_code = WSAGetLastError();

			if (error_code == WSAHOST_NOT_FOUND)
			{
				continue;
			}

			fmt::print("[Error] getaddrinfo() failed: {}\n", error_code);
			return 10;
		}

		for (auto ptr = results; ptr != nullptr; ptr = ptr->ai_next)
		{
			if ((ptr->ai_family == AF_INET && s4 >= 0) || (ptr->ai_family == AF_INET6 && s6 >= 0))
			{
				dht_ping_node(ptr->ai_addr, ptr->ai_addrlen);
	}
		}
	}

	while (true)
	{
		bool searching = false;
		bool dumping = false;
		char buffer[4096]{};
		sockaddr_storage from{};
		socklen_t from_len;
		time_t to_sleep = 0;
		timeval tv{};
		fd_set readfds{};
		tv.tv_sec = to_sleep;
		tv.tv_usec = rand() % 1000000;

		FD_ZERO(&readfds);

		if (s4 >= 0)
		{
			FD_SET(s4, &readfds);
		}

		if (s6 >= 0)
		{
			FD_SET(s6, &readfds);
		}

		auto ret_val = select(s4 > s6 ? s4 + 1 : s6 + 1, &readfds, nullptr, nullptr, &tv);

		if (ret_val < 0)
		{
			auto error_code = WSAGetLastError();

			if (error_code != WSAEINTR)
			{
				fmt::print("[Error] select(): {}\n", error_code);
				Sleep(1000);
			}
		}

		if ((GetAsyncKeyState(0x51) & 0x8000) != 0)
		{
			break;
		}
		else if ((GetAsyncKeyState(0x44) & 0x8000) != 0)
		{
			dumping = true;
		}
		else if ((GetAsyncKeyState(0x53) & 0x8000) != 0)
		{
			searching = true;
		}

		if (ret_val > 0)
		{
			from_len = sizeof(from);

			if (s4 >= 0 && FD_ISSET(s4, &readfds))
			{
				ret_val = recvfrom(s4, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&from), &from_len);
			}
			else if (s6 >= 0 && FD_ISSET(s6, &readfds))
			{
				ret_val = recvfrom(s6, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&from), &from_len);
			}
			else
			{
				fmt::print("[Error] uh-oh...\n");
				return 11;
			}
		}

		if (ret_val > 0)
		{
			buffer[ret_val] = '\0';
			ret_val = dht_periodic(buffer, ret_val, reinterpret_cast<sockaddr*>(&from), from_len, &to_sleep, dht_callback, nullptr);
		}
		else
		{
			ret_val = dht_periodic(nullptr, 0, nullptr, 0, &to_sleep, dht_callback, nullptr);
		}

		if (ret_val < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			else
			{
				fmt::print("[Error] dht_periodic(): {}\n", ret_val);

				if (ret_val == EINVAL || ret_val == EFAULT)
				{
					fmt::print("[Error] Aborting!\n");
					return 12;
				}
				
				to_sleep = 1;
			}
		}

		if (searching)
		{
			if (s4 >= 0)
			{
				dht_search(hash, 0, AF_INET, dht_callback, nullptr);
			}

			if (s6 >= 0)
			{
				dht_search(hash, 0, AF_INET6, dht_callback, nullptr);
			}
		}

		if (dumping)
		{
			dht_dump_tables(stdout);
		}
	}

	auto num4 = 500, num6 = 500;
	auto total = dht_get_nodes(good4, &num4, good6, &num6);

	fmt::print("Found {} ({} + {}) good nodes.\n", total, num4, num6);

	if (dht_uninit() < 0)
	{
		fmt::print("[Error] Failed to uninitialize DHT\n");
		return 13;
	}

	closesocket(s4);
	closesocket(s6);
	WSACleanup();

	if (write_id_to_disk)
	{
		FILE* file{};

		if (fopen_s(&file, "dht_id", "wb") == 0)
		{
			fwrite(dht_id, 1, sizeof(dht_id), file);
			fflush(file);
			fclose(file);
		}
		else
		{
			fmt::print("[Error] fopen_s() failed: {}\n", errno);
			return 14;
		}
	}

	return 0;
}

extern "C" int dht_gettimeofday(struct timeval* tv, struct timezone* tz)
{
	/*
	 * Author: Ugo Varetto - ugovaretto@gmail.com
	 * This code is distributed under the terms of the Apache Software License version 2.0
	 * https://opensource.org/licenses/Apache-2.0
	 */
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag = 0;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		tmpres /= 10;  /*convert into microseconds*/
		/*converting file time to unix epoch*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}

int dht_sendto(int sockfd, const void* buf, int len, int flags, const sockaddr* to, int tolen)
{
	return sendto(sockfd, reinterpret_cast<const char*>(buf), len, flags, to, tolen);
}

int dht_blacklisted(const sockaddr* sa, int salen)
{
	return 0;
}

void dht_hash(void* hash_return, int hash_size, const void* v1, int len1, const void* v2, int len2, const void* v3, int len3)
{
	hash_state _hash_state{};

	if (hash_size > 20)
	{
		memset(reinterpret_cast<char*>(hash_return) + 20, 0, hash_size - 20);
	}

	sha1_init(&_hash_state);
	sha1_process(&_hash_state, reinterpret_cast<const unsigned char*>(v1), len1);
	sha1_process(&_hash_state, reinterpret_cast<const unsigned char*>(v2), len2);
	sha1_process(&_hash_state, reinterpret_cast<const unsigned char*>(v3), len3);
	sha1_done(&_hash_state, reinterpret_cast<unsigned char*>(hash_return));
}

int dht_random_bytes(void* buf, size_t size)
{
	return yarrow_read(reinterpret_cast<unsigned char*>(buf), size, &_prng_state);
}