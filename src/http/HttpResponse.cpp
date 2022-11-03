#include "HttpResponse.h"
#include <cassert>
#include <iostream>
#include "../util/Util.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/nsword"},
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
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

const std::unordered_map<int, std::string> HttpResponse::ERROR_PAGES = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
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

void HttpResponse::Init(const std::string &srcDir, std::string &path, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if (mmFile_) unmapFile();
    this->srcDir_ = srcDir;
    this->path_ = path;
    this->code_ = code;
    this->isKeepAlive_ = isKeepAlive;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
    
}

void HttpResponse::makeResponse(Buffer &buffer) {
    // check whether the file exists
    // function stat() save file infomation in the specified stat according to the file name
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    } // file authority
    else if (!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;
    } else if (code_==-1){ // OK
        code_ = 200;
    }

    addStateLine(buffer);
    addHeader(buffer);
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
    if (*mmRet == -1) {
        errorContent(buffer, "File not found!");
        return;
    }
    mmFile_ = (char *)mmRet;
    close(srcFd);
    buffer.append("Content-length: " + std::to_string(mmFileStat_.st_size) + util::ENDLINE_FLAG + util::ENDLINE_FLAG);
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