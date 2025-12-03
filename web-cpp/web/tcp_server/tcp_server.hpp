#pragma once

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  define _WINSOCK_DEPRECATED_NO_WARNINGS
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "Ws2_32.lib")
#else
#  include <arpa/inet.h>
#  include <errno.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

namespace net {

#ifdef _WIN32
	using socket_t = SOCKET;
	constexpr socket_t invalid_socket = INVALID_SOCKET;
	inline void close_socket(socket_t s) {
		if (s != invalid_socket) {
			::closesocket(s);
		}
	}
#else
	using socket_t = int;
	constexpr socket_t invalid_socket = -1;
	inline void close_socket(socket_t s) {
		if (s != invalid_socket) {
			::close(s);
		}
	}
#endif

	class NetInitializer {
	public:
		NetInitializer();
		~NetInitializer();

		NetInitializer(const NetInitializer&) = delete;
		NetInitializer& operator=(const NetInitializer&) = delete;
	};


	class ThreadPool {
	public:
		ThreadPool(std::size_t thread_count, std::size_t max_queue_size = 1024);
		~ThreadPool();

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		bool try_enqueue(std::function<void()> job);

		void enqueue(std::function<void()> job);

	private:
		void worker_loop();

		std::vector<std::thread> workers_;
		std::queue<std::function<void()>> jobs_;
		std::mutex mutex_;
		std::condition_variable cv_jobs_;
		std::condition_variable cv_space_;
		std::size_t max_queue_size_;
		bool stop_ = false;
	};


	class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
	public:
		~TcpConnection();

		TcpConnection(const TcpConnection&) = delete;
		TcpConnection& operator=(const TcpConnection&) = delete;

		TcpConnection(socket_t sock, sockaddr_storage addr, socklen_t addr_len);

		std::size_t send(const void* data, std::size_t size);

		std::size_t recv(void* buffer, std::size_t size);

		void close();

		bool is_valid() const;

		std::string remote_address() const;
		std::uint16_t remote_port() const;

		void set_timeout_ms(int ms);

	private:
		friend class TcpServer;

		socket_t sock_ = invalid_socket;
		sockaddr_storage remote_addr_{};
		socklen_t remote_addr_len_ = 0;
		mutable std::mutex socket_mutex_;
	};


	class TcpServer {
	public:
		using ConnectionHandler = std::function<void(std::shared_ptr<TcpConnection>)>;

		TcpServer(const std::string& bind_address,
			std::uint16_t port,
			ConnectionHandler handler,
			std::size_t thread_count = std::thread::hardware_concurrency(),
			std::size_t max_queue_size = 1024);

		~TcpServer();

		TcpServer(const TcpServer&) = delete;
		TcpServer& operator=(const TcpServer&) = delete;

		void start();
		void stop();

		bool is_running() const;

	private:
		void accept_loop();

		NetInitializer net_init_;
		std::string bind_address_;
		std::uint16_t port_;
		ConnectionHandler handler_;

		socket_t listen_sock_ = invalid_socket;
		std::atomic<bool> running_{ false };
		std::thread accept_thread_;
		ThreadPool pool_;
	};

}
