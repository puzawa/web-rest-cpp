#include "tcp_server.hpp"

#include <algorithm>
#include <cstring>

namespace net {


	NetInitializer::NetInitializer() {
#ifdef _WIN32
		WSADATA wsa_data{};
		int result = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
		if (result != 0) {
			throw std::runtime_error("WSAStartup failed");
		}
#else

#endif
	}

	NetInitializer::~NetInitializer() {
#ifdef _WIN32
		::WSACleanup();
#endif
	}


	ThreadPool::ThreadPool(std::size_t thread_count, std::size_t max_queue_size)
		: max_queue_size_(max_queue_size ? max_queue_size : 1) // avoid 0
	{
		if (thread_count == 0) {
			thread_count = 1;
		}
		for (std::size_t i = 0; i < thread_count; ++i) {
			workers_.emplace_back([this] { worker_loop(); });
		}
	}

	ThreadPool::~ThreadPool() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			stop_ = true;
		}
		cv_jobs_.notify_all();
		cv_space_.notify_all();

		for (auto& t : workers_) {
			if (t.joinable()) {
				t.join();
			}
		}
	}

	bool ThreadPool::try_enqueue(std::function<void()> job) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (stop_) return false;
		if (jobs_.size() >= max_queue_size_) {
			return false;
		}
		jobs_.push(std::move(job));
		cv_jobs_.notify_one();
		return true;
	}

	void ThreadPool::enqueue(std::function<void()> job) {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			cv_space_.wait(lock, [this] {
				return stop_ || jobs_.size() < max_queue_size_;
				});
			if (stop_) return;
			jobs_.push(std::move(job));
		}
		cv_jobs_.notify_one();
	}

	void ThreadPool::worker_loop() {
		while (true) {
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock(mutex_);
				cv_jobs_.wait(lock, [this] {
					return stop_ || !jobs_.empty();
					});
				if (stop_ && jobs_.empty()) {
					return;
				}
				job = std::move(jobs_.front());
				jobs_.pop();
				cv_space_.notify_one();
			}
			job();
		}
	}


	TcpConnection::TcpConnection(socket_t sock, sockaddr_storage addr, socklen_t addr_len)
		: sock_(sock), remote_addr_(addr), remote_addr_len_(addr_len) {
	}

	TcpConnection::~TcpConnection() {
		close();
	}

	bool TcpConnection::is_valid() const {
		return sock_ != invalid_socket;
	}

	void TcpConnection::close() {
		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (sock_ != invalid_socket) {
#ifdef _WIN32
			::shutdown(sock_, SD_BOTH);
#else
			::shutdown(sock_, SHUT_RDWR);
#endif
			close_socket(sock_);
			sock_ = invalid_socket;
		}
	}

	std::size_t TcpConnection::send(const void* data, std::size_t size) {
		const char* buf = static_cast<const char*>(data);
		std::size_t total_sent = 0;

		std::lock_guard<std::mutex> lock(socket_mutex_);
		while (total_sent < size) {
#ifdef _WIN32
			int sent = ::send(sock_, buf + total_sent,
				static_cast<int>(size - total_sent), 0);
#else
			ssize_t sent = ::send(sock_, buf + total_sent,
				size - total_sent, 0);
#endif
			if (sent <= 0) {
				break;
			}
			total_sent += static_cast<std::size_t>(sent);
		}
		return total_sent;
	}

	std::size_t TcpConnection::recv(void* buffer, std::size_t size) {
		std::lock_guard<std::mutex> lock(socket_mutex_);
#ifdef _WIN32
		int received = ::recv(sock_, static_cast<char*>(buffer),
			static_cast<int>(size), 0);
#else
		ssize_t received = ::recv(sock_, buffer, size, 0);
#endif
		if (received <= 0) {
			return 0;
		}
		return static_cast<std::size_t>(received);
	}

	std::string TcpConnection::remote_address() const {
		char host[NI_MAXHOST] = {};
		char serv[NI_MAXSERV] = {};

		if (getnameinfo(reinterpret_cast<const sockaddr*>(&remote_addr_),
			remote_addr_len_,
			host, sizeof(host),
			serv, sizeof(serv),
			NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
			return {};
		}
		return std::string(host);
	}

	std::uint16_t TcpConnection::remote_port() const {
		if (remote_addr_.ss_family == AF_INET) {
			auto* addr = reinterpret_cast<const sockaddr_in*>(&remote_addr_);
			return ntohs(addr->sin_port);
		}
		else if (remote_addr_.ss_family == AF_INET6) {
			auto* addr = reinterpret_cast<const sockaddr_in6*>(&remote_addr_);
			return ntohs(addr->sin6_port);
		}
		return 0;
	}

	void TcpConnection::set_timeout_ms(int ms) {
		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (sock_ == invalid_socket) return;

#ifdef _WIN32
		int tv = ms;
		::setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
			reinterpret_cast<const char*>(&tv), sizeof(tv));
		::setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO,
			reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
		timeval tv{};
		tv.tv_sec = ms / 1000;
		tv.tv_usec = (ms % 1000) * 1000;
		::setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
			&tv, sizeof(tv));
		::setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO,
			&tv, sizeof(tv));
