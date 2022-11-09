#include "HttpResponse.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include "../util/Util.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/msword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css "},
    {".js", "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {206, "Partial Content"},
    {301, "Moved Permanently"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

HttpResponse::HttpResponse() :
    code_{-1}, isKeepAlive_{false}, mmFile_{nullptr}, mmFileStat_{0} {
}
HttpResponse::~HttpResponse() {
    unmapFile();
}

char *HttpResponse::getFile() {
    return mmFile_;
}

size_t HttpResponse::getFileLength() {
    return mmFileStat_.st_size;
}

void HttpResponse::init(const std::string &srcDir, std::string &path, std::unordered_map<std::string, std::string> &header, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if (mmFile_) unmapFile();
    this->srcDir_ = srcDir;
    this->header_ = std::move(header);
    this->path_ = path;
    this->code_ = code;
    this->isKeepAlive_ = isKeepAlive;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
    begin_ = -1;
    end_ = -1;
}

void HttpResponse::makeResponse(Buffer &buffer, bool needRedirect) {
    // check whether the file exists
    // function stat() save file infomation in the specified stat according to the file name
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    } // file authority
    else if (needRedirect) {
        code_ = 301;
    } else if (!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;
    } else if (header_.find("Range") != header_.end()) {
        code_ = 206;
        std::string sub = header_["Range"].substr(6);
        auto divide = sub.find_first_of('-');
        begin_ = std::stoi(sub.substr(0, divide));
        if (divide < sub.size() - 1) {
            end_ = std::stoi(sub.substr(divide + 1));
        }
        std::cout << "begin:" << begin_ << std::endl;
        std::cout << "end:" << end_ << std::endl;
    } else if (code_ == -1) { // OK
        code_ = 200;
    }
    std::cout << code_ << std::endl;
    addStateLine(buffer);
    addHeader(buffer);
    if (needRedirect) {
        buffer.append("Location: https://127.0.0.1").append(path_).append(util::ENDLINE_FLAG);
    }
    addContent(buffer);
}

void HttpResponse::unmapFile() {
    if (mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

void HttpResponse::addStateLine(Buffer &buffer) {
    std::string status;
    auto result = CODE_STATUS.find(code_);
    if (result != CODE_STATUS.end()) {
        status = result->second;
    } else { // ignore other status
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buffer.append("HTTP/1.1 " + std::to_string(code_) + " " + status + util::ENDLINE_FLAG);
}

void HttpResponse::addHeader(Buffer &buffer) {
    buffer.append("Connection: ");
    if (isKeepAlive_) {
        buffer.append("keep-alive" + util::ENDLINE_FLAG);
        buffer.append("keep-alive: max=6, timeout=120" + util::ENDLINE_FLAG);
    } else {
        buffer.append("close" + util::ENDLINE_FLAG);
    }
    buffer.append("Content-type: " + getFileType() + util::ENDLINE_FLAG);
}

void HttpResponse::addContent(Buffer &buffer) {
    // file descriptor
    // return -1 if fail
    std::cout << "file path " << (srcDir_ + path_) << std::endl;
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0) {
        errorContent(buffer, "File not found!");
        return;
    }

    // 将文件映射到内存提高文件的访问速度
    // MAP_PRIVATE 建立一个写入时拷贝的私有映射
    std::cout << "file path " << (srcDir_ + path_) << std::endl;
    auto mmRet = (int *)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    std::cout << mmRet << std::endl;
    std::cout << *mmRet << std::endl;
    std::cout << "here" << std::endl;

    if (*mmRet == -1) {
        std::cout << "File not found!" << std::endl;
        errorContent(buffer, "File not found!");
        return;
    }
    std::cout << "File found!" << std::endl;
    mmFile_ = (char *)mmRet;
    std::cout << "close fd" << std::endl;
    close(srcFd);
    std::cout << "append" << std::endl;

    auto length = mmFileStat_.st_size;

    std::cout << "begin:" << begin_ << std::endl;
        std::cout << "end:" << end_ << std::endl;
    if (begin_ != -1 && end_ != -1) {
        length = end_ - begin_ + 1;
    } else if (begin_ != -1 && end_ == -1)  {
        length -= begin_;
    }
    buffer.append("Content-length: " + std::to_string(length) + util::ENDLINE_FLAG + util::ENDLINE_FLAG);
}

void HttpResponse::errorContent(Buffer &buffer, std::string msg) {
    std::string status;
    std::string body = "<html><title>Error</title><body bgcolor=\"ffffff\">";
    auto result = CODE_STATUS.find(code_);
    if (result != CODE_STATUS.end()) {
        status = result->second;
    } else {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    body += std::to_string(code_) + " : " + status + "\n";
    body += "<p>" + msg + "</p>";
    body += "<hr><em>By My HTTP Server</em></body></html>";
    buffer.append("Content-length: " + std::to_string(body.size()) + util::ENDLINE_FLAG + util::ENDLINE_FLAG)
        .append(body);
}

std::string HttpResponse::getFileType() {
    auto idx = path_.find_last_of('.');
    if (idx == std::string::npos) {
        return "text/plain";
    }
    auto suffix = path_.substr(idx);
    auto result = SUFFIX_TYPE.find(suffix);
    if (result != SUFFIX_TYPE.end()) {
        return result->second;
    }
    return "text/plain";
}