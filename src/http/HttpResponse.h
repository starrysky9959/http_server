/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-19 19:41:36
 * @LastEditors: starrysky9959 starrysky9651@outlook.com
 * @LastEditTime: 2023-01-30 15:01:00
 * @Description:  
 */
#pragma once

#include <cstddef>
#include <string>
#include <fcntl.h>    // open
#include <unistd.h>   // close
#include <sys/stat.h> // stat
#include <sys/mman.h> // mmap, munmap
#include <unordered_map>
#include "../buffer/Buffer.h"
#include "HttpRequest.h"

/*
 * HTTP响应也由四个部分组成, 分别是: 状态行, 消息报头, 空行和响应正文
*/
class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();
    void init(const std::string &srcDir,HttpRequest &request,bool isKeepAlive, int code);

    void makeResponse(Buffer &buffer, bool needRedirect=false);

    char *getFile();
    size_t getFileLength();
    void unmapFile();

    // handle range
    int begin_;
    int end_;

private:
    void addStateLine(Buffer &buffer);
    void addHeader(Buffer &buffer);
    void addContent(Buffer &buffer);
    void errorContent(Buffer &buffer, std::string msg);
    void postContent(Buffer &buffer);

    std::string getFileType();

    std::string srcDir_;
    std::string path_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> postParameter_;
    bool isKeepAlive_;
    int code_;
    int length_;

    // Linux文件的内存共享映射
    char *mmFile_;
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    // static const std::unordered_map<int, std::string> ERROR_PAGES;
};