#endif
	}


	TcpServer::TcpServer(const std::string& bind_address,
		std::uint16_t port,
		ConnectionHandler handler,
		std::size_t thread_count,
		std::size_t max_queue_size)
		: net_init_()
		, bind_address_(bind_address)
		, port_(port)
		, handler_(std::move(handler))
		, pool_(thread_count, max_queue_size) {
	}

	TcpServer::~TcpServer() {
		stop();
	}

	bool TcpServer::is_running() const {
		return running_.load();
	}

	void TcpServer::start() {
		if (running_.exchange(true)) {
			return;
		}

		listen_sock_ = ::socket(AF_INET6, SOCK_STREAM, 0);
		if (listen_sock_ == invalid_socket) {
			running_ = false;
			throw std::runtime_error("Failed to create IPv6 socket");
		}

		int no = 0;
		::setsockopt(listen_sock_, IPPROTO_IPV6, IPV6_V6ONLY,
			reinterpret_cast<const char*>(&no), sizeof(no));

		int opt = 1;
		::setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
			reinterpret_cast<const char*>(&opt), sizeof(opt));

		sockaddr_in6 addr6{};
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(port_);
		addr6.sin6_addr = in6addr_any;

		bool bound = false;

		if (!bind_address_.empty() && bind_address_ != "::" && bind_address_ != "0.0.0.0") {
			if (inet_pton(AF_INET6, bind_address_.c_str(), &addr6.sin6_addr) == 1) {
				if (::bind(listen_sock_,
					reinterpret_cast<sockaddr*>(&addr6),
					sizeof(addr6)) == 0) {
					bound = true;
				}
			}
			if (!bound) {
				close_socket(listen_sock_);
				listen_sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
				if (listen_sock_ == invalid_socket) {
					running_ = false;
					throw std::runtime_error("Failed to create IPv4 socket");
				}
				::setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
					reinterpret_cast<const char*>(&opt), sizeof(opt));

				sockaddr_in addr4{};
				addr4.sin_family = AF_INET;
				addr4.sin_port = htons(port_);
				if (inet_pton(AF_INET, bind_address_.c_str(), &addr4.sin_addr) != 1) {
					close_socket(listen_sock_);
					listen_sock_ = invalid_socket;
					running_ = false;
					throw std::runtime_error("Invalid bind address");
				}
				if (::bind(listen_sock_,
					reinterpret_cast<sockaddr*>(&addr4),
					sizeof(addr4)) != 0) {
					close_socket(listen_sock_);
					listen_sock_ = invalid_socket;
					running_ = false;
					throw std::runtime_error("bind() failed");
				}
				bound = true;
			}
		}
		else {
			if (::bind(listen_sock_,
				reinterpret_cast<sockaddr*>(&addr6),
				sizeof(addr6)) != 0) {
				close_socket(listen_sock_);
				listen_sock_ = invalid_socket;
				running_ = false;
				throw std::runtime_error("bind() failed");
			}
			bound = true;
		}

		if (!bound) {
			close_socket(listen_sock_);
			listen_sock_ = invalid_socket;
			running_ = false;
			throw std::runtime_error("bind() failed");
		}

		if (::listen(listen_sock_, SOMAXCONN) != 0) {
			close_socket(listen_sock_);
			listen_sock_ = invalid_socket;
			running_ = false;
			throw std::runtime_error("listen() failed");
		}

		accept_thread_ = std::thread(&TcpServer::accept_loop, this);
	}

	void TcpServer::stop() {
		if (!running_.exchange(false)) {
			return;
		}

		if (listen_sock_ != invalid_socket) {
			close_socket(listen_sock_);
			listen_sock_ = invalid_socket;
		}

		if (accept_thread_.joinable()) {
			accept_thread_.join();
		}
	}

	void TcpServer::accept_loop() {
		while (running_) {
			sockaddr_storage client_addr{};
			socklen_t addr_len = sizeof(client_addr);

			socket_t client_sock = ::accept(listen_sock_,
				reinterpret_cast<sockaddr*>(&client_addr),
				&addr_len);
			if (client_sock == invalid_socket) {
				if (!running_) {
					break;
				}
				continue;
			}

			auto conn = std::make_shared<TcpConnection>(client_sock, client_addr, addr_len);

			bool ok = pool_.try_enqueue([this, conn]() {
				handler_(conn);
				conn->close();
				});

			if (!ok) {
				conn->close();
			}
		}
	}

}
