#include "HttpRequest.h"
#include <algorithm>
#include <glog/logging.h>
#include <iostream>
#include <regex>
#include <string>
#include "../util/Util.h"

HttpRequest::HttpRequest() :
    state_(PARSE_STATE::REQUEST_LINE) {
}

void HttpRequest::init() {
    method_ = "";
    path_ = "";
    version_ = "";
    body_ = "";
    state_ = PARSE_STATE::REQUEST_LINE;
    header_.clear();
    postParameter_.clear();
}

std::string HttpRequest::getMethod() const {
    return method_;
}

std::string HttpRequest::getPath() const {
    return path_;
}

std::string &HttpRequest::getPath() {
    return path_;
}

std::string HttpRequest::getVersion() const {
    return version_;
}

std::unordered_map<std::string, std::string> &HttpRequest::getHeader() {
    return header_;
}

std::unordered_map<std::string, std::string> &HttpRequest::getPostParameter() {
    return postParameter_;
}

bool HttpRequest::isKeepAlive() const {
    auto result = header_.find("Connection");
    if (result != header_.end()) {
        return result->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer &buffer) {
    if (buffer.readableBytes() <= 0) {
        return false;
    }
    while (buffer.readableBytes() > 0 && state_ != PARSE_STATE::FINISH) {
        // return buffer.beginWriteConst() if does not find the END_FLAG
        auto lineEnd = std::search(buffer.peek(), buffer.beginWriteConst(), util::ENDLINE_FLAG.begin(), util::ENDLINE_FLAG.end());
        std::string line(buffer.retrieveUntilToString(lineEnd));
        LOG(INFO) << "current line: " << line;

        switch (state_) {
        case PARSE_STATE::REQUEST_LINE:
            if (!parseRequestLine(line)) {
                return false;
            }
            parsePath();
            break;
        case PARSE_STATE::HEADER:
            parseHeader(line);
            if (buffer.readableBytes() <= util::ENDLINE_FLAG.size()) {
                state_ = PARSE_STATE::FINISH;
            }
            break;
        case PARSE_STATE::BODY:
            parseBody(line);
            break;
        default:
            break;
        }
        if (lineEnd == buffer.beginWriteConst()) break;
        buffer.retrieve(util::ENDLINE_FLAG.size());
    }
    LOG(INFO) << "[method]:" << method_ << ", [path]:" << path_ << ", [version]:" << version_;
    return true;
}

void HttpRequest::parsePath() {
    if (path_ == "/") {
        path_ = "/index.html";
    }
}

bool HttpRequest::parseRequestLine(const std::string &line) {
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, pattern)) {
        // sub_match[0]是整个字符串
        // 之后, sub_match[i]是第i个()括号表达式内的字符串
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = PARSE_STATE::HEADER;
        return true;
    }
    return false;
}
void HttpRequest::parseHeader(const std::string &line) {
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, pattern)) {
        header_[subMatch[1]] = subMatch[2];
    } else {
        state_ = PARSE_STATE::BODY;
    }
}

void HttpRequest::parseBody(const std::string &line) {
    body_ = line;
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        parseParameter();
        path_ = "/index.html";
    }
    state_ = PARSE_STATE::FINISH;
    LOG(INFO) << "[body]:" << body_ << ", [len]:" << body_.size();
}

inline int hexToDex(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A' + 10;
    }
    if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 10;
    }
    return ch - '0';
}
void HttpRequest::parseParameter() {
    LOG(INFO) << "parse post method parameter start.";
    if (body_.empty()) {
        return;
    }

    std::string key;
    std::string value;
    std::string tmp;
    int split;
    int last = 0;
    for (int i = 0; i < body_.size(); ++i) {
        switch (body_[i]) {
        case '+':
            tmp += ' ';
            break;
        case '%':
            tmp += static_cast<char>(hexToDex(body_[i + 1]) * 16 + hexToDex(body_[i + 2]));
            i += 2;
            break;
        case '=':
            split = tmp.size();
            break;
        case '&':
            key = tmp.substr(0, split);
            value = tmp.substr(split);
            postParameter_[key] = value;
            LOG(INFO) << "[key]:" << key << ", [value]:" << value;
            tmp.clear();
            break;
        default:
            tmp += body_[i];
        }
    }
    if (!tmp.empty()) {
        key = tmp.substr(0, split);
        value = tmp.substr(split);
        postParameter_[key] = value;
        LOG(INFO) << "[key]:" << key << ", [value]:" << value;
        tmp.clear();
    }

    LOG(INFO) << "parse post method parameterss end.";
}