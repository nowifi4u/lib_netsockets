#ifndef LIB_SOCKET_H
#define LIB_SOCKET_H

#if defined (_MSC_VER)
#ifndef _CRT_SECURE_NO_WARNINGS

#define LIB_SOCKET_CRT_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#endif
#endif

#if defined (_MSC_VER)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> //hostent
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#endif
#include <iostream>
#include <cerrno>
#include <string>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <ctime>

namespace netsockets {

	static const int MAXPENDING = 5; // maximum outstanding connection requests

	//multi platform socket descriptor
#if _WIN32
	typedef SOCKET socketfd_t;
#else
	typedef int socketfd_t;
#endif

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	//str_extract()
	//extract last component of file full path
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	inline std::string str_extract(const std::string& str_in)
	{
		size_t pos = str_in.find_last_of("/\\");
		std::string str = str_in.substr(pos + 1, str_in.size());
		return str;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	//prt_time
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	inline std::string prt_time()
	{
		time_t t;
		time(&t);
		std::string str_time(std::ctime(&t));
		str_time.resize(str_time.size() - 1); //remove \n
		str_time += " ";
		return str_time;
	}

	///////////////////////////////////////////////////////////////////////////////////////
	//set_daemon
	///////////////////////////////////////////////////////////////////////////////////////

	inline int set_daemon(const char* str_dir)
	{
#if defined (_MSC_VER)
		std::cout << str_dir << std::endl;
#else
		pid_t pid, sid;
		pid = fork();
		if (pid < 0)
		{
			std::cout << "cannot create pid: " << pid << std::endl;
			exit(EXIT_FAILURE);
		}
		if (pid > 0)
		{
			std::cout << "created pid: " << pid << std::endl;
			exit(EXIT_SUCCESS);
		}

		umask(0);

		sid = setsid();
		if (sid < 0)
		{
			std::cout << "cannot create sid: " << sid << std::endl;
			exit(EXIT_FAILURE);
		}

		if (str_dir)
		{
			if ((chdir(str_dir)) < 0)
			{
				std::cout << "cannot chdir to: " << str_dir << std::endl;
				exit(EXIT_FAILURE);
			}
			std::cout << "chdir to: " << str_dir << std::endl;
		}
#endif
		return 0;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	//wait()
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	inline void wait(int nbr_secs)
	{
#ifdef _MSC_VER
		Sleep(1000 * nbr_secs);
#else
		sleep(nbr_secs);
#endif
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	//socket_t
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	class socket_t
	{
	public:

		socket_t() : m_sockfd(0)
		{
			memset(&m_sockaddr_in, 0, sizeof(m_sockaddr_in));
		}

		socket_t(socketfd_t sockfd, sockaddr_in sock_addr) :
			m_sockfd(sockfd),
			m_sockaddr_in(sock_addr)
		{
		}

		void close()
		{
#if defined (_MSC_VER)
			::closesocket(m_sockfd);
#else
			::close(m_sockfd);
#endif
			//clear members
			memset(&m_sockaddr_in, 0, sizeof(m_sockaddr_in));
			m_sockfd = 0;
		}

		int write_all(const void* _buf, int size_buf)
		{
			const char* buf = static_cast<const char*>(_buf); // can't do pointer arithmetic on void* 
			int sent_size; // size in bytes sent or -1 on error 
			int size_left; // size in bytes left to send 
			const int flags = 0;
			size_left = size_buf;
			while (size_left > 0)
			{
				sent_size = ::send(m_sockfd, buf, size_left, flags);
				if (-1 == sent_size)
				{
					std::cout << "send error: " << strerror(errno) << std::endl;
					return -1;
				}
				size_left -= sent_size;
				buf += sent_size;
			}
			return 1;
		}

		int write_all(const std::string& str)
		{
			return write_all(str.c_str(), str.length());
		}

		int read_all(void* _buf, int size_buf)
		{
			char* buf = static_cast<char*>(_buf); // can't do pointer arithmetic on void* 
			int recv_size; // size in bytes received or -1 on error 
			int size_left; // size in bytes left to send 
			const int flags = 0;
			int total_recv_size = 0;
			size_left = size_buf;
			while (size_left > 0)
			{
				recv_size = ::recv(m_sockfd, buf, size_left, flags);
				if (-1 == recv_size)
				{
					std::cout << "recv error: " << strerror(errno) << std::endl;
				}
				//everything received, exit
				if (0 == recv_size)
				{
					break;
				}
				size_left -= recv_size;
				buf += recv_size;
				total_recv_size += recv_size;
			}
			return total_recv_size;
		}

		int hostname_to_ip(const char* host_name, char* ip)
		{
			struct addrinfo hints, * servinfo, * p;
			struct sockaddr_in* h;
			int rv;

			memset(&hints, 0, sizeof hints);
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			if ((rv = getaddrinfo(host_name, "http", &hints, &servinfo)) != 0)
			{
				return 1;
			}

			for (p = servinfo; p != NULL; p = p->ai_next)
			{
				h = (struct sockaddr_in*)p->ai_addr;
				strcpy(ip, inet_ntoa(h->sin_addr));
			}

			freeaddrinfo(servinfo);
			return 0;
		}

	public:
		socketfd_t m_sockfd; //socket descriptor 
		sockaddr_in m_sockaddr_in; //client address (used to store return value of server accept())
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	//tcp_server_t
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	class tcp_server_t : public socket_t
	{
	public:

		tcp_server_t(const unsigned short server_port)
			: socket_t()
		{
#if defined (_MSC_VER)
			WSADATA ws_data;
			if (WSAStartup(MAKEWORD(2, 0), &ws_data) != 0)
			{
				exit(1);
			}
#endif
			sockaddr_in server_addr; // local address

			// create TCP socket for incoming connections
			if ((m_sockfd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			{
				std::cout << "socket error: " << std::endl;
				exit(1);
			}

			//allow socket descriptor to be reuseable
			int on = 1;
			if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0)
			{
				std::cout << "setsockopt error: " << std::endl;
				exit(1);
			}

			// construct local address structure
			memset(&server_addr, 0, sizeof(server_addr));     // zero out structure
			server_addr.sin_family = AF_INET;                 // internet address family
			server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // any incoming interface
			server_addr.sin_port = htons(server_port);        // local port

			// bind to the local address
			if (::bind(m_sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0)
			{
				//bind error: Permission denied
				//probably trying to bind a port under 1024. These ports usually require root privileges to be bound.
				std::cout << "bind error: " << std::endl;
				exit(1);
			}

			// mark the socket so it will listen for incoming connections
			if (::listen(m_sockfd, MAXPENDING) < 0)
			{
				std::cout << "listen error: " << std::endl;
				exit(1);
			}

		}

		socket_t accept()
		{
			sockaddr_in addr_client; // client address
			socketfd_t fd; //socket descriptor 
#if defined (_MSC_VER)
			int len_addr; // length of client address data structure
#else
			socklen_t len_addr;
#endif

			// set length of client address structure (in-out parameter)
			len_addr = sizeof(addr_client);

			// wait for a client to connect
			if ((fd = ::accept(m_sockfd, (struct sockaddr*)&addr_client, &len_addr)) < 0)
			{
				std::cout << "accept error " << std::endl;
			}

			socket_t socket(fd, addr_client);
			return socket;
		}

		~tcp_server_t()
		{
#if defined (_MSC_VER)
			WSACleanup();
#endif
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	//tcp_client_t
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	class tcp_client_t : public socket_t
	{
	public:

		tcp_client_t()
			: socket_t()
		{
#if defined (_MSC_VER)
			WSADATA ws_data;
			if (WSAStartup(MAKEWORD(2, 0), &ws_data) != 0)
			{
				exit(1);
			}
#endif
		}

		~tcp_client_t()
		{
#if defined (_MSC_VER)
			WSACleanup();
#endif
		}

		int connect()
		{
			struct sockaddr_in server_addr; // server address

			// create a stream socket using TCP
			if ((m_sockfd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			{
				std::cout << "socket error: " << std::endl;
				return -1;
			}

			// construct the server address structure
			memset(&server_addr, 0, sizeof(server_addr)); // zero out structure
			server_addr.sin_family = AF_INET; // internet address family
			if (inet_pton(AF_INET, m_server_ip.c_str(), &server_addr.sin_addr) <= 0) // server IP address
			{
				std::cout << "inet_pton error: " << strerror(errno) << std::endl;
				return -1;
			}
			server_addr.sin_port = htons(m_server_port); // server port

			// establish the connection to the server
			if (::connect(m_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
			{
				std::cout << "connect error: " << strerror(errno) << std::endl;
				return -1;
			}
			return 0;
		}

		tcp_client_t(const char* host_name, const unsigned short server_port)
			: socket_t(),
			m_server_port(server_port)
		{
#if defined (_MSC_VER)
			WSADATA ws_data;
			if (WSAStartup(MAKEWORD(2, 0), &ws_data) != 0)
			{
				exit(1);
			}
#endif

			char server_ip[100];

			//get ip address from hostname
			hostname_to_ip(host_name, server_ip);

			//store
			m_server_ip = server_ip;
		}

		int connect(const char* host_name, const unsigned short server_port)
		{
			struct sockaddr_in server_addr; // server address
			char server_ip[100];

			//get ip address from hostname
			hostname_to_ip(host_name, server_ip);

			//store
			m_server_ip = server_ip;
			m_server_port = server_port;

			// create a stream socket using TCP
			if ((m_sockfd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			{
				std::cout << "socket error: " << std::endl;
				return -1;
			}

			// construct the server address structure
			memset(&server_addr, 0, sizeof(server_addr)); // zero out structure
			server_addr.sin_family = AF_INET; // internet address family
			if (inet_pton(AF_INET, m_server_ip.c_str(), &server_addr.sin_addr) <= 0) // server IP address
			{
				std::cout << "inet_pton error: " << strerror(errno) << std::endl;
				return -1;
			}
			server_addr.sin_port = htons(m_server_port); // server port

			// establish the connection to the server
			if (::connect(m_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
			{
				std::cout << "connect error: " << strerror(errno) << std::endl;
				return -1;
			}
			return 0;
		}

	protected:
		std::string m_server_ip;
		unsigned short m_server_port;
	};

} // namespace netsockets

#ifdef LIB_SOCKET_CRT_NO_WARNINGS
#undef _CRT_SECURE_NO_WARNINGS
#endif

#endif