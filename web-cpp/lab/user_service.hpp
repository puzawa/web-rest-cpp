#pragma once

#include <optional>
#include <string>
#include <vector>

#include "db_user_repo.hpp"
#include "local_user_repo.hpp"

enum class UserError {
    None,
    InvalidCredentials,
    UserAlreadyExists,  
    UserNotFound,
    Unauthorized,
    DbError
};

template <typename T>
struct Result {
    std::optional<T> value;
    UserError error{ UserError::None };

    bool ok() const { return error == UserError::None; }

    static Result success(T v) {
        Result r;
        r.value = std::move(v);
        r.error = UserError::None;
        return r;
    }

    static Result failure(UserError e) {
        Result r;
        r.error = e;
        return r;
    }
};

struct Unit {};

using ResultVoid = Result<Unit>;


struct AuthResult {
    std::string token;
    std::vector<DotView> dots;
};

class UserService {
public:
    UserService(DbUserRepository& db, LocalUserRepository& local)
        : db_(db), local_(local) {
    }

    Result<AuthResult> login(const std::string& login, const std::string& password);
    Result<AuthResult> register_user(const std::string& login, const std::string& password);

    ResultVoid logout(const std::string& token);
    ResultVoid remove_user_by_login(const std::string& login);

    std::string login_from_token(const std::string& token) const;

    Result<DotView> add_dot(const std::string& login, const DotView& dot);
    ResultVoid clear_dots(const std::string& login);
    Result<std::vector<DotView>> get_dots(const std::string& login);

private:
    DbUserRepository& db_;
    LocalUserRepository& local_;
};
