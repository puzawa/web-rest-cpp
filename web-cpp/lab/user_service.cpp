#include "user_service.hpp"


Result<AuthResult> UserService::login(const std::string& login, const std::string& password) {
    try {
        if (!db_.db_check_password(login, password)) {
            return Result<AuthResult>::failure(UserError::InvalidCredentials);
        }

        auto dots = db_.db_get_dots(login);
        local_.set_dots(login, dots);
        std::string token = local_.create_session(login);

        AuthResult ar{ token, std::move(dots) };
        return Result<AuthResult>::success(std::move(ar));
    }
    catch (...) {
        return Result<AuthResult>::failure(UserError::DbError);
    }
}

Result<AuthResult> UserService::register_user(const std::string& login, const std::string& password) {
    try {
        if (!db_.db_create_user(login, password)) {
            return Result<AuthResult>::failure(UserError::UserAlreadyExists);
        }

        local_.set_dots(login, {});
        std::string token = local_.create_session(login);

        AuthResult ar{ token, {} };
        return Result<AuthResult>::success(std::move(ar));
    }
    catch (...) {
        return Result<AuthResult>::failure(UserError::DbError);
    }
}

ResultVoid UserService::logout(const std::string& token) {
    local_.remove_session(token);
    return ResultVoid::success(Unit{});
}

ResultVoid UserService::remove_user_by_login(const std::string& login) {
    try {
        bool ok = db_.db_delete_user(login);
        if (!ok) {
            return ResultVoid::failure(UserError::UserNotFound);
        }
        local_.remove_user(login);
        return ResultVoid::success(Unit{});
    }
    catch (...) {
        return ResultVoid::failure(UserError::DbError);
    }
}

std::string UserService::login_from_token(const std::string& token) const {
    return local_.get_login_by_token(token);
}

Result<DotView> UserService::add_dot(const std::string& login, const DotView& dot) {
    try {
        local_.add_dot(login, dot);
        db_.push_task(DbTask{ login, dot });
        return Result<DotView>::success(dot);
    }
    catch (...) {
        return Result<DotView>::failure(UserError::DbError);
    }
}

ResultVoid UserService::clear_dots(const std::string& login) {
    try {
        db_.db_clear_dots(login);
        local_.clear_dots(login);
        return ResultVoid::success(Unit{});
    }
    catch (...) {
        return ResultVoid::failure(UserError::DbError);
    }
}

Result<std::vector<DotView>> UserService::get_dots(const std::string& login) {
    auto dots = local_.get_dots(login);
    if (!dots.empty()) {
        return Result<std::vector<DotView>>::success(std::move(dots));
    }

    try {
        dots = db_.db_get_dots(login);
        local_.set_dots(login, dots);
        return Result<std::vector<DotView>>::success(std::move(dots));
    }
    catch (...) {
        return Result<std::vector<DotView>>::failure(UserError::DbError);
    }
}
