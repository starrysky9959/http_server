/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-19 19:41:36
 * @LastEditors: starrysky9959 965105951@qq.com
 * @LastEditTime: 2022-11-05 01:02:10
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

/*
 * HTTP响应也由四个部分组成, 分别是: 状态行, 消息报头, 空行和响应正文
*/
class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();
    void init(const std::string &srcDir, std::string &path, std::unordered_map<std::string, std::string> &header, bool isKeepAlive, int code);

    void makeResponse(Buffer &buffer, bool needRedirect);

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

    std::string getFileType();

    void errorContent(Buffer &buffer, std::string msg);

    std::string path_;
    std::unordered_map<std::string, std::string> header_;
    std::string srcDir_;
    int code_;
    bool isKeepAlive_;

    // Linux文件的内存共享映射
    char *mmFile_;
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    // static const std::unordered_map<int, std::string> ERROR_PAGES;
};
