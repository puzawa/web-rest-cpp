#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>

#include "models.hpp" 

class LocalUserRepository {
public:
   
    std::string create_session(const std::string& login) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string token = generate_token();
        sessions_[token] = login;
        return token;
    }

    void remove_session(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(token);
    }

    void remove_user(const std::string& login) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = sessions_.begin(); it != sessions_.end(); ) {
            if (it->second == login) {
                it = sessions_.erase(it);
            }
            else {
                ++it;
            }
        }
        user_dots_cache_.erase(login);
    }

    std::string get_login_by_token(const std::string& token) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(token);
        if (it == sessions_.end()) return {};
        return it->second;
    }

    void set_dots(const std::string& login, const std::vector<DotView>& dots) {
        std::lock_guard<std::mutex> lock(mutex_);
        user_dots_cache_[login] = dots;
    }

    std::vector<DotView> get_dots(const std::string& login) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = user_dots_cache_.find(login);
        if (it == user_dots_cache_.end()) return {};
        return it->second;
    }

    void add_dot(const std::string& login, const DotView& dot) {
        std::lock_guard<std::mutex> lock(mutex_);
        user_dots_cache_[login].push_back(dot);
    }

    void clear_dots(const std::string& login) {
        std::lock_guard<std::mutex> lock(mutex_);
        user_dots_cache_[login].clear();
    }

private:
  
    std::string generate_token() {
        static std::mt19937_64 rng(std::random_device{}());
        static std::uniform_int_distribution<unsigned long long> dist;

        std::ostringstream oss;
        oss << std::hex << dist(rng) << dist(rng);
        return oss.str();
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> sessions_;               
    std::unordered_map<std::string, std::vector<DotView>> user_dots_cache_;
};
