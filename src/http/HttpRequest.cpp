#include "HttpRequest.h"
#include <algorithm>
#include <iostream>
#include <regex>
#include <string>
#include "../util/Util.h"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML = {
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "/picture",
};

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
    while (buffer.readableBytes() && state_ != PARSE_STATE::FINISH) {
        // return buffer.beginWriteConst() if does not find the END_FLAG
        auto lineEnd = std::search(buffer.peek(), buffer.beginWriteConst(), util::ENDLINE_FLAG.begin(), util::ENDLINE_FLAG.end());
        std::string line(buffer.retrieveUntilToString(lineEnd));
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
    return true;
}

void HttpRequest::parsePath() {
    if (path_ == "/") {
        path_ = "/index.html";
    }
    std::cout<<"path: "<<path_<<std::endl;
}

bool HttpRequest::parseRequestLine(const std::string &line) {
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, pattern)) {
        // 首个sub_match是整个字符串
        // 下个sub_match是首个有括号表达式
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
    state_ = PARSE_STATE::FINISH;
}